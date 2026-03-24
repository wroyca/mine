#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>

// #include <cpptrace/cpptrace.hpp>

#include <mine/mine-editor-core.hxx>
#include <mine/mine-terminal-raw.hxx>
#include <mine/mine-terminal-render.hxx>
#include <mine/mine-async-loop.hxx>
#include <mine/mine-async-input.hxx>
#include <mine/mine-assert.hxx>
#include <mine/mine-window.hxx>

// OpenGL GUI dependencies.
//
#include <mine/mine-window-opengl.hxx>
#include <mine/mine-window-render.hxx>
#include <mine/mine-window-input.hxx>

#include <mine/mine-utility.hxx>

using namespace std;
using namespace std::filesystem;

namespace mine
{
  // Signal Handling
  //
  // This is the messy interface between the clean C++ world and the raw OS
  // signals. We need to catch:
  //
  // 1. SIGINT/SIGTERM: To exit gracefully (restore terminal mode).
  // 2. SIGABRT: To print a stack trace when we crash (via assert or abort).
  // 3. SIGWINCH: Handled separately via Boost.Asio (see app class).
  //
  namespace
  {
    volatile sig_atomic_t g_sig (0);

    // Global pointer to the raw mode RAII guard.
    //
    // This is a necessary evil. If we crash or get killed by a signal, the
    // RAII destructor for `terminal_raw_mode` won't run naturally. We need
    // this hook to manually restore the terminal state (uncooked -> cooked)
    // inside the signal handler; otherwise, the user's shell is left in a
    // broken state (no echo, no cursor).
    //
    terminal_raw_mode* g_raw (nullptr);

    extern "C" void
    signal_handler (int s)
    {
      g_sig = s;
    }

    extern "C" void
    abort_handler (int s)
    {
      // The "Last Gasp".
      //
      // If we are here, the ship is sinking. We try to restore the terminal
      // first so the error message is actually readable.
      //
      if (g_raw != nullptr)
      {
        try
        {
          // Explicit destructor call. This restores the termios settings.
          //
          // Is this async-signal-safe? Technically no. `tcsetattr` is, but
          // the C++ destructor might do other things. However, we are about
          // to abort anyway, so the risk of deadlock is irrelevant compared
          // to the benefit of a clean terminal.
          //
          g_raw->~terminal_raw_mode ();
          g_raw = nullptr;
        }
        catch (...)
        {
          // Swallow it. We have bigger problems.
          //
        }
      }

      // Print the trace.
      //
      // `cpptrace` is definitely not async-signal-safe (it allocates memory,
      // reads DWARF info, etc.). But again, better a stack trace that works
      // 99% of the time than a silent `Aborted`.
      //
      cerr << "\nCAUGHT SIGABRT (" << s << ")\n"
           << "Stack trace at abort:\n";

      // cpptrace::generate_trace ().print ();
      cerr << endl;

      // Unhook ourselves and re-raise to let the OS generate the core dump
      // and proper exit code.
      //
      signal (s, SIG_DFL);
      raise (s);
    }
  }

  // The Application Context (Terminal)
  //
  class terminal_editor_app
  {
  public:
    terminal_editor_app ()
    {
      // Hook the global for emergency cleanup.
      //
      g_raw = &raw_;
      init ();
    }

    explicit
    terminal_editor_app (string f)
      : file_ (std::move(f))
    {
      g_raw = &raw_;
      init ();
    }

    ~terminal_editor_app ()
    {
      g_raw = nullptr;
    }

    void
    run ()
    {
      // Install C-style handlers for the "hard" signals.
      //
      signal (SIGINT, signal_handler);
      signal (SIGTERM, signal_handler);
      signal (SIGABRT, abort_handler);

      // Clear the screen to remove any terminal artifacts before we start
      // rendering. This is the same escape sequence we use on resize.
      //
      cout << "\x1b[2J\x1b[H" << flush;

      // Kick off the reactor.
      //
      input_->start ();
      ren_->force_redraw (core_.current ());

      // The Main Loop.
      //
      // We rely on `boost::asio::io_context` to drive our async logic.
      //
      while (!g_sig && !quit_)
      {
        // Strategy: Block, then Drain.
        //
        // 1. `run_one()` blocks until *at least one* event arrives (input,
        //     timer, signal). This keeps CPU usage near 0% when idle.
        //
        if (loop_.context ().run_one () > 0)
        {
          // 2. `poll()` executes any *other* events that are already ready.
          //
          // Why? If the user pasted 1KB of text, `run_one` processes the
          // first byte. We don't want to render the screen after every single
          // byte. By polling, we drain the input queue, batching the logic
          // updates, and only render once the queue is empty.
          //
          loop_.context ().poll ();
        }
        else
        {
          // If run_one() returns 0, the context is "stopped" (out of work). We
          // must restart it to keep waiting.
          //
          if (loop_.context ().stopped ())
            loop_.context ().restart ();
        }
      }

      // Shutdown sequence.
      //
      input_->stop ();
#ifndef _WIN32
      if (winch_)
        winch_->cancel ();
#else
      if (winch_timer_)
        winch_timer_->cancel ();
#endif
    }

  private:
    void
    init ()
    {
      // Detect Environment.
      //
      auto sz (get_terminal_size ());
      if (!sz)
      {
        cerr << "error: failed to detect terminal size" << endl;
        exit (1);
      }

      // Bootstrap Core.
      //
      // We initialize the view immediately with the correct size. If we
      // defaulted to 80x24 and then resized, the user might see a single frame
      // of wrong layout flickering at startup.
      //
      auto s (core_.current ());
      auto v (s.view ().resize (*sz));

      core_ = editor_core (loop_, s.with_view (v));
      ren_ = make_unique<terminal_renderer> (*sz);

      // Wire up Logic -> UI.
      //
      core_.on_change (
        [this] (const editor_state& st, state_change_type t)
        {
          if (t == state_change_type::cursor)
            ren_->render_cursor_only (st);
          else
            ren_->render (st);
        });

      core_.on_message (
        [this] (const string& m)
        {
          // TODO: This should eventually feed into a status bar component.
          //
          last_msg_ = m;
        });

      // Wire up Input -> Logic.
      //
      input_ = make_unique<async_input_handler> (
        loop_,
        [this] (const input_event& e) { handle_input (e); });

      // Setup Resize Handler.
      //
      // We use `signal_set` for SIGWINCH because it integrates cleanly with
      // the ASIO reactor, unlike the raw C handlers.
      //
#ifndef _WIN32
      winch_ = make_unique<boost::asio::signal_set> (loop_.context (),
                                                     SIGWINCH);
      handle_resize ();
#else
      // Windows doesn't use SIGWINCH. We poll periodically instead.
      //
      winch_timer_ = make_unique<boost::asio::steady_timer> (loop_.context ());
      last_term_sz_ = get_terminal_size ();
      handle_resize ();
#endif

      // Load Initial File.
      //
      if (!file_.empty ())
        core_.load (file_);
    }

    void
    handle_resize ()
    {
#ifndef _WIN32
      winch_->async_wait ([this] (const boost::system::error_code& ec, int)
      {
        if (!ec)
        {
          // Re-arm immediately.
          //
          handle_resize ();

          // The Resize Dance.
          //
          // When the terminal window resizes, the text reflows chaotically.
          // We clear the screen immediately to remove artifacts, then query
          // the new dimensions and tell the engine to rebuild the world.
          //
          cout << "\x1b[2J\x1b[H" << flush;

          auto sz (get_terminal_size ());
          if (sz)
          {
            core_.resize (*sz);
            ren_->resize (*sz);
            ren_->force_redraw (core_.current ());
          }
        }
      });
#else
      if (!winch_timer_)
        return;

      winch_timer_->expires_after (std::chrono::milliseconds (250));
      winch_timer_->async_wait ([this] (const boost::system::error_code& e)
      {
        if (!e)
        {
          // Re-arm immediately to catch any subsequent signals in the flurry.
          //
          handle_resize ();

          auto s (get_terminal_size ());
          if (s)
          {
            // Figure out if the dimensions actually changed.
            //
            bool c (!last_term_sz_);

            if (c)
            {
              last_term_sz_ = s;

              // Handle the resize dance.
              //
              cout << "\x1b[2J\x1b[H" << flush;

              core_.resize (*s);
              ren_->resize (*s);
              ren_->force_redraw (core_.current ());
            }
          }
        }
      });
#endif
    }

    void
    handle_input (const input_event& e)
    {
      // Delegate everything else to the editor logic.
      //
      core_.handle_input (e);
    }

    // Infrastructure.
    //
    async_loop  loop_;
    editor_core core_ {loop_};

    // RAII guard for raw mode.
    //
    // MUST be declared before UI components so it destructs *last* (restoring
    // the terminal only after we stop printing).
    //
    terminal_raw_mode raw_;

    unique_ptr<terminal_renderer>       ren_;
    unique_ptr<async_input_handler>     input_;

#ifndef _WIN32
    unique_ptr<boost::asio::signal_set> winch_;
#else
    unique_ptr<boost::asio::steady_timer> winch_timer_;
    decltype(get_terminal_size()) last_term_sz_;
#endif

    string file_;
    string last_msg_;
    bool   quit_ {false};
  };

  // The GUI Application Context (OpenGL)
  //
  class window_editor_app
  {
  public:
    window_editor_app ()
      : win_ (1024, 768, "mine"),
        gl_ (),
        ren_ ()
    {
      init ();
    }

    explicit window_editor_app (string f)
      : win_ (1024, 768, "mine"),
        gl_ (),
        ren_ (),
        file_ (std::move (f))
    {
      init ();
    }

    void
    run ()
    {
      auto last (std::chrono::steady_clock::now ());

      // The GUI Event Loop.
      //
      // Unlike the terminal backend which blocks cleanly on STDIN using
      // Asio, GLFW fundamentally expects to own the main thread's event
      // pump. We must interleave Asio's poll to process background IO.
      //
      while (!win_.closing () && !quit_)
      {
        win_.update ();

        auto now (std::chrono::steady_clock::now ());
        float dt (std::chrono::duration<float> (now - last).count ());
        last = now;

        if (dt > 0.1f)
          dt = 0.1f;

        auto sz (win_.framebuffer_size ());

        if (sz.first != last_w_ || sz.second != last_h_)
        {
          ren_.resize (sz.first, sz.second);

          screen_size v_sz (static_cast<uint16_t> (sz.second / 16),
                            static_cast<uint16_t> (sz.first / 8));

          core_.resize (v_sz);

          last_w_ = sz.first;
          last_h_ = sz.second;
          dirty_ = true;
        }

        // Drain any pending asynchronous operations (file IO, etc).
        //
        // We use poll() rather than run() so we do not block the thread
        // if the ASIO queue is empty.
        //
        loop_.context ().poll ();

        if (loop_.context ().stopped ())
          loop_.context ().restart ();

        ren_.update (dt);

        if (ren_.is_animating ())
          dirty_ = true;

        if (dirty_)
        {
          ren_.render (core_.current (), track_);
          track_ = false;

          win_.swap_buffers ();
          dirty_ = false;
        }
        else
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (8));
        }
      }
    }

  private:
    void
    init ()
    {
      cerr << "info: opengl version " << gl_.version () << "\n"
           << "info: opengl renderer " << gl_.renderer () << "\n"
           << "info: opengl vendor " << gl_.vendor () << endl;

      path font_path = build_install_data / "fonts" / "regular.ttf";

      if (!ren_.load_font (font_path.c_str (), 28))
      {
        cerr << "warning: failed to load default font. text rendering disabled." << endl;
      }

      // Bootstrap Core.
      //
      // We fake a standard terminal grid size so the core doesn't crash
      // trying to resize a zero-bound view before the first real frame resize.
      //
      screen_size sz (24, 80);

      auto s (core_.current ());
      auto v (s.view ().resize (sz));

      core_ = editor_core (loop_, s.with_view (v));

      // Wire up Logic -> UI.
      //
      core_.on_change (
        [this] (const editor_state& /*st*/, state_change_type /*t*/)
      {
        // Flag for redraw on the next frame.
        //
        dirty_ = true;
        track_ = true;
      });

      core_.on_message ([this] (const string& m)
      {
        // TODO: Route to GUI status bar.
        //
        (void) m;
      });

      // Input wiring.
      //
      // Notice how the continuous scroll callback is cleanly separated from
      // the variant-based input_event callback. We pump raw doubles into the
      // renderer without polluting the discrete terminal inputs.
      //
      input_ = make_unique<window_input> (
        win_.handle (),
        [this] (const input_event& e) { handle_input (e); },
        [this] (double x, double y)
        {
          ren_.scroll (static_cast<float> (x), static_cast<float> (y));
          dirty_ = true;
        },
        [this] (double x, double y, mouse_state st, key_modifier mod)
        {
          screen_position sp (ren_.screen_to_grid (static_cast<float> (x),
                                                   static_cast<float> (y),
                                                   core_.current ()));

          mouse_event me {sp.col,
                          sp.row,
                          static_cast<mouse_button> (0),
                          mod,
                          st};

          core_.handle_input (me);

          track_ = true;
          dirty_ = true;
        }
      );

      // Load Initial File.
      //
      if (!file_.empty ())
        core_.load (file_);
    }

    void
    handle_input (const input_event& e)
    {
      // Delegate everything else to the editor logic.
      //
      core_.handle_input (e);
    }

    async_loop  loop_;
    editor_core core_ {loop_};

    window          win_;
    opengl_context  gl_;
    window_renderer ren_;

    unique_ptr<window_input> input_;

    string file_;

    int  last_w_ {0};
    int  last_h_ {0};
    bool quit_   {false};
    bool dirty_  {true};
    bool track_  {true};
  };
}

int
main (int argc, char* argv[])
{
  try
  {
    bool gui (false);
    std::string f;

    // A very simple argument parser. We look for a --gui switch, and assume
    // the first non-switch argument is the filename.
    //
    for (int i (1); i < argc; ++i)
    {
      std::string a (argv[i]);
      if (a == "--gui")
        gui = true;
      else if (f.empty ())
        f = a;
    }

    if (gui)
    {
      if (!f.empty ())
      {
        mine::window_editor_app app (f);
        app.run ();
      }
      else
      {
        mine::window_editor_app app;
        app.run ();
      }
    }
    else
    {
      if (!f.empty ())
      {
        mine::terminal_editor_app app (f);
        app.run ();
      }
      else
      {
        mine::terminal_editor_app app;
        app.run ();
      }
    }

    return 0;
  }
  catch (const std::exception& e)
  {
    std::cerr << "Abandon all hope, ye who enter here: " << e.what () << std::endl;
    return 1;
  }
}

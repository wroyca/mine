#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <boost/asio/signal_set.hpp>

// #include <cpptrace/cpptrace.hpp>

#include <mine/mine-assert.hxx>
#include <mine/mine-async-input.hxx>
#include <mine/mine-async-loop.hxx>
#include <mine/mine-editor-core.hxx>
#include <mine/mine-terminal-raw.hxx>
#include <mine/mine-terminal-render.hxx>
#include <mine/mine-window-opengl.hxx>
#include <mine/mine-window-render.hxx>
#include <mine/mine-window.hxx>

using namespace std;

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

  // The Application Context
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
      if (winch_)
        winch_->cancel ();
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
      winch_ = make_unique<boost::asio::signal_set> (loop_.context (),
                                                     SIGWINCH);
      handle_resize ();

      // Load Initial File.
      //
      if (!file_.empty ())
        core_.load (file_);
    }

    void
    handle_resize ()
    {
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
    }

    void
    handle_input (const input_event& e)
    {
      // App-level shortcuts (Quit, Save).
      //
      // These bypass the editor core because they affect the lifecycle of
      // the application itself or IO, not just buffer state.
      //
      if (const auto* k = get_if<text_input_event> (&e))
      {
        // Ctrl+Q -> Quit
        //
        if (k->text == "q" && has_modifier (k->modifiers, key_modifier::ctrl))
        {
          quit_ = true;
          return;
        }

        // Ctrl+S -> Save
        //
        if (k->text == "s" && has_modifier (k->modifiers, key_modifier::ctrl))
        {
          core_.save ();
          return;
        }
      }

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
    unique_ptr<boost::asio::signal_set> winch_;

    string file_;
    string last_msg_;
    bool   quit_ = false;
  };

  // The GUI Application Context
  //
  class window_editor_app
  {
  public:
    window_editor_app ()
      : win_ (1024, 768, "mine"),
        gl_ (),
        quit_ (false),
        dirty_ (true)
    {
      init ();
    }

    explicit window_editor_app (string f)
      : win_ (1024, 768, "mine"),
        gl_ (),
        file_ (std::move (f)),
        quit_ (false),
        dirty_ (true)
    {
      init ();
    }

    void
    run ()
    {
      // The GUI Event Loop.
      //
      // Unlike the terminal backend which blocks cleanly on STDIN using
      // Asio, GLFW fundamentally expects to own the main thread's event
      // pump. We must interleave Asio's poll to process background IO.
      //
      while (!win_.closing () && !quit_)
      {
        win_.update ();

        // Drain any pending asynchronous operations (file IO, etc).
        //
        // We use poll() rather than run() so we do not block the thread
        // if the ASIO queue is empty.
        //
        loop_.context ().poll ();

        if (loop_.context ().stopped ())
          loop_.context ().restart ();

        // TODO: Actually render the OpenGL quad here.
        //
        if (dirty_)
        {
          ren_.render (core_.current ());
          win_.swap_buffers ();

          dirty_ = false;
        }
      }
    }

  private:
    void
    init ()
    {
      // Bootstrap Core.
      //
      // Since we lack an OpenGL font renderer right now, we just fake a
      // standard terminal grid size so the core doesn't crash trying to
      // resize a zero-bound view.
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
      });

      core_.on_message ([this] (const string& m)
      {
        // TODO: Route to GUI status bar.
        //
        (void) m;
      });

      // Load Initial File.
      //
      if (!file_.empty ())
        core_.load (file_);
    }

    async_loop loop_;
    editor_core core_ {loop_};
    window win_;
    opengl_context gl_;
    window_renderer ren_;

    string file_;
    bool quit_;
    bool dirty_;
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

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#if defined(_WIN32)
#  include <boost/asio/windows/stream_handle.hpp>
#  include <windows.h>
#else
#  include <boost/asio/posix/stream_descriptor.hpp>
#  include <unistd.h>
#endif

#include <mine/mine-async-load-document.hxx>
#include <mine/mine-async-read-input.hxx>
#include <mine/mine-async-runtime.hxx>
#include <mine/mine-async-save-document.hxx>
#include <mine/mine-editor.hxx>
#include <mine/mine-terminal.hxx>
#include <mine/mine-window.hxx>

// OpenGL GUI dependencies.
//
#include <mine/mine-window-input.hxx>
#include <mine/mine-window-opengl.hxx>
#include <mine/mine-window-render.hxx>

#include <mine/mine-utility.hxx>

using namespace std;
using namespace filesystem;
namespace asio = boost::asio;

namespace mine
{
  // Signal handling.
  //
  // This is the messy interface between the clean C++ world and the raw OS
  // signals. We need to catch:
  //
  // 1. SIGABRT: To restore the terminal and print a stack trace when we crash.
  // 2. SIGINT/SIGTERM/SIGWINCH: Handled via Boost.Asio signal_set coroutines.
  //
  namespace
  {
    terminal_raw_mode* g_raw (nullptr);

    extern "C" void
    abort_handler (int s)
    {
      if (g_raw != nullptr)
      {
        try
        {
          g_raw->~terminal_raw_mode ();
          g_raw = nullptr;
        }
        catch (...)
        {
          // We are already aborting, so just swallow any secondary exceptions
          // here to avoid recursive termination faults.
          //
        }
      }

      cerr << "\nCAUGHT SIGABRT (" << s << ")\n"
           << "Stack trace at abort:\n";

      cerr << endl;

      signal (s, SIG_DFL);
      raise (s);
    }
  }

  // Terminal backend.
  //

  class terminal_editor_app
  {
  public:
    terminal_editor_app ()
      : input_chan_ (runtime_.main_context (), 256),
#if defined(_WIN32)
        stdin_stream_ (runtime_.input_context ())
#else
        stdin_stream_ (runtime_.input_context ())
#endif
    {
      g_raw = &raw_;

#if defined(_WIN32)
      stdin_stream_.assign (::GetStdHandle (STD_INPUT_HANDLE));
#else
      // Note that we set standard input to non-blocking here.
      //
      stdin_stream_.assign (STDIN_FILENO);
      stdin_stream_.non_blocking (true);
#endif

      init ();
    }

    explicit terminal_editor_app (string f)
      : input_chan_ (runtime_.main_context (), 256),
#if defined(_WIN32)
        stdin_stream_ (runtime_.input_context ()),
#else
        stdin_stream_ (runtime_.input_context ()),
#endif
        file_ (move (f))
    {
      g_raw = &raw_;

#if defined(_WIN32)
      stdin_stream_.assign (::GetStdHandle (STD_INPUT_HANDLE));
#else
      stdin_stream_.assign (STDIN_FILENO);
      stdin_stream_.non_blocking (true);
#endif

      init ();
    }

    ~terminal_editor_app ()
    {
      g_raw = nullptr;
    }

    void
    run ()
    {
      // Install our C-style abort handler so we can restore the terminal
      // gracefully if the application crashes.
      //
      signal (SIGABRT, abort_handler);

      // Clear the screen to remove any residual shell artifacts.
      //
      cout << "\x1b[2J\x1b[H" << flush;
      ren_->force_redraw (core_.current ());

      // Spawn the input reader. We pin this to a dedicated input thread so
      // heavy file I/O or rendering doesn't block keystrokes.
      //
      asio::co_spawn (runtime_.input_executor (),
                      async_read_input (stdin_stream_, input_chan_),
                      asio::detached);

      // Spawn the main event processing coroutine.
      //
      asio::co_spawn (runtime_.main_executor (),
                      async_process_events (),
                      asio::detached);

      // Spawn signal handlers as coroutines on the main executor.
      //
      asio::co_spawn (runtime_.main_executor (),
                      async_handle_signals (),
                      asio::detached);

#ifndef _WIN32
      asio::co_spawn (runtime_.main_executor (),
                      async_handle_resize (),
                      asio::detached);
#else
      asio::co_spawn (runtime_.main_executor (),
                      async_poll_resize (),
                      asio::detached);
#endif

      // Load the initial file if one was specified on the command line.
      //
      if (!file_.empty ())
      {
        asio::co_spawn (runtime_.main_executor (),
                        async_open_file (file_),
                        asio::detached);
      }

      // Block on the main context until we are asked to shut down.
      //
      runtime_.run ();
    }

  private:
    void
    init ()
    {
      auto sz (get_terminal_size ());
      if (!sz)
      {
        // If we cannot detect the terminal size, there is no sensible way to
        // render the interface. Bail out early.
        //
        cerr << "error: failed to detect terminal size" << endl;
        exit (1);
      }

      core_.resize (*sz);
      ren_ = make_unique<terminal_renderer> (*sz);

      // Wire up the logic to the user interface.
      //
      core_.on_change ([this] (const workspace& st, change_hint_type t)
      {
        if (t == change_hint_type::cursor)
          ren_->render_cursor_only (st);
        else
          ren_->render (st);
      });

      core_.on_message ([this] (const string& m)
      {
        last_msg_ = m;
      });

      // Wire up the asynchronous save callback.
      //
      core_.on_save ([this] (document_id id, string path, content text)
      {
        asio::co_spawn (runtime_.main_executor (),
                        async_save_and_notify (id, move (path), move (text)),
                        asio::detached);
      });

      core_.load_config ();
    }

    // Read events from the channel and feed them to the editor.
    //
    // The channel is populated by async_read_input running on the dedicated
    // input thread. By pulling from it here inside a coroutine on the main
    // executor, all editor mutations naturally occur on a single thread.
    //
    asio::awaitable<void>
    async_process_events ()
    {
      for (;;)
      {
        auto [ec, event](co_await input_chan_.async_receive (
          asio::as_tuple (asio::use_awaitable)));

        if (ec)
          break;

        core_.handle_input (event);

        if (core_.quit_requested ())
        {
          runtime_.shutdown ();
          co_return;
        }
      }
    }

    // Wait for SIGINT or SIGTERM and trigger a graceful shutdown.
    //
    asio::awaitable<void>
    async_handle_signals ()
    {
      asio::signal_set sigs (co_await asio::this_coro::executor,
                             SIGINT,
                             SIGTERM);

      auto [ec, sig](
        co_await sigs.async_wait (asio::as_tuple (asio::use_awaitable)));

      if (!ec)
        runtime_.shutdown ();
    }

#ifndef _WIN32
    // Wait for SIGWINCH (terminal resize) and update the editor geometry.
    //
    // On Unix systems, we are notified of window size changes via a signal.
    // We catch it and adjust our internal buffers.
    //
    asio::awaitable<void>
    async_handle_resize ()
    {
      asio::signal_set sigs (co_await asio::this_coro::executor, SIGWINCH);

      for (;;)
      {
        auto [ec, sig](
          co_await sigs.async_wait (asio::as_tuple (asio::use_awaitable)));

        if (ec)
          co_return;

        cout << "\x1b[2J\x1b[H" << flush;

        auto sz (get_terminal_size ());
        if (sz)
        {
          core_.resize (*sz);
          ren_->resize (*sz);
          ren_->force_redraw (core_.current ());
        }
      }
    }
#else
    // Windows fallback: poll terminal size periodically.
    //
    // Windows lacks a clean equivalent to SIGWINCH that easily integrates
    // into ASIO, so we resort to a timer-based polling approach.
    //
    asio::awaitable<void>
    async_poll_resize ()
    {
      asio::steady_timer timer (co_await asio::this_coro::executor);
      auto last_sz (get_terminal_size ());

      for (;;)
      {
        timer.expires_after (chrono::milliseconds (250));

        auto [ec](
          co_await timer.async_wait (asio::as_tuple (asio::use_awaitable)));

        if (ec)
          co_return;

        auto sz (get_terminal_size ());
        if (sz && (!last_sz || *last_sz != *sz))
        {
          last_sz = sz;

          cout << "\x1b[2J\x1b[H" << flush;

          core_.resize (*sz);
          ren_->resize (*sz);
          ren_->force_redraw (core_.current ());
        }
      }
    }
#endif

    asio::awaitable<void>
    async_open_file (string path)
    {
      try
      {
        auto text (co_await async_load_document (path));
        core_.open_document (path, move (text));
      }
      catch (const exception& e)
      {
        core_.show_message ("Load error: " + string (e.what ()));
      }
    }

    asio::awaitable<void>
    async_save_and_notify (document_id id, string path, content text)
    {
      try
      {
        co_await async_save_document (path, text);
        core_.mark_saved (id);
        core_.show_message ("Written: " + path);
      }
      catch (const exception& e)
      {
        core_.show_message ("Save error: " + string (e.what ()));
      }
    }

    async_runtime runtime_;
    editor core_;

    // Keep this declared before UI components so it restores the terminal
    // state as the very last step during destruction.
    //
    terminal_raw_mode raw_;

    unique_ptr<terminal_renderer> ren_;

    input_channel input_chan_;

#if defined(_WIN32)
    asio::windows::stream_handle stdin_stream_;
#else
    asio::posix::stream_descriptor stdin_stream_;
#endif

    string file_;
    string last_msg_;
  };

  // GUI backend (OpenGL).
  //

  class window_editor_app
  {
  public:
    window_editor_app ()
      : win_ (1024, 768, "mine"),
        gl_ (),
        ren_ (),
        last_w_ (0),
        last_h_ (0),
        dirty_ (true),
        track_ (true)
    {
      init ();
    }

    explicit window_editor_app (string f)
      : win_ (1024, 768, "mine"),
        gl_ (),
        ren_ (),
        file_ (move (f)),
        last_w_ (0),
        last_h_ (0),
        dirty_ (true),
        track_ (true)
    {
      init ();
    }

    void
    run ()
    {
      asio::co_spawn (runtime_.main_executor (),
                      async_frame_loop (),
                      asio::detached);

      if (!file_.empty ())
      {
        asio::co_spawn (runtime_.main_executor (),
                        async_open_file (file_),
                        asio::detached);
      }

      runtime_.run ();
    }

  private:
    void
    init ()
    {
      cerr << "info: opengl version " << gl_.version () << "\n"
           << "info: opengl renderer " << gl_.renderer () << "\n"
           << "info: opengl vendor " << gl_.vendor () << endl;

      path font_path (build_install_data / "fonts" / "regular.ttf");

      if (!ren_.load_font (font_path.string ().c_str (), 28))
      {
        // It is not fatal if we fail to load the font, but text rendering
        // will obviously be broken. We should notify the user.
        //
        cerr << "warning: failed to load default font. text rendering disabled."
             << endl;
      }

      screen_size sz (24, 80);
      core_.resize (sz);

      core_.on_change ([this] (const workspace& /*st*/, change_hint_type /*t*/)
      {
        dirty_ = true;
        track_ = true;
      });

      core_.on_message ([this] (const string& m)
      {
        (void) m;
      });

      core_.on_save ([this] (document_id id, string path, content text)
      {
        asio::co_spawn (runtime_.main_executor (),
                        async_save_and_notify (id, move (path), move (text)),
                        asio::detached);
      });

      core_.load_config ();

      input_ = make_unique<window_input> (
        win_.handle (),
        [this] (const input_event& e)
      {
        core_.handle_input (e);
      },
        [this] (double x, double y)
      {
        ren_.scroll (static_cast<float> (x),
                     static_cast<float> (y),
                     core_.current ());
        dirty_ = true;
      },
        [this] (double x, double y, mouse_state st, key_modifier mod)
      {
        screen_position sp (ren_.screen_to_grid (static_cast<float> (x),
                                                 static_cast<float> (y),
                                                 core_.current ()));

        mouse_event me (sp.col, sp.row, static_cast<mouse_button> (0), mod, st);

        core_.handle_input (me);

        track_ = true;
        dirty_ = true;
      });
    }

    // Drive the GUI frame loop via an ASIO steady timer.
    //
    // The co_await on the timer gracefully yields control to ASIO between
    // frames.
    //
    asio::awaitable<void>
    async_frame_loop ()
    {
      asio::steady_timer frame_timer (co_await asio::this_coro::executor);
      auto last (chrono::steady_clock::now ());

      while (!win_.closing () && !core_.quit_requested ())
      {
        win_.update ();

        auto now (chrono::steady_clock::now ());
        float dt (chrono::duration<float> (now - last).count ());
        last = now;

        // Cap the delta time to prevent massive jumps in the simulation
        // if the thread stalls for a moment.
        //
        if (dt > 0.1f)
          dt = 0.1f;

        auto sz (win_.framebuffer_size ());

        if (sz.first != last_w_ || sz.second != last_h_)
        {
          ren_.resize (sz.first, sz.second);

          float lh (ren_.line_height ());
          if (lh <= 0.0f)
            lh = 16.0f;

          screen_size v_sz (static_cast<uint16_t> (sz.second / lh),
                            static_cast<uint16_t> (sz.first / (lh * 0.5f)));

          core_.resize (v_sz);

          last_w_ = sz.first;
          last_h_ = sz.second;
          dirty_ = true;
        }

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

        frame_timer.expires_after (chrono::milliseconds (1));
        co_await frame_timer.async_wait (asio::use_awaitable);
      }

      runtime_.shutdown ();
    }

    asio::awaitable<void>
    async_open_file (string path)
    {
      try
      {
        auto text (co_await async_load_document (path));
        core_.open_document (path, move (text));
      }
      catch (const exception& e)
      {
        core_.show_message ("Load error: " + string (e.what ()));
      }
    }

    asio::awaitable<void>
    async_save_and_notify (document_id id, string path, content text)
    {
      try
      {
        co_await async_save_document (path, text);
        core_.mark_saved (id);
        core_.show_message ("Written: " + path);
      }
      catch (const exception& e)
      {
        core_.show_message ("Save error: " + string (e.what ()));
      }
    }

    async_runtime runtime_;
    editor core_;

    window win_;
    opengl_context gl_;
    window_renderer ren_;

    unique_ptr<window_input> input_;

    string file_;

    int last_w_;
    int last_h_;
    bool dirty_;
    bool track_;
  };
}

int
main (int argc, char* argv[])
{
  try
  {
    bool gui (false);
    string f;

    // Parse command line arguments. We simply look for the --gui flag and
    // assume the first positional argument is the target file to open.
    //
    for (int i (1); i < argc; ++i)
    {
      string a (argv[i]);
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
  catch (const exception& e)
  {
    // If an exception makes it all the way out here, something went wrong with
    // the underlying runtime.
    //
    cerr << "Abandon all hope, ye who enter here: " << e.what () << endl;
    return 1;
  }
}

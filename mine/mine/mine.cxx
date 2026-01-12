#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <chrono>

#include <boost/asio/signal_set.hpp>

#include <cpptrace/cpptrace.hpp>

#include <mine/mine-editor-core.hxx>
#include <mine/mine-terminal-raw.hxx>
#include <mine/mine-terminal-render.hxx>
#include <mine/mine-async-loop.hxx>
#include <mine/mine-async-input.hxx>
#include <mine/mine-assert.hxx>

using namespace std;

namespace mine
{
  // Signal handling.
  //
  // We need to catch SIGINT (Ctrl+C) and SIGTERM to exit gracefully, restoring
  // the terminal state. For SIGABRT, we want a stack trace before dying.
  //
  namespace
  {
    volatile sig_atomic_t g_signal_received (0);

    extern "C" void
    signal_handler (int s)
    {
      g_signal_received = s;
    }

    extern "C" void
    abort_handler (int s)
    {
      // We are crashing. Try to print something useful.
      //
      // Note: cpptrace might allocate memory or do other non-async-signal-safe
      // things. Technically this is undefined behavior inside a signal handler,
      // but since we are aborting anyway, it's worth the risk for debugging
      // info.
      //
      cerr << "\n=== CAUGHT SIGABRT (" << s << ") ===\n"
                << "Stack trace at abort:\n";

      cpptrace::generate_trace ().print ();

      cerr << endl;

      // Reset to default handler and re-raise to actually terminate and dump
      // core.
      //
      signal (s, SIG_DFL);
      raise (s);
    }
  }

  // The application context.
  //
  class terminal_editor_app
  {
  public:
    terminal_editor_app ()
    {
      init ();
    }

    explicit terminal_editor_app (const string& fn)
        : initial_file_ (fn)
    {
      init ();
    }

    void
    run ()
    {
      // Install handlers.
      //
      signal (SIGINT, signal_handler);
      signal (SIGTERM, signal_handler);
      signal (SIGABRT, abort_handler);

      // Start the reactor.
      //
      input_handler_->start ();
      renderer_->force_redraw (core_.current ());

      // The Main Loop.
      //
      // We drive the boost::asio::io_context (wrapped in async_loop).
      //
      while (!g_signal_received && !should_quit_)
      {
        // If the context ran out of work (empty queue), it stops. We must
        // restart it to keep waiting for input.
        //
        // Block until at least one handler runs.
        //
        // If we used poll() here, we'd burn 100% CPU. run_one() blocks
        // efficiently.
        //
        if (loop_.context ().run_one () > 0)
        {
          // If we processed an event, drain any other ready events immediately
          // to batch updates (e.g., if the user pasted a block of text).
          //
          loop_.context ().poll ();
        }
      }

      // If we are here, we are shutting down.
      //
      input_handler_->stop ();
      if (resize_signal_)
        resize_signal_->cancel ();
    }

  private:
    void
    init ()
    {
      // Setup Terminal.
      //
      auto sz (get_terminal_size ());
      if (!sz)
      {
        cerr << "error: failed to detect terminal size" << endl;
        exit (1);
      }

      // Bootstrap core state with the detected dimensions (*sz) to make the
      // initial render layout matches the physical screen.
      //
      // Failed that, we might get a frame of 80x24 content before the resize
      // event kicks in.
      //
      auto s (core_.current ());
      auto v (s.view ().resize (*sz));

      core_ = editor_core (loop_, s.with_view (std::move (v)));

      renderer_ = make_unique<terminal_renderer> (*sz);

      // Connect the core state changes to the renderer.
      //
      core_.on_change (
        [this] (const editor_state& st, state_change_type type)
        {
          if (type == state_change_type::cursor)
            renderer_->render_cursor_only (st);
          else
            renderer_->render (st);
        });

      core_.on_message (
        [this] (const string& msg)
        {
          // TODO: Actually render this in the status bar.
          //
          last_file_message_ = msg;
        });

      // Connect input to the handler.
      //
      input_handler_ = make_unique<async_input_handler> (
        loop_,
        [this] (const input_event& e) { handle_input_event (e); });

      // Setup resize signal handler.
      //
      resize_signal_ = make_unique<boost::asio::signal_set> (loop_.context (), SIGWINCH);
      handle_resize ();

      // Load File.
      //
      if (!initial_file_.empty ())
        core_.load (initial_file_);
    }

    void
    handle_resize ()
    {
      resize_signal_->async_wait ([this] (const boost::system::error_code& ec, int)
      {
        if (!ec)
        {
          // Re-arm for next signal.
          //
          handle_resize ();

          // Clear screen immediately to prevent the terminal from displaying
          // wrapped content at the old size. The terminal may have already
          // reflowed text when resized, so we need to clear ASAP before
          // querying the new size.
          //
          cout << "\x1b[2J\x1b[H" << flush;

          // Query new terminal size.
          //
          auto sz (get_terminal_size ());
          if (sz)
          {
            // Update core view and renderer.
            //
            core_.resize (*sz);
            renderer_->resize (*sz);
            renderer_->force_redraw (core_.current ());
          }
        }
      });
    }

    void
    handle_input_event (const input_event& e)
    {
      // Intercept app-level commands before passing to the editor core.
      //
      if (const auto* k = get_if<key_press_event> (&e))
      {
        // Ctrl+Q to Quit.
        //
        if (k->ch == 'q' && has_modifier (k->modifiers, key_modifier::ctrl))
        {
          should_quit_ = true;
          return;
        }

        // Ctrl+S to Save.
        //
        if (k->ch == 's' && has_modifier (k->modifiers, key_modifier::ctrl))
        {
          core_.save ();
          return;
        }
      }

      // Dispatch to logic.
      //
      core_.handle_input (e);
    }

    async_loop loop_;
    editor_core core_ {loop_};

    // Must come after loop/core but before renderer ideally, though strict
    // destruction order usually matters more for the loop.
    //
    terminal_raw_mode raw_mode_;

    unique_ptr<terminal_renderer> renderer_;
    unique_ptr<async_input_handler> input_handler_;
    unique_ptr<boost::asio::signal_set> resize_signal_;

    string initial_file_;
    string last_file_message_;
    bool should_quit_ = false;
  };
}

int
main (int argc, char* argv[])
{
  try
  {
    if (argc > 1)
    {
      mine::terminal_editor_app app (argv[1]);
      app.run ();
    }
    else
    {
      mine::terminal_editor_app app;
      app.run ();
    }

    return 0;
  }
  catch (const exception& e)
  {
    cerr << "error: " << e.what () << endl;
    return 1;
  }
}

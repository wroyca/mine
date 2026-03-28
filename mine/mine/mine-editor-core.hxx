#pragma once

#include <functional>
#include <optional>
#include <variant>
#include <fstream>
#include <sstream>
#include <map>

#include <lua.hpp>

#include <mine/mine-async-loop.hxx>
#include <mine/mine-command.hxx>
#include <mine/mine-core-state.hxx>
#include <mine/mine-io-file.hxx>
#include <mine/mine-terminal-input.hxx>
#include <mine/mine-utility.hxx>
#include <mine/mine-vm-bridge.hxx>
#include <mine/mine-vm.hxx>

namespace mine
{
  // Rendering optimization hints.
  //
  // When the state changes, the renderer needs to know "how much" changed.
  // If we just moved the cursor, we don't want to re-tokenize syntax
  // highlighting or redraw the whole screen.
  //
  enum class change_hint
  {
    cursor,    // Cheap: move hardware cursor, update status line.
    selection, // Medium: redraw text to update highlight, but no text edits.
    content,   // Expensive: buffer modified, full re-render.
    view       // Expensive: scroll, mostly blit but requires full re-render.
  };

  using state_change_type = change_hint;

  // The Nexus.
  //
  class core
  {
  public:
    using change_callback = std::function<void (const state&, change_hint)>;
    using msg_callback = std::function<void (const std::string&)>;

    // We really need the loop to do anything useful (loading/saving).
    //
    explicit
    core (async_loop& l, state s = state ())
      : h_ (std::move (s)),
        l_ (&l)
    {
      // Capture 'this' so the VM can securely invoke our message display.
      //
      print_handler_ = [this] (std::string_view msg)
      {
        show_message (std::string (msg));
      };
    }

    // State Access
    //

    const state&
    current () const noexcept
    {
      return h_.current ();
    }

    // Bindings API
    //

    void
    bind_key (std::string_view chord, std::string_view action)
    {
      auto evt (parse_key_chord (chord));
      if (evt)
      {
        keymaps_[*evt] = std::string (action);
      }
      else
      {
        show_message ("Invalid key chord for mapping: " + std::string (chord));
      }
    }

    // Command Dispatch
    //

    void
    dispatch (const command& cmd)
    {
      // Meta-commands that bypass standard state execution.
      //
      if (cmd.name () == "quit")
      {
        quit ();
        return;
      }

      if (cmd.name () == "undo")
      {
        undo ();
        return;
      }

      if (cmd.name () == "redo")
      {
        redo ();
        return;
      }

      if (cmd.name () == "save")
      {
        save ();
        return;
      }

      if (cmd.name () == "save_and_quit")
      {
        save ();
        quit ();
        return;
      }

      const auto& pre (h_.current ());
      auto post (cmd.execute (pre));

      // Handle the command line submission.
      //
      if (post.cmdline ().is_submitted)
      {
        std::string a (post.cmdline ().content);

        // Deactivate and clear the command line state before we execute the
        // action.
        //
        auto c (post.cmdline ());
        c.active = false;
        c.is_submitted = false;
        c.content.clear ();
        c.cursor_pos = 0;

        post = post.with_cmdline (c);
        h_ = h_.replace_current (std::move (post));

        // Delegate to the command interface to parse the string.
        //
        auto pc (parse_cmdline (a));

        if (pc)
        {
          dispatch (*pc);
        }
        else
        {
          // We didn't recognize the command. Figure out if the user actually
          // typed something or if they just hit enter on a bunch of whitespace.
          //
          auto b (a.find_first_not_of (" \t"));

          if (b != std::string::npos)
          {
            auto e (a.find_last_not_of (" \t"));
            auto t (a.substr (b, e - b + 1));

            show_message ("Unknown command: " + t);
          }
          else
          {
            // The input was effectively empty, so just force the UI to redraw
            // and clear out the line.
            //
            notify (change_hint::selection);
          }
        }

        return;
      }

      change_hint hint;

      if (cmd.modifies_buffer (pre))
      {
        // Text edits are destructive. We push a new state to the history
        // stack so the user can undo them.
        //
        hint = change_hint::content;
        h_ = h_.push (std::move (post));
      }
      else if (pre.view () != post.view ())
      {
        // View changes (scrolling) are significant enough to warrant an
        // undo entry? Debatable. For now, yes, to allow jumping back to
        // previous context.
        //
        hint = change_hint::view;
        h_ = h_.push (std::move (post));
      }
      else if (pre.get_cursor () != post.get_cursor () || pre.cmdline () != post.cmdline ())
      {
        // Cursor movement or cmdline activity.
        //
        // If either the previous state or the new state has an active selection
        // mark, or if we typed in the cmdline, it means the visual state
        // changed. We must force a full redraw via 'selection'.
        //
        if (pre.get_cursor ().has_mark () ||
            post.get_cursor ().has_mark () ||
            pre.cmdline () != post.cmdline ())
          hint = change_hint::selection;
        else
          hint = change_hint::cursor;

        // Note that we replace the current node. We explicitly do not want
        // to spam the undo stack with hundreds of states while the user is
        // dragging the mouse to select text or typing in the cmdline.
        //
        h_ = h_.replace_current (std::move (post));
      }
      else
      {
        // The command executed but resulted in the exact same state (e.g.,
        // hitting Left Arrow when already at line 0, column 0). We just
        // drop it and skip the render cycle.
        //
        return;
      }

      notify (hint);
    }

    void
    handle_input (const input_event& e)
    {
      // First check user-defined key bindings.
      //
      if (auto it = keymaps_.find (e); it != keymaps_.end ())
      {
        auto c = make_command_by_name (it->second);
        if (c)
        {
          dispatch (*c);
          return;
        }
      }

      // Fallback to the standard command mappings.
      //
      auto c (make_command (e));

      if (c)
        dispatch (*c);
    }

    // History
    //

    bool
    can_undo () const noexcept
    {
      return h_.can_undo ();
    }

    bool
    can_redo () const noexcept
    {
      return h_.can_redo ();
    }

    void
    undo ()
    {
      if (can_undo ())
      {
        h_ = h_.undo ();
        notify (change_hint::content);
      }
      else
      {
        show_message ("Already at oldest change");
      }
    }

    void
    redo ()
    {
      if (can_redo ())
      {
        h_ = h_.redo ();
        notify (change_hint::content);
      }
      else
      {
        show_message ("Already at newest change");
      }
    }

    void
    quit ()
    {
      exit (0);
    }

    // File I/O
    //
    // This uses the "Side Effect" pattern. The IO functions return a pure
    // tuple of [new_state, effect_closure]. We apply the state immediately,
    // then schedule the closure on the async loop.
    //

    void
    load (const std::string& path)
    {
      if (!l_)
        return;

      auto [nfb, eff] (mine::load_file (fb_, path));
      fb_ = std::move (nfb);

      // Reset history when loading a completely new file.
      //
      auto s (h_.current ()
              .with_buffer (fb_.content)
              .with_modified (false));

      h_ = history (std::move (s));

      run_io (std::move (eff));
      notify (change_hint::content);
    }

    void
    save ()
    {
      if (!l_)
        return;

      // Cannot save a "new file" that hasn't been named yet.
      //
      if (!std::holds_alternative<existing_file> (fb_.state))
      {
        show_message ("No file name");
        return;
      }

      // Sync the editor content into the file buffer before saving.
      //
      fb_.content = h_.current ().buffer ();

      auto [nfb, eff] (mine::save_file (fb_));
      fb_ = std::move (nfb);

      run_io (std::move (eff));
      notify (change_hint::content); // To clear dirty flag visuals.
    }

    // Queries & Callbacks
    //

    bool
    dirty () const noexcept
    {
      return fb_.is_dirty ();
    }

    bool
    io_busy () const noexcept
    {
      return fb_.io_in_progress ();
    }

    std::optional<std::string>
    filename () const noexcept
    {
      return fb_.file_name ();
    }

    std::optional<float>
    progress () const noexcept
    {
      return fb_.progress_percent ();
    }

    void
    on_change (change_callback c)
    {
      cb_change_ = std::move (c);
    }

    void
    on_message (msg_callback c)
    {
      cb_msg_ = std::move (c);
    }

    // Display a transient message directly onto the command line prompt.
    //
    void
    show_message (const std::string& m)
    {
      auto s (h_.current ().with_cmdline_message (m));
      h_ = h_.replace_current (std::move (s));
      notify (change_hint::selection);

      // Pass it down to the callback if anyone is listening.
      //
      if (cb_msg_)
        cb_msg_ (m);
    }

    // Scripting configuration initialization.
    //
    void
    load_config ()
    {
      vm_.initialize ();
      vm_.set_print_handler (&print_handler_);
      vm_.set_global_userdata ("__mine_core", this);

      register_core_api (vm_);

      // Now bootstrap Fennel. We need to read the compiler from the install
      // data directory and evaluate it directly into the VM.
      //
      std::filesystem::path fp (build_install_data / "fennel.lua");
      std::ifstream f (fp, std::ios::binary);

      if (f)
      {
        std::ostringstream s;
        s << f.rdbuf ();

        auto r (vm_.load_fennel (s.str ()));

        if (!r)
        {
          show_message ("Fennel load error: " + *r.error);
          return;
        }

        // Set up the package path for Fennel. We want the user to be able to
        // require other .fnl files from their config directory, so we inject it
        // into the search path.
        //
        auto cd (get_user_config_dir ());

        if (cd)
        {
          std::string p ((*cd / "?.fnl").string ());
          vm_.add_fennel_path (p);
        }

        // Evaluate the main configuration file if it actually exists.
        //
        auto cf (get_user_config_file ());

        if (cf && std::filesystem::exists (*cf))
        {
          auto er (vm_.execute_fennel_file (cf->string ()));

          if (!er)
          {
            show_message ("Config error: " + *er.error);
          }
        }
      }
      else
      {
        // Warn the user. They might be running from a weird prefix where the
        // data files are missing.
        //
        show_message ("Warning: fennel.lua not found at " + fp.string ());
      }
    }

    // Handle terminal window resize.
    //
    // We need to update the view geometry so that scrolling calculations
    // remain correct.
    //
    // Note that we use `replace_current` here. Resizing is a "meta" change
    // to the editor state, not a semantic edit to the text. We don't want
    // the user to have to hit Undo to revert a window resize.
    //
    void
    resize (screen_size s)
    {
      auto v (h_.current ().view ().resize (s));
      auto ns (h_.current ().with_view (std::move (v)));

      h_ = h_.replace_current (std::move (ns));
      notify (change_hint::view);
    }

  private:
    void
    notify (change_hint h)
    {
      if (cb_change_)
        cb_change_ (h_.current (), h);
    }

    // Run the IO effect on the async loop.
    //
    void
    run_io (io_effect eff)
    {
      // We capture 'this' safely because 'core' outlives the IO operations
      // in the application main loop.
      //
      l_->post ([this, e = std::move (eff)] () mutable
      {
        e ([this] (file_io_action a)
        {
          this->complete_io (std::move (a));
        });
      });
    }

    // Callback from the IO thread (back on the main thread via post).
    //
    void
    complete_io (const file_io_action& a)
    {
      auto [nfb, msg] (update_file_buffer (fb_, a));
      fb_ = std::move (nfb);

      // If the IO operation resulted in new content (e.g., a load finished),
      // update the editor state.
      //
      if (fb_.content != h_.current ().buffer ())
      {
        auto s (h_.current ()
                .with_buffer (fb_.content)
                .with_modified (false));

        h_ = history (std::move (s));
        notify (change_hint::content);
      }

      if (msg)
      {
        show_message (*msg);
      }
    }

  private:
    history h_;
    file_buffer fb_;
    async_loop* l_;
    vm vm_ {vm_limits::permissive()};
    std::map<input_event, std::string> keymaps_;
    std::function<void (std::string_view)> print_handler_;

    change_callback cb_change_;
    msg_callback cb_msg_;
  };

  using editor_core = core;
}

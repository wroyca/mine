#include <mine/mine-editor-core.hxx>

#include <fstream>
#include <sstream>
#include <filesystem>

using namespace std;

namespace mine
{
  core::
  core (async_loop& l, state s)
    : h_ (std::move (s)),
      l_ (&l)
  {
    files_[buffer_id {1}] = file_buffer ();

    print_handler_ = [this] (string_view msg)
    {
      show_message (string (msg));
    };
  }

  void core::
  bind_key (string_view chord, string_view action)
  {
    auto evt (parse_key_chord (chord));
    if (evt)
    {
      keymaps_[*evt] = string (action);
    }
    else
    {
      show_message ("Invalid key chord for mapping: " + string (chord));
    }
  }

  void core::
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

    if (cmd.name () == "close_window")
    {
      vector<window_layout> lays;
      pre.get_layout (lays, 100, 100);

      if (lays.size () <= 1)
      {
        quit ();
        return;
      }

      h_ = h_.replace_current (post);
      notify (change_hint::view);
      return;
    }

    if (post.cmdline ().is_submitted)
    {
      string a (post.cmdline ().content);

      auto c (post.cmdline ());
      c.active = false;
      c.is_submitted = false;
      c.content.clear ();
      c.cursor_pos = 0;

      post = post.with_cmdline (c);
      h_ = h_.replace_current (std::move (post));

      auto pc (parse_cmdline (a));

      if (pc)
      {
        dispatch (*pc);
      }
      else
      {
        auto b (a.find_first_not_of (" \t"));

        if (b != string::npos)
        {
          auto e (a.find_last_not_of (" \t"));
          auto t (a.substr (b, e - b + 1));

          show_message ("Unknown command: " + t);
        }
        else
        {
          notify (change_hint::selection);
        }
      }

      return;
    }

    change_hint hint;

    if (cmd.modifies_buffer (pre))
    {
      hint = change_hint::content;
      h_ = h_.push (std::move (post));
    }
    else if (pre.view () != post.view () || cmd.name () == "split_window")
    {
      hint = change_hint::view;
      h_ = h_.push (std::move (post));
    }
    else if (pre.get_cursor () != post.get_cursor () ||
             pre.cmdline () != post.cmdline () ||
             pre.active_window () != post.active_window ())
    {
      if (pre.get_cursor ().has_mark () ||
          post.get_cursor ().has_mark () ||
          pre.cmdline () != post.cmdline ())
        hint = change_hint::selection;
      else
        hint = change_hint::cursor;

      h_ = h_.replace_current (std::move (post));
    }
    else
    {
      return;
    }

    notify (hint);
  }

  void core::
  handle_input (const input_event& e)
  {
    if (auto it = keymaps_.find (e); it != keymaps_.end ())
    {
      auto c = make_command_by_name (it->second);
      if (c)
      {
        dispatch (*c);
        return;
      }
    }

    auto c (make_command (e));

    if (c)
      dispatch (*c);
  }

  void core::
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

  void core::
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

  void core::
  quit ()
  {
    exit (0);
  }

  void core::
  load (const string& path)
  {
    if (!l_)
      return;

    buffer_id new_id (h_.current ().next_buffer_id ());
    auto s (h_.current ().with_new_buffer (make_empty_buffer (), path));

    s = s.switch_buffer (new_id);

    file_buffer fb;
    auto[nfb, eff] (mine::load_file (std::move (fb), path));
    files_[new_id] = std::move (nfb);

    h_ = h_.push (std::move (s));
    run_io (std::move (eff), new_id);
    notify (change_hint::content);
  }

  void core::
  save ()
  {
    if (!l_)
      return;

    buffer_id active_id (h_.current ().active_buffer_id ());
    auto& fb (files_[active_id]);

    if (!holds_alternative<existing_file> (fb.state))
    {
      show_message ("No file name");
      return;
    }

    fb.content = h_.current ().buffer ();

    auto[nfb, eff] (mine::save_file (std::move (fb)));
    files_[active_id] = std::move (nfb);

    run_io (std::move (eff), active_id);
    notify (change_hint::content);
  }

  void core::
  show_message (const string& m)
  {
    auto s (h_.current ().with_cmdline_message (m));
    h_ = h_.replace_current (std::move (s));
    notify (change_hint::selection);

    if (cb_msg_)
      cb_msg_ (m);
  }

  void core::
  load_config ()
  {
    vm_.initialize ();
    vm_.set_print_handler (&print_handler_);
    vm_.set_global_userdata ("__mine_core", this);

    register_core_api (vm_);

    filesystem::path fp (build_install_data / "fennel.lua");
    ifstream f (fp, ios::binary);

    if (f)
    {
      ostringstream s;
      s << f.rdbuf ();

      auto r (vm_.load_fennel (s.str ()));

      if (!r)
      {
        show_message ("Fennel load error: " + *r.error);
        return;
      }

      auto cd (get_user_config_dir ());

      if (cd)
      {
        string p ((*cd / "?.fnl").string ());
        vm_.add_fennel_path (p);
      }

      auto cf (get_user_config_file ());

      if (cf && filesystem::exists (*cf))
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
      show_message ("Warning: fennel.lua not found at " + fp.string ());
    }
  }

  void core::
  resize (screen_size s)
  {
    auto ns (h_.current ().resize_layout (s));
    h_ = h_.replace_current (std::move (ns));
    notify (change_hint::view);
  }

  void core::
  notify (change_hint h)
  {
    if (cb_change_)
      cb_change_ (h_.current (), h);
  }

  void core::
  run_io (io_effect eff, buffer_id id)
  {
    l_->post ([this, e = std::move (eff), id] () mutable
    {
      e ([this, id] (file_io_action a)
      {
        this->complete_io (id, std::move (a));
      });
    });
  }

  void core::
  complete_io (buffer_id id, const file_io_action& a)
  {
    auto& fb (files_[id]);
    auto [nfb, msg] (update_file_buffer (std::move (fb), a));
    fb = std::move (nfb);

    if (fb.content != h_.current ().get_buffer (id).content)
    {
      auto s (h_.current ().update_buffer (id, fb.content));

      h_ = h_.push (std::move (s));
      notify (change_hint::content);
    }

    if (msg)
    {
      show_message (*msg);
    }
  }
}

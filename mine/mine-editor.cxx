#include <mine/mine-editor.hxx>

#include <fstream>
#include <sstream>
#include <filesystem>

#include <mine/mine-language.hxx>

using namespace std;

namespace mine
{
  editor::
  editor (workspace s)
    : h_ (std::move (s))
  {
    print_handler_ = [this] (string_view msg)
    {
      show_message (string (msg));
    };
  }

  void editor::
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

  void editor::
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
      vector<window_partition> lays;
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
      h_ = h_.replace_current (move (post));

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
      h_ = h_.push (move (post));
    }
    else if (pre.view () != post.view () || cmd.name () == "split_window")
    {
      hint = change_hint::view;
      h_ = h_.push (move (post));
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

      h_ = h_.replace_current (move (post));
    }
    else
    {
      return;
    }

    notify (hint);
  }

  void editor::
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

  void editor::
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

  void editor::
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

  void editor::
  quit ()
  {
    quit_requested_ = true;
  }

  void editor::
  open_document (const string& path, content text)
  {
    document_id new_id (h_.current ().next_document_id ());
    auto s (h_.current ().with_new_document (
      std::move (text), path, detect_language (path)));

    s = s.switch_document (new_id);

    h_ = h_.push (std::move (s));
    notify (change_hint::content);
  }

  void editor::
  mark_saved (document_id id)
  {
    auto s (h_.current ().with_modified (false));
    h_ = h_.replace_current (std::move (s));
    notify (change_hint::content);
  }

  void editor::
  save ()
  {
    document_id active_id (h_.current ().active_document_id ());
    auto const& doc (h_.current ().get_document (active_id));

    if (doc.name.empty ())
    {
      show_message ("No file name");
      return;
    }

    if (cb_save_)
    {
      cb_save_ (active_id, doc.name, h_.current ().active_content ());
    }
  }

  void editor::
  show_message (const string& m)
  {
    auto s (h_.current ().with_cmdline_message (m));
    h_ = h_.replace_current (move (s));
    notify (change_hint::selection);

    if (cb_msg_)
      cb_msg_ (m);
  }

  void editor::
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

  void editor::
  resize (screen_size s)
  {
    auto ns (h_.current ().resize_layout (s));
    h_ = h_.replace_current (move (ns));
    notify (change_hint::view);
  }

  void editor::
  notify (change_hint h)
  {
    if (cb_change_)
      cb_change_ (h_.current (), h);
  }
}

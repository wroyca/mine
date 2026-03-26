#pragma once

#include <memory>
#include <string>

#include <immer/vector.hpp>

#include <mine/mine-core-view.hxx>
#include <mine/mine-core-cursor.hxx>
#include <mine/mine-core-buffer.hxx>

namespace mine
{
  // Command Line State.
  //
  // Manages the prompt below the status bar for Vim-like commands (:w, :q,
  // etc.)
  //
  struct cmdline_state
  {
    bool        active {false};
    std::string content;
    std::size_t cursor_pos {0};
    std::string message;

    // Transient flag indicating the user has pressed Enter and the core needs
    // to parse and execute the command.
    //
    bool is_submitted {false};

    bool operator== (const cmdline_state&) const = default;
  };

  // The World.
  //
  // This aggregates editor state into a single immutable value. To change
  // anything in the editor, we produce a new `state` from the old one.
  //
  // Note that because all members are either trivial (bool, cursor) or
  // persistent (buffer, view), copying this class is cheap.
  //
  class state
  {
  public:
    state ()
      : b_ (make_empty_buffer ()),
        c_ (cursor_position (line_number (0), column_number (0))),
        v_ (line_number (0), screen_size (24, 80)),
        m_ (false),
        cmd_ ()
    {
    }

    state (text_buffer b,
           cursor c,
           view v,
           bool m = false,
           cmdline_state cmd = {})
      : b_ (std::move (b)),
        c_ (c),
        v_ (v),
        m_ (m),
        cmd_ (std::move (cmd))
    {
    }

    // Accessors.
    //

    const text_buffer&
    buffer () const noexcept { return b_; }

    const class cursor&
    get_cursor () const noexcept { return c_; }

    const class view&
    view () const noexcept { return v_; }

    bool
    modified () const noexcept { return m_; }

    const cmdline_state&
    cmdline () const noexcept { return cmd_; }

    // Transitions.
    //

    state
    with_buffer (text_buffer b) const
    {
      auto nc (c_.clamp_to_buffer (b));
      auto nv (v_.scroll_to_cursor (nc, b));

      return state (std::move (b), nc, nv, true, cmd_);
    }

    state
    with_cursor (class cursor c) const
    {
      auto nc (c.clamp_to_buffer (b_));
      auto nv (v_.scroll_to_cursor (nc, b_));

      return state (b_, nc, nv, m_, cmd_);
    }

    state
    with_view (class view v) const
    {
      return state (b_, c_, v, m_, cmd_);
    }

    state
    with_modified (bool m) const
    {
      return state (b_, c_, v_, m, cmd_);
    }

    state
    with_cmdline (cmdline_state cmd) const
    {
      return state (b_, c_, v_, m_, std::move (cmd));
    }

    state
    with_cmdline_message (std::string m) const
    {
      cmdline_state c (cmd_);
      c.message = std::move (m);
      return state (b_, c_, v_, m_, std::move (c));
    }

    // Atomic update of both buffer and cursor (common for typing).
    //
    state
    update (text_buffer b, class cursor c) const
    {
      auto nc (c.clamp_to_buffer (b));
      auto nv (v_.scroll_to_cursor (nc, b));

      return state (std::move (b), nc, nv, true, cmd_);
    }

    auto
    operator<=> (const state&) const = delete;

  private:
    text_buffer   b_;
    class cursor  c_;
    class view    v_;
    bool          m_;
    cmdline_state cmd_;
  };

  // The Time Machine.
  //
  // We store a linear sequence of states. Because of structural sharing
  // in `text_buffer` (immer), this doesn't consume much RAM even for
  // thousands of steps.
  //
  class history
  {
  public:
    using container_type = immer::vector<state>;

    history ()
      : s_ (container_type {state {}}),
        i_ (0)
    {
    }

    explicit
    history (state s)
      : s_ (container_type {std::move (s)}),
        i_ (0)
    {
    }

    const state&
    current () const noexcept
    {
      return s_[i_];
    }

    // Record a new significant state (e.g., after typing a char).
    //
    // Note that it wipes any potential "redo" future.
    //
    history
    push (state s) const
    {
      // Truncate history at current point.
      //
      // immer::take() is efficient (O(log N)).
      //
      auto ns (s_.take (i_ + 1));

      ns = ns.push_back (std::move (s));

      return history (std::move (ns), i_ + 1);
    }

    // Update the current state *without* creating a history point.
    //
    // We use this for things like cursor movement or scrolling, which
    // generally shouldn't be undoable actions.
    //
    history
    replace_current (state s) const
    {
      auto ns (s_.set (i_, std::move (s)));
      return history (std::move (ns), i_);
    }

    // Undo / Redo.
    //

    bool
    can_undo () const noexcept
    {
      return i_ > 0;
    }

    bool
    can_redo () const noexcept
    {
      return i_ + 1 < s_.size ();
    }

    history
    undo () const
    {
      if (!can_undo ())
        return *this;

      return history (s_, i_ - 1);
    }

    history
    redo () const
    {
      if (!can_redo ())
        return *this;

      return history (s_, i_ + 1);
    }

    std::size_t
    size () const noexcept
    {
      return s_.size ();
    }

  private:
    history (container_type s, std::size_t i)
      : s_ (std::move (s)),
        i_ (i)
    {
    }

  private:
    container_type s_;
    std::size_t i_;
  };

  // Type alias for compatibility.
  //
  using editor_state = state;
}

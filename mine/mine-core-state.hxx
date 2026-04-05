#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include <immer/map.hpp>
#include <immer/box.hpp>

#include <mine/mine-core-view.hxx>
#include <mine/mine-core-cursor.hxx>
#include <mine/mine-core-buffer.hxx>

namespace mine
{
  constexpr buffer_id invalid_buffer {0};
  constexpr window_id invalid_window {0};

  // Command line state.
  //
  // We manage the prompt below the status bar for Vim-like commands (:w, :q,
  // etc) directly in the state.
  //
  struct cmdline_state
  {
    bool        active {false};
    std::string content;
    std::size_t cursor_pos {0};
    std::string message;

    // Transient flag indicating the user has pressed Enter. We use this to
    // signal the core that it needs to parse and execute the command.
    //
    bool is_submitted {false};

    bool
    operator== (const cmdline_state&) const = default;
  };

  // Buffer state.
  //
  // A buffer represents the actual text content being edited. Notice that we
  // might have multiple windows looking at the exact same buffer, so the text
  // and the modified flag must naturally live here rather than in the window.
  //
  struct buffer_state
  {
    text_buffer content;
    bool        modified {false};
    std::string name;

    bool
    operator== (const buffer_state&) const = default;
  };

  // Window state.
  //
  // A window is simply a viewport onto a buffer. It logically maintains its
  // own cursor and scroll position, independent of other windows sharing the
  // same buffer.
  //
  struct window_state
  {
    buffer_id    buf {invalid_buffer};
    mine::cursor cur;
    mine::view   vw;

    bool
    operator== (const window_state&) const = default;
  };

  // Split direction.
  //
  // We use the common visual semantics here. Horizontal means slicing the
  // screen top to bottom, vertical means side by side.
  //
  enum class split_dir : std::uint8_t
  {
    horizontal,
    vertical
  };

  struct split_node;

  // Nullable wrapper around immer::box.
  //
  // We need value semantics and structural sharing for our layout tree, which
  // immer::box provides. However, immer::box is not natively nullable, nor
  // does it stop infinite recursion on default construction of recursive
  // types. We wrap it in an optional to give us a safe base case and
  // pointer-like ergonomics.
  //
  class split_ptr
  {
  public:
    split_ptr () = default;
    split_ptr (std::nullptr_t) {}

    explicit
    split_ptr (const split_node& n);

    bool
    operator == (const split_ptr& o) const;

    explicit
    operator bool () const
    {
      return box_.has_value ();
    }

    const split_node*
    operator ->() const
    {
      return box_.value ().operator ->();
    }

    const split_node&
    operator * () const
    {
      return box_.value ().operator * ();
    }

  private:
    std::optional<immer::box<split_node>> box_;
  };

  // Layout tree node.
  //
  // We use a simple binary tree to represent the window layout. A node is
  // either a leaf (containing a window ID) or an internal node (splitting
  // the space between two children).
  //
  struct split_node
  {
    bool      is_leaf {true};
    window_id win {invalid_window};
    split_dir dir {split_dir::vertical};
    float     ratio {0.5f};
    split_ptr child1;
    split_ptr child2;

    bool
    operator == (const split_node& o) const
    {
      if (is_leaf != o.is_leaf)
        return false;

      if (is_leaf)
        return win == o.win;

      return dir == o.dir &&
             ratio == o.ratio &&
             child1 == o.child1 &&
             child2 == o.child2;
    }
  };

  inline split_ptr::
  split_ptr (const split_node& n)
    : box_ (immer::box<split_node> (n)) {}

  inline bool
  split_ptr::operator == (const split_ptr& o) const
  {
    if (box_.has_value () != o.box_.has_value ())
      return false;
    if (!box_.has_value ())
      return true;

    // Let immer::box do the heavy lifting. It checks identity first
    // before falling back to deep structural comparison.
    //
    return box_.value () == o.box_.value ();
  }

  // Intermediate resolved layout structure.
  //
  // This bridges our logical split tree to the physical rendering grid.
  //
  struct window_layout
  {
    window_id     win;
    std::uint16_t x;
    std::uint16_t y;
    std::uint16_t w;
    std::uint16_t h;
  };

  // The world.
  //
  // We aggregate editor state into a single immutable value. To change
  // anything in the editor, we produce a new state from the old one. Notice
  // that because the layout and open windows are part of the state, we can
  // literally "undo" closing a split.
  //
  class state
  {
  public:
    state ();

    explicit
    state (text_buffer b,
           mine::cursor c,
           mine::view v,
           bool m = false,
           cmdline_state cmd = {});

    // Accessors.
    //

    const text_buffer&
    buffer () const noexcept
    {
      return buffers_.at (windows_.at (active_window_).buf).content;
    }

    const mine::cursor&
    get_cursor () const noexcept
    {
      return windows_.at (active_window_).cur;
    }

    const mine::view&
    view () const noexcept
    {
      return windows_.at (active_window_).vw;
    }

    bool
    modified () const noexcept
    {
      return buffers_.at (windows_.at (active_window_).buf).modified;
    }

    const cmdline_state&
    cmdline () const noexcept
    {
      return cmd_;
    }

    buffer_id
    active_buffer_id () const noexcept
    {
      return windows_.at (active_window_).buf;
    }

    window_id
    active_window () const noexcept
    {
      return active_window_;
    }

    buffer_id
    next_buffer_id () const noexcept
    {
      return next_buffer_id_;
    }

    const window_state&
    get_window (window_id id) const
    {
      return windows_.at (id);
    }

    const buffer_state&
    get_buffer (buffer_id id) const
    {
      return buffers_.at (id);
    }

    screen_size
    global_size () const noexcept
    {
      return screen_size_;
    }

    void
    get_layout (std::vector<window_layout>& out,
                std::uint16_t w,
                std::uint16_t h) const;

    // Transitions.
    //

    state
    with_buffer (text_buffer b) const;

    state
    with_cursor (mine::cursor c) const;

    state
    with_view (mine::view v) const;

    state
    with_modified (bool m) const;

    state
    with_cmdline (cmdline_state cmd) const;

    state
    with_cmdline_message (std::string m) const;

    state
    update (text_buffer b, mine::cursor c) const;

    // Multi-buffer / multi-window transitions.
    //

    state
    with_new_buffer (text_buffer b, std::string name) const;

    state
    update_buffer (buffer_id id, text_buffer b) const;

    state
    switch_buffer (buffer_id id) const;

    state
    split_active_window (split_dir dir) const;

    state
    close_active_window () const;

    state
    switch_window (int dx, int dy) const;

    state
    switch_window_direct (window_id id) const;

    state
    resize_layout (screen_size s) const;

    auto
    operator <=> (const state&) const = delete;

  private:
    immer::map<buffer_id, buffer_state> buffers_;
    immer::map<window_id, window_state> windows_;
    split_ptr layout_;

    window_id active_window_ {invalid_window};
    buffer_id next_buffer_id_ {buffer_id {1}};
    window_id next_window_id_ {window_id {1}};
    screen_size screen_size_ {24, 80};

    cmdline_state cmd_;

    static void
    compute_layout (const split_ptr& node,
                    std::uint16_t x,
                    std::uint16_t y,
                    std::uint16_t w,
                    std::uint16_t h,
                    std::vector<window_layout>& out);

    static split_ptr
    insert_split (const split_ptr& node,
                  window_id target,
                  window_id new_win,
                  split_dir dir);

    static split_ptr
    remove_window (const split_ptr& node, window_id target, bool& removed);

    static window_id
    find_first_leaf (const split_ptr& node);
  };

  // The time machine.
  //
  // We store a linear sequence of states. Because of structural sharing in
  // immer, this doesn't consume much RAM even for thousands of steps.
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

    history
    push (state s) const
    {
      auto ns (s_.take (i_ + 1));
      ns = ns.push_back (std::move (s));
      return history (std::move (ns), i_ + 1);
    }

    history
    replace_current (state s) const
    {
      auto ns (s_.set (i_, std::move (s)));
      return history (std::move (ns), i_);
    }

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

  using editor_state = state;
}

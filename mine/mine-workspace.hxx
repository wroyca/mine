#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include <immer/map.hpp>
#include <immer/box.hpp>

#include <mine/mine-viewport.hxx>
#include <mine/mine-cursor.hxx>
#include <mine/mine-content.hxx>
#include <mine/mine-language.hxx>

namespace mine
{
  constexpr document_id invalid_document {0};
  constexpr window_id invalid_window {0};

  // Command line state.
  //
  // We manage the prompt below the status bar for Vim-like commands (:w, :q,
  // etc) directly in the state.
  //
  struct command_line
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
    operator== (const command_line&) const = default;
  };

  // The persistent representation of an open document.
  //
  // A document is a named, immutable text with a modification flag. Multiple
  // windows can share the same document, so the text and the modified flag
  // live here rather than in the window.
  //
  struct document
  {
    mine::content  text;
    bool           modified {false};
    std::string    name;
    mine::language lang;

    bool
    operator== (const document&) const = default;
  };

  // Window state.
  //
  // A window is simply a viewport onto a buffer. It logically maintains its
  // own cursor and scroll position, independent of other windows sharing the
  // same buffer.
  //
  struct editor_window
  {
    document_id      doc {invalid_document};
    mine::cursor     cursor;
    mine::viewport   viewport;

    bool
    operator== (const editor_window&) const = default;
  };

  // Split direction.
  //
  // We use the common visual semantics here. Horizontal means slicing the
  // screen top to bottom, vertical means side by side.
  //
  enum class layout_direction : std::uint8_t
  {
    horizontal,
    vertical
  };

  struct layout_node;

  // Nullable wrapper around immer::box.
  //
  // We need value semantics and structural sharing for our layout tree, which
  // immer::box provides. However, immer::box is not natively nullable, nor
  // does it stop infinite recursion on default construction of recursive
  // types. We wrap it in an optional to give us a safe base case and
  // pointer-like ergonomics.
  //
  class layout_tree
  {
  public:
    layout_tree () = default;
    layout_tree (std::nullptr_t) {}

    explicit
    layout_tree (const layout_node& n);

    bool
    operator == (const layout_tree& o) const;

    explicit
    operator bool () const
    {
      return box_.has_value ();
    }

    const layout_node*
    operator ->() const
    {
      return box_.value ().operator ->();
    }

    const layout_node&
    operator * () const
    {
      return box_.value ().operator * ();
    }

  private:
    std::optional<immer::box<layout_node>> box_;
  };

  // Layout tree node.
  //
  // We use a simple binary tree to represent the window layout. A node is
  // either a leaf (containing a window ID) or an internal node (splitting
  // the space between two children).
  //
  struct layout_node
  {
    bool      is_leaf {true};
    window_id win {invalid_window};
    layout_direction dir {layout_direction::vertical};
    float     ratio {0.5f};
    layout_tree child1;
    layout_tree child2;

    bool
    operator == (const layout_node& o) const
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

  inline layout_tree::
  layout_tree (const layout_node& n)
    : box_ (immer::box<layout_node> (n)) {}

  inline bool
  layout_tree::operator == (const layout_tree& o) const
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
  struct window_partition
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
  class workspace
  {
  public:
    workspace ();

    explicit
    workspace (content b,
           mine::cursor c,
           mine::viewport v,
           bool m = false,
           command_line cmd = {});

    // Accessors.
    //

    const content&
    active_content () const noexcept
    {
      return buffers_.at (windows_.at (active_window_).doc).text;
    }

    const mine::cursor&
    get_cursor () const noexcept
    {
      return windows_.at (active_window_).cursor;
    }

    const mine::viewport&
    view () const noexcept
    {
      return windows_.at (active_window_).viewport;
    }

    bool
    modified () const noexcept
    {
      return buffers_.at (windows_.at (active_window_).doc).modified;
    }

    const command_line&
    cmdline () const noexcept
    {
      return cmd_;
    }

    document_id
    active_document_id () const noexcept
    {
      return windows_.at (active_window_).doc;
    }

    window_id
    active_window () const noexcept
    {
      return active_window_;
    }

    document_id
    next_document_id () const noexcept
    {
      return next_document_id_;
    }

    const editor_window&
    get_window (window_id id) const
    {
      return windows_.at (id);
    }

    const document&
    get_document (document_id id) const
    {
      return buffers_.at (id);
    }

    screen_size
    global_size () const noexcept
    {
      return screen_size_;
    }

    void
    get_layout (std::vector<window_partition>& out,
                std::uint16_t w,
                std::uint16_t h) const;

    // Transitions.
    //

    [[nodiscard]] workspace
    with_content (content b) const;

    [[nodiscard]] workspace
    with_cursor (mine::cursor c) const;

    [[nodiscard]] workspace
    with_view (mine::viewport v) const;

    [[nodiscard]] workspace
    with_modified (bool m) const;

    [[nodiscard]] workspace
    with_cmdline (command_line cmd) const;

    [[nodiscard]] workspace
    with_cmdline_message (std::string m) const;

    [[nodiscard]] workspace
    update (content b, mine::cursor c) const;

    // Multi-buffer / multi-window transitions.
    //

    [[nodiscard]] workspace
    with_new_document (content b,
                       std::string name,
                       language lang = language::unknown ()) const;

    [[nodiscard]] workspace
    with_document_language (document_id id, language lang) const;

    [[nodiscard]] workspace
    update_document (document_id id, content b) const;

    [[nodiscard]] workspace
    switch_document (document_id id) const;

    [[nodiscard]] workspace
    split_active_window (layout_direction dir) const;

    [[nodiscard]] workspace
    close_active_window () const;

    [[nodiscard]] workspace
    switch_window (int dx, int dy) const;

    [[nodiscard]] workspace
    switch_window_direct (window_id id) const;

    [[nodiscard]] workspace
    resize_layout (screen_size s) const;

    auto
    operator <=> (const workspace&) const = delete;

  private:
    immer::map<document_id, document> buffers_;
    immer::map<window_id, editor_window> windows_;
    layout_tree layout_;

    window_id active_window_ {invalid_window};
    document_id next_document_id_ {document_id {1}};
    window_id next_window_id_ {window_id {1}};
    screen_size screen_size_ {24, 80};

    command_line cmd_;

    static void
    compute_layout (const layout_tree& node,
                    std::uint16_t x,
                    std::uint16_t y,
                    std::uint16_t w,
                    std::uint16_t h,
                    std::vector<window_partition>& out);

    static layout_tree
    insert_split (const layout_tree& node,
                  window_id target,
                  window_id new_win,
                  layout_direction dir);

    static layout_tree
    remove_window (const layout_tree& node, window_id target, bool& removed);

    static window_id
    find_first_leaf (const layout_tree& node);
  };

  // The time machine.
  //
  // We store a linear sequence of states. Because of structural sharing in
  // immer, this doesn't consume much RAM even for thousands of steps.
  //
  class edit_history
  {
  public:
    using container_type = immer::vector<workspace>;

    edit_history ()
      : s_ (container_type {workspace {}}),
        i_ (0)
    {
    }

    explicit
    edit_history (workspace s)
      : s_ (container_type {std::move (s)}),
        i_ (0)
    {
    }

    const workspace&
    current () const noexcept
    {
      return s_[i_];
    }

    [[nodiscard]] edit_history
    push (workspace s) const
    {
      auto ns (s_.take (i_ + 1));
      ns = ns.push_back (std::move (s));
      return edit_history (std::move (ns), i_ + 1);
    }

    [[nodiscard]] edit_history
    replace_current (workspace s) const
    {
      auto ns (s_.set (i_, std::move (s)));
      return edit_history (std::move (ns), i_);
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

    [[nodiscard]] edit_history
    undo () const
    {
      if (!can_undo ())
        return *this;

      return edit_history (s_, i_ - 1);
    }

    [[nodiscard]] edit_history
    redo () const
    {
      if (!can_redo ())
        return *this;

      return edit_history (s_, i_ + 1);
    }

    std::size_t
    size () const noexcept
    {
      return s_.size ();
    }

  private:
    edit_history (container_type s, std::size_t i)
      : s_ (std::move (s)),
        i_ (i)
    {
    }

  private:
    container_type s_;
    std::size_t i_;
  };
}

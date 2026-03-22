#pragma once

#include <string>
#include <optional>

#include <mine/mine-terminal-screen.hxx>
#include <mine/mine-core-state.hxx>

namespace mine
{
  class terminal_renderer
  {
  public:
    terminal_renderer () = default;

    explicit terminal_renderer (screen_size s)
      : current_screen_ (s)
    {
    }

    // Main rendering entry point.
    //
    // This builds the new frame, computes the difference against the
    // current frame, emits the ANSI codes, and updates the internal state.
    //
    void
    render (const editor_state& state);

    // Optimized render for cursor movement only.
    //
    // If we know that only the cursor has moved (and no scrolling or text
    // edits occurred), we can skip the expensive screen build/diff process
    // and just emit the cursor jump code.
    //
    void
    render_cursor_only (const editor_state& state);

    // Hard reset.
    //
    // Clears the physical screen, discards the internal buffer, and forces
    // a full redraw. Useful on startup or if the terminal output gets
    // garbled by external noise (e.g., printf debugging).
    //
    void
    force_redraw (const editor_state& state);

    // Handle terminal window resize.
    //
    // We resize the internal buffer to match the new dimensions. The next
    // call to render() will fill the new area.
    //
    void
    resize (screen_size new_size);

    // Accessors.
    //
    const terminal_screen&
    current_screen () const noexcept { return current_screen_; }

  private:
    terminal_screen current_screen_ {screen_size (24, 80)};
    std::optional<cursor_position> last_cursor_pos_;

    // Composition helpers.
    //
    terminal_screen
    build_screen (const editor_state& state) const;

    void
    draw_buffer (terminal_screen& screen, const editor_state& state) const;

    void
    draw_status_line (terminal_screen& screen, const editor_state& state) const;

    // ANSI Output helpers.
    //
    void
    apply_diff (const screen_diff& diff);

    void
    position_cursor (const editor_state& state);

    void
    move_cursor (screen_position pos);

    void
    set_attributes (cell_attributes attrs);

    void
    clear_screen ();

    void
    write (const std::string& str);

    void
    hide_cursor ();

    void
    show_cursor ();

    void
    begin_sync_update ();

    void
    end_sync_update ();
  };

  int
  estimate_grapheme_width (std::string_view g);
}

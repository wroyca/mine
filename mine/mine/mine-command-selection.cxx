#include <mine/mine-command-selection.hxx>
#include <mine/mine-core-view.hxx>

using namespace std;

namespace mine
{
  begin_selection_command::
  begin_selection_command (uint16_t x, uint16_t y)
    : x_ (x),
      y_ (y)
  {
  }

  editor_state begin_selection_command::
  execute (const editor_state& s) const
  {
    // Terminals typically report coordinates as (x,y), but our screen
    // position struct expects (row,col), which is (y,x).
    //
    screen_position sp (y_, x_);

    auto p (s.view ().screen_to_buffer (sp, s.buffer ()));

    // Grab the cursor and move it to the resolved position.
    //
    auto c (s.get_cursor ().move_to (p));
    c.set_mark ();

    return s.with_cursor (c);
  }

  string_view begin_selection_command::
  name () const noexcept
  {
    return "begin_selection";
  }

  bool begin_selection_command::
  modifies_buffer () const noexcept
  {
    return false;
  }

  update_selection_command::
  update_selection_command (uint16_t x, uint16_t y)
    : x_ (x),
      y_ (y)
  {
  }

  editor_state update_selection_command::
  execute (const editor_state& s) const
  {
    screen_position sp (y_, x_);

    // Again, use the view instance from the state to resolve the drag
    // coordinates.
    //
    auto p (s.view ().screen_to_buffer (sp, s.buffer ()));

    // Move the active cursor point to the new drag coordinates. Since we
    // previously called set_mark() during begin_selection, moving the cursor
    // here automatically expands or shrinks the selected region.
    //
    auto c (s.get_cursor ().move_to (p));

    return s.with_cursor (c);
  }

  string_view update_selection_command::
  name () const noexcept
  {
    return "update_selection";
  }

  bool update_selection_command::
  modifies_buffer () const noexcept
  {
    return false;
  }

  // end_selection_command
  //
  end_selection_command::
  end_selection_command (uint16_t x, uint16_t y)
    : x_ (x),
      y_ (y)
  {
  }

  editor_state end_selection_command::
  execute (const editor_state& s) const
  {
    // For many editors, releasing the mouse button doesn't actually change the
    // internal cursor state; the selection remains active until a keyboard
    // event clears it. If the cursor strayed outside the viewport bounds during
    // a drag, we might do a final clamp here. We could also hook system
    // clipboard integration here if desired. For now, we simply return the
    // state untouched.
    //
    return s;
  }

  string_view end_selection_command::
  name () const noexcept
  {
    return "end_selection";
  }

  bool end_selection_command::
  modifies_buffer () const noexcept
  {
    return false;
  }
}

#include <mine/mine-terminal-input-csi.hxx>

#include <charconv>

using namespace std;

namespace mine
{
  // Parse a semicolon-separated list of parameters (e.g., "1;15;0").
  //
  // We avoid creating substrings or intermediate vectors to keep this
  // path allocation-light, as it runs on every keystroke.
  //
  vector<int> csi_parser::
  parse_params (string_view s)
  {
    vector<int> r;

    if (s.empty ())
      return r;

    // Skip private mode markers (<, ?, >) so integer parsing doesn't fail.
    //
    size_t b (0);
    if (s.front () == '<' || s.front () == '?' || s.front () == '>')
      b = 1;

    size_t e (s.find (';', b));

    while (e != string_view::npos)
    {
      int v (0);
      auto res (from_chars (s.data () + b, s.data () + e, v));

      if (res.ec == errc ())
        r.push_back (v);
      else
        r.push_back (0); // Default to 0 on parse error.

      b = e + 1;
      e = s.find (';', b);
    }

    // Handle the tail.
    //
    if (b < s.size ())
    {
      int v (0);
      auto res (from_chars (s.data () + b, s.data () + s.size (), v));
      r.push_back (res.ec == errc {} ? v : 0);
    }
    else
    {
      // Trailing semicolon implies a final 0.
      //
      r.push_back (0);
    }

    return r;
  }

  // Interpret the parsed CSI parts into a concrete event.
  //
  // The structure is generally: CSI [params] [intermediates] final_byte
  //
  input_event csi_parser::
  interpret_sequence (char f,
                      const vector<int>& p,
                      string_view /*inter*/,
                      char prefix)
  {
    // Modifiers.
    //
    // In many XTerm-compatible sequences (like "1;5A"), the second parameter
    // encodes the modifiers (Shift/Alt/Ctrl).
    //
    key_modifier m (key_modifier::none);

    if (p.size () >= 2 && p[1] > 1)
      m = extract_modifiers (p[1]);

    // Mouse events (SGR 1006 extension).
    //
    // Format: ESC [ < b ; x ; y M (or m for release).
    //
    // We check the prefix '<' to confirm SGR format.
    //
    if (prefix == '<' && (f == 'M' || f == 'm'))
    {
      if (p.size () >= 3)
      {
        int pb (p[0]);

        // Coordinates are 1-based in the terminal. Translate them to 0-based
        // for our internal representation.
        //
        uint16_t x (p[1] > 0 ? p[1] - 1 : 0);
        uint16_t y (p[2] > 0 ? p[2] - 1 : 0);

        // Determine the overall mouse state. Note that 'm' explicitly
        // indicates a release. If it is 'M', it can be either a press
        // or a drag, which we disambiguate using the motion bit (0x20).
        //
        mouse_state s (f == 'm'         ? mouse_state::release :
                       (pb & 0x20) != 0 ? mouse_state::drag    :
                                          mouse_state::press);

        // Extract modifiers embedded in the button code (bits 2,3,4).
        //
        if (pb & 4)  m = m | key_modifier::shift;
        if (pb & 8)  m = m | key_modifier::alt;
        if (pb & 16) m = m | key_modifier::ctrl;

        // Strip out everything but the raw button ID. We mask out the modifiers
        // (0x1c) and the motion bit (0x20), so we clear 0x3c.
        //
        // Note that the resulting values are:
        //
        // 00, 01, 02 : Left, Middle, Right.
        // 64, 65     : Scroll Up, Down.
        //
        uint8_t b (static_cast<uint8_t> (pb & ~0x3C));

        return mouse_event {x, y, b, m, s};
      }
    }

    // Cursor / Arrow keys.
    //
    // These typically end in A-H (e.g., UP is 'A').
    //
    if (f >= 'A' && f <= 'H')
      return special_key_event {map_cursor_key (f), m};

    // Tilde sequences (Function/Editing keys).
    //
    // These look like ESC [ <num> ~ (e.g., PageUp is "5~").
    //
    if (f == '~' && !p.empty ())
      return special_key_event {map_function_key (p[0]), m};

    // If we got here, we don't recognize the sequence. Fallback to acting
    // as if the user just pressed ESC, or maybe ignore it? Let's return
    // unknown for now.
    //
    return special_key_event {special_key::unknown, key_modifier::none};
  }

  special_key csi_parser::
  map_cursor_key (char f)
  {
    switch (f)
    {
      case 'A': return special_key::up;
      case 'B': return special_key::down;
      case 'C': return special_key::right;
      case 'D': return special_key::left;
      case 'H': return special_key::home;
      case 'F': return special_key::end;
      default:  return special_key::unknown;
    }
  }

  special_key csi_parser::
  map_function_key (int p)
  {
    // These mappings are largely based on the VT220 / XTerm defaults.
    //
    switch (p)
    {
      // Editing block.
      //
      case 1: return special_key::home;
      case 2: return special_key::insert;
      case 3: return special_key::delete_key;
      case 4: return special_key::end;
      case 5: return special_key::page_up;
      case 6: return special_key::page_down;
      case 7: return special_key::home; // RXVT style
      case 8: return special_key::end;  // RXVT style

      // Function keys (F1-F12).
      // Note the gaps; F1-F5 are often mapped to 11-15, but F6 starts at 17.
      //
      case 11: return special_key::f1;
      case 12: return special_key::f2;
      case 13: return special_key::f3;
      case 14: return special_key::f4;
      case 15: return special_key::f5;
      case 17: return special_key::f6;
      case 18: return special_key::f7;
      case 19: return special_key::f8;
      case 20: return special_key::f9;
      case 21: return special_key::f10;
      case 23: return special_key::f11;
      case 24: return special_key::f12;

      default: return special_key::unknown;
    }
  }

  key_modifier csi_parser::
  extract_modifiers (int p)
  {
    // In XTerm protocol, modifiers are encoded as (param - 1).
    //   2: Shift
    //   3: Alt
    //   5: Ctrl
    //   ... and combinations thereof.
    //
    int b (p - 1);
    key_modifier m (key_modifier::none);

    if (b & 1) m = m | key_modifier::shift;
    if (b & 2) m = m | key_modifier::alt;
    if (b & 4) m = m | key_modifier::ctrl;
    if (b & 8) m = m | key_modifier::meta;

    return m;
  }
}

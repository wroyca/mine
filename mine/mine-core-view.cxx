#include <mine/mine-core-view.hxx>
#include <mine/mine-terminal.hxx>
#include <mine/mine-unicode.hxx>

namespace mine
{
  cursor_position view::
  screen_to_buffer (screen_position p, const text_buffer& b) const noexcept
  {
    // Translate the physical screen row to a logical document line by
    // accounting for our internal scroll offset.
    //
    line_number l (top_.value + p.row);

    // It is quite possible for the screen position to lie past the end of the
    // document. In this case, we clamp it to the last available line and
    // effectively force the column to the very end of that line.
    //
    if (l.value >= b.line_count ())
    {
      // Naturally, if the buffer is completely empty, we just park at the
      // origin.
      //
      if (b.line_count () == 0)
        return cursor_position (line_number (0), column_number (0));

      l.value = b.line_count () - 1;
      p.col = 0xffff; // Force to the end of the line.
    }

    const auto& ln (b.line_at (l));

    // If the line is empty, bail out early.
    //
    if (ln.count () == 0)
      return cursor_position (l, column_number (0));

    auto t (ln.view ());
    const auto& s (ln.idx.get_segmentation ());
    auto r (make_grapheme_range (s));

    uint16_t sc (0);
    std::size_t gc (0);

    // Walk through the graphemes and accumulate their visual width. The idea
    // is to stop as soon as adding the next grapheme would push us past the
    // requested screen column.
    //
    for (auto i (r.begin ()); i != r.end (); ++i)
    {
      if (sc >= p.col)
        break;

      auto g (i->text (t));
      int w (estimate_grapheme_width (g));

      // Note that grapheme width estimation can sometimes return 0 or negative
      // values (for instance, on control or zero-width characters). We fall
      // back to 1 to ensure we actually make visual progress.
      //
      if (w <= 0)
        w = 1;

      if (sc + static_cast<uint16_t> (w) > p.col)
        break;

      sc += static_cast<uint16_t> (w);
      gc++;
    }

    return cursor_position (l, column_number (gc));
  }
}

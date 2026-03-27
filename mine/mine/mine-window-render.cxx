#include <mine/mine-window-render.hxx>
#include <mine/mine-assert.hxx>
#include <mine/mine-unicode.hxx>
#include <mine/mine-window-opengl-typography-material.hxx>

#include <algorithm>
#include <string>

using namespace std;

namespace mine
{
  namespace
  {
    uint32_t
    decode_utf8 (string_view s)
    {
      if (s.empty ())
        return 0;

      unsigned char c (static_cast<unsigned char> (s[0]));

      // Return ASCII as-is. We expect the vast majority of our text to hit this
      // path, so keep it fast.
      //
      if (c < 0x80)
        return c;

      // Handle multi-byte sequences. We don't try to be a fully compliant
      // validator here, just enough to decode valid input. If the sequence is
      // truncated, we just return 0 to avoid reading past the end.
      //
      if ((c & 0xE0) == 0xC0 && s.size () >= 2)
        return ((c & 0x1F) << 6) | (s[1] & 0x3F);

      if ((c & 0xF0) == 0xE0 && s.size () >= 3)
        return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);

      if ((c & 0xF8) == 0xF0 && s.size () >= 4)
        return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) |
          ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);

      return 0;
    }

    int
    estimate_width (string_view g)
    {
      if (g.empty ())
        return 0;

      unsigned char c (static_cast<unsigned char> (g[0]));

      // Standard ASCII characters take 1 column.
      //
      if (c < 0x80)
        return 1;

      // Wide characters (like CJK) generally start with 0xE0 or higher in
      // UTF-8. This is obviously a gross oversimplification since we don't
      // query the full Unicode East Asian Width property, but it gets the job
      // done for basic alignment.
      //
      if (c >= 0xE0)
        return 2;

      return 1;
    }

    // Map our semantic tokens to GUI colors.
    //
    static vec4
    map_token_color (syntax_token_type t, const vec4& def)
    {
      switch (t)
      {
        case syntax_token_type::keyword:  return vec4 (0.8f, 0.4f, 0.8f, 1.0f);
        case syntax_token_type::string:   return vec4 (0.4f, 0.8f, 0.4f, 1.0f);
        case syntax_token_type::type:     return vec4 (0.8f, 0.8f, 0.4f, 1.0f);
        case syntax_token_type::function: return vec4 (0.4f, 0.6f, 1.0f, 1.0f);
        case syntax_token_type::variable: return vec4 (0.4f, 0.8f, 0.8f, 1.0f);
        case syntax_token_type::constant: return vec4 (0.8f, 0.4f, 0.4f, 1.0f);
        case syntax_token_type::comment:  return vec4 (0.5f, 0.5f, 0.5f, 1.0f);
        default:                          return def;
      }
    }
  }

  window_renderer::
  window_renderer ()
    : pack_ (1024, 1024),
      up_ (1024, 1024),
      c_txt_ (1.0f, 1.0f, 1.0f, 1.0f),
      c_cur_ (1.0f, 1.0f, 1.0f, 1.0f),
      c_sel_ (0.3f, 0.5f, 0.8f, 0.5f),
      c_bg_ (0.0f, 0.0f, 0.0f, 1.0f) // Pure black like TUI
  {
    highlighter_.init ();

    bool ok (dev_.init ());
    MINE_INVARIANT (ok);

    sh_txt_ = dev_.create_shader (shaders::text_vertex, shaders::text_fragment);
    sh_sld_ = dev_.create_shader (shaders::text_vertex, shaders::solid_fragment);
    sh_ui_  = dev_.create_shader (shaders::ui_vertex,   shaders::ui_fragment);

    MINE_INVARIANT (sh_txt_.valid ());
    MINE_INVARIANT (sh_sld_.valid ());
    MINE_INVARIANT (sh_ui_.valid ());

    // Allocate enough space for a reasonably large screen full of text. 64k
    // vertices gives us about 10k glyphs (6 verts per quad), which is plenty
    // for a standard editor view without needing reallocation.
    //
    size_t mb (65536 * sizeof (text_vertex));
    size_t ui_mb (4096 * sizeof (ui_vertex));

    vbo_txt_ = dev_.create_vbo (mb, nullptr, buffer_usage::dynamic_draw);
    vao_txt_ = dev_.create_vao ();

    // Describe the vertex layout to the device. We interleave position, uv
    // coordinates, and color for cache locality.
    //
    vertex_attr a[] {
      {0, 2, sizeof (text_vertex), offsetof (text_vertex, p), false},
      {1, 2, sizeof (text_vertex), offsetof (text_vertex, uv), false},
      {2, 4, sizeof (text_vertex), offsetof (text_vertex, c), false}};

    dev_.configure_vao (vao_txt_, vbo_txt_, a);

    vbo_sld_ = dev_.create_vbo (mb, nullptr, buffer_usage::dynamic_draw);
    vao_sld_ = dev_.create_vao ();
    dev_.configure_vao (vao_sld_, vbo_sld_, a);

    // Setup UI primitives buffers
    //
    vbo_ui_ = dev_.create_vbo (ui_mb, nullptr, buffer_usage::dynamic_draw);
    vao_ui_ = dev_.create_vao ();

    vertex_attr ui_a[] {
      {0, 2, sizeof (ui_vertex), offsetof (ui_vertex, p), false},
      {1, 2, sizeof (ui_vertex), offsetof (ui_vertex, uv), false},
      {2, 4, sizeof (ui_vertex), offsetof (ui_vertex, c), false},
      {3, 2, sizeof (ui_vertex), offsetof (ui_vertex, dim), false},
      {4, 1, sizeof (ui_vertex), offsetof (ui_vertex, radius), false}};

    dev_.configure_vao (vao_ui_, vbo_ui_, ui_a);

    // Setup UI text buffers (using the standard text format)
    //
    vbo_ui_txt_ = dev_.create_vbo (mb, nullptr, buffer_usage::dynamic_draw);
    vao_ui_txt_ = dev_.create_vao ();
    dev_.configure_vao (vao_ui_txt_, vbo_ui_txt_, a);

    // Setup a single channel texture for our glyph atlas. We only need
    // alpha/red coverage for typography; coloring happens in the shader.
    //
    tex_desc td;
    td.w = pack_.width ();
    td.h = pack_.height ();
    td.fmt = texture_format::r8;

    tex_atl_ = dev_.create_tex (td);

    v_txt_.reserve (65536);
    v_sld_.reserve (65536);
    v_ui_.reserve (4096);
    v_ui_txt_.reserve (4096);
  }

  window_renderer::
  ~window_renderer ()
  {
    // Clean up all GPU resources explicitly. We don't rely on the OS to clean
    // up after us since we might be re-initializing contexts.
    //
    dev_.destroy_vao (vao_txt_);
    dev_.destroy_vbo (vbo_txt_);
    dev_.destroy_vao (vao_sld_);
    dev_.destroy_vbo (vbo_sld_);
    dev_.destroy_vao (vao_ui_);
    dev_.destroy_vbo (vbo_ui_);
    dev_.destroy_vao (vao_ui_txt_);
    dev_.destroy_vbo (vbo_ui_txt_);

    dev_.destroy_shader (sh_txt_);
    dev_.destroy_shader (sh_sld_);
    dev_.destroy_shader (sh_ui_);

    dev_.destroy_tex (tex_atl_);
  }

  window_renderer::
  window_renderer (window_renderer&& x) noexcept
    : dev_ (move (x.dev_)),
      cam_ (move (x.cam_)),
      rast_ (move (x.rast_)),
      pack_ (move (x.pack_)),
      up_ (move (x.up_)),
      highlighter_ (move (x.highlighter_)),
      sh_txt_ (x.sh_txt_),
      sh_sld_ (x.sh_sld_),
      sh_ui_  (x.sh_ui_),
      vao_txt_ (x.vao_txt_),
      vbo_txt_ (x.vbo_txt_),
      vao_sld_ (x.vao_sld_),
      vbo_sld_ (x.vbo_sld_),
      vao_ui_  (x.vao_ui_),
      vbo_ui_  (x.vbo_ui_),
      vao_ui_txt_ (x.vao_ui_txt_),
      vbo_ui_txt_ (x.vbo_ui_txt_),
      tex_atl_ (x.tex_atl_),
      gl_ (move (x.gl_)),
      v_txt_ (move (x.v_txt_)),
      v_sld_ (move (x.v_sld_)),
      v_ui_  (move (x.v_ui_)),
      v_ui_txt_ (move (x.v_ui_txt_)),
      c_txt_ (x.c_txt_),
      c_cur_ (x.c_cur_),
      c_sel_ (x.c_sel_),
      c_bg_ (x.c_bg_),
      max_sy_ (x.max_sy_)
  {
    // Reset the moved-from handles so we don't double-free on destruction. The
    // device wrapper expects handles to be cleanly invalidated.
    //
    x.sh_txt_.reset ();
    x.sh_sld_.reset ();
    x.sh_ui_.reset ();
    x.vao_txt_.reset ();
    x.vbo_txt_.reset ();
    x.vao_sld_.reset ();
    x.vbo_sld_.reset ();
    x.vao_ui_.reset ();
    x.vbo_ui_.reset ();
    x.vao_ui_txt_.reset ();
    x.vbo_ui_txt_.reset ();
    x.tex_atl_.reset ();
  }

  window_renderer& window_renderer::
  operator = (window_renderer&& x) noexcept
  {
    if (this != &x)
    {
      this->~window_renderer ();
      new (this) window_renderer (move (x));
    }
    return *this;
  }

  bool window_renderer::
  load_font (string_view p, uint32_t s)
  {
    return rast_.load (p, s);
  }

  void window_renderer::
  ensure_glyph (uint32_t cp)
  {
    // See if we have this codepoint mapped already. Fast path for the common
    // case where the text hasn't introduced new characters.
    //
    if (gl_.count (cp))
      return;

    // Rasterize the missing codepoint. Bail out if the font doesn't have it,
    // though ideally we should render a "missing glyph" box instead of just
    // skipping it. Something to fix later.
    //
    auto bmp (rast_.rasterize (cp));

    if (!bmp)
      return;

    // Pack the new bitmap into the atlas. We add a 1px border on all sides to
    // avoid texture bleeding when the GPU samples with linear filtering.
    //
    auto r (pack_.pack (bmp->w + 2, bmp->h + 2));

    if (!r)
      return;

    // Write bitmap data to the upload buffer. Note that we check for empty data
    // because characters like 'space' have metrics but no pixels.
    //
    if (!bmp->data.empty ())
      up_.write (r->x + 1, r->y + 1, bmp->w, bmp->h, bmp->data.data ());

    // Store the layout metrics for rendering. We convert them to float here so
    // we aren't doing the cast per-glyph during the hot render loop.
    //
    glyph_info g;
    g.cp = cp;
    g.id = bmp->id;
    g.w = static_cast<float> (bmp->w);
    g.h = static_cast<float> (bmp->h);
    g.bx = bmp->bx;
    g.by = bmp->by;
    g.adv = bmp->adv;

    float iw (1.0f / static_cast<float> (pack_.width ()));
    float ih (1.0f / static_cast<float> (pack_.height ()));

    // Calculate normalized UV coordinates for the shader, making sure to
    // account for that 1px padding we added earlier.
    //
    g.uv.u0 = static_cast<float> (r->x + 1) * iw;
    g.uv.v0 = static_cast<float> (r->y + 1) * ih;
    g.uv.u1 = static_cast<float> (r->x + 1 + bmp->w) * iw;
    g.uv.v1 = static_cast<float> (r->y + 1 + bmp->h) * ih;

    gl_[cp] = g;
  }

  void window_renderer::
  push_quad (vector<text_vertex>& v,
             const vec2& p0,
             const vec2& p1,
             const vec2& uv0,
             const vec2& uv1,
             const vec4& c)
  {
    // Emit two triangles to form a quad. Standard counter-clockwise winding
    // so backface culling doesn't chew them up if we ever turn it on.
    //
    v.push_back (text_vertex {vec2 (p0.x, p0.y), vec2 (uv0.x, uv0.y), c});
    v.push_back (text_vertex {vec2 (p1.x, p0.y), vec2 (uv1.x, uv0.y), c});
    v.push_back (text_vertex {vec2 (p0.x, p1.y), vec2 (uv0.x, uv1.y), c});

    v.push_back (text_vertex {vec2 (p1.x, p0.y), vec2 (uv1.x, uv0.y), c});
    v.push_back (text_vertex {vec2 (p1.x, p1.y), vec2 (uv1.x, uv1.y), c});
    v.push_back (text_vertex {vec2 (p0.x, p1.y), vec2 (uv0.x, uv1.y), c});
  }

  void window_renderer::
  push_ui_quad (vector<ui_vertex>& v,
                const vec2& p0,
                const vec2& p1,
                const vec4& c,
                float radius)
  {
    vec2 dim (p1.x - p0.x, p1.y - p0.y);

    v.push_back (ui_vertex {vec2 (p0.x, p0.y), vec2 (0.0f, 0.0f), c, dim, radius});
    v.push_back (ui_vertex {vec2 (p1.x, p0.y), vec2 (1.0f, 0.0f), c, dim, radius});
    v.push_back (ui_vertex {vec2 (p0.x, p1.y), vec2 (0.0f, 1.0f), c, dim, radius});

    v.push_back (ui_vertex {vec2 (p1.x, p0.y), vec2 (1.0f, 0.0f), c, dim, radius});
    v.push_back (ui_vertex {vec2 (p1.x, p1.y), vec2 (1.0f, 1.0f), c, dim, radius});
    v.push_back (ui_vertex {vec2 (p0.x, p1.y), vec2 (0.0f, 1.0f), c, dim, radius});
  }

  void window_renderer::
  render (const editor_state& s, bool track)
  {
    highlighter_.update (s.buffer ());

    dev_.clear (c_bg_.x, c_bg_.y, c_bg_.z, c_bg_.w);

    float lh (rast_.line_height ());

    // If we haven't loaded a font or the height is bogus, just bail. Better to
    // render a blank background than to divide by zero later.
    //
    if (lh <= 0.0f)
      return;

    v_txt_.clear ();
    v_sld_.clear ();
    v_ui_.clear ();
    v_ui_txt_.clear ();

    const auto& b (s.buffer ());
    const auto& c (s.get_cursor ());

    // Figure out our selection boundaries. If the cursor has an active mark, we
    // need to sort the start and end positions so our sweep logic works
    // regardless of which direction the user dragged.
    //
    bool hs (c.has_mark ());
    cursor_position ss (c.position ());
    cursor_position se (c.position ());

    if (hs)
    {
      ss = min (c.mark (), c.position ());
      se = max (c.mark (), c.position ());
    }

    // Calculate the physical cursor coordinates. We have to walk the graphemes
    // on the cursor's line to sum up their advances, because variable-width
    // fonts mean we can't just multiply column * average_width.
    //
    float cx (0.0f);
    float cy (static_cast<float> (c.line ().value) * lh);

    if (c.column ().value > 0)
    {
      const auto& l (b.line_at (c.line ()));
      auto txt (l.view ());
      const auto& seg (l.idx.get_segmentation ());
      auto rng (make_grapheme_range (seg));

      size_t target (c.column ().value);

      for (auto it (rng.begin ()); it != rng.end () && target > 0;
           ++it, --target)
      {
        uint32_t cp (decode_utf8 (it->text (txt)));
        ensure_glyph (cp);
        cx += gl_[cp].adv;
      }
    }

    // Track cursor with the camera if requested. This ensures the cursor stays
    // within the viewport during typing or keyboard navigation.
    //
    if (track)
      cam_.make_visible (vec2 (cx, cy), lh * 2.0f);

    max_sy_ = static_cast<float> (b.line_count ()) * lh;
    cam_.clamp_scroll (0.0f, max_sy_);

    vec2 cam (cam_.position ());
    vec2 vp (cam_.viewport ());
    float zm (cam_.zoom ());

    float vh (vp.y / zm);

    // Calculate the visible range of lines based on the camera viewport. We pad
    // the end by a couple of lines to prevent glyphs from popping into
    // existence as they scroll over the bottom edge.
    //
    size_t first (static_cast<size_t> (max (0.0f, cam.y / lh)));
    size_t last (static_cast<size_t> ((cam.y + vh) / lh) + 2);

    float y (static_cast<float> (first) * lh);
    float asc (rast_.ascender ());

    auto highlights (highlighter_.query_lines (first, last));

    for (size_t i (first); i < last; ++i)
    {
      // Draw the end-of-buffer tildes if we are past the last line
      //
      if (i >= b.line_count ())
      {
        uint32_t cp = '~';
        ensure_glyph (cp);

        auto g_it = gl_.find (cp);
        if (g_it != gl_.end ())
        {
          const auto& g = g_it->second;
          float x0 = g.bx;
          float y0 = y + (asc - g.by);
          float x1 = x0 + g.w;
          float y1 = y0 + g.h;

          push_quad (v_txt_,
                     vec2 (x0, y0),
                     vec2 (x1, y1),
                     vec2 (g.uv.u0, g.uv.v0),
                     vec2 (g.uv.u1, g.uv.v1),
                     vec4 (0.3f, 0.5f, 0.9f, 1.0f));
        }
        y += lh;
        continue;
      }

      const auto& l (b.line_at (line_number (i)));
      auto txt (l.view ());
      const auto& seg (l.idx.get_segmentation ());
      auto rng (make_grapheme_range (seg));

      float x (0.0f);
      size_t lcol (0);

      for (auto it (rng.begin ()); it != rng.end (); ++it, ++lcol)
      {
        uint32_t cp (decode_utf8 (it->text (txt)));
        ensure_glyph (cp);

        auto g_it (gl_.find (cp));

        if (g_it != gl_.end ())
        {
          const auto& g (g_it->second);

          cursor_position cur {line_number (i), column_number (lcol)};

          // Draw the selection background if this grapheme falls within the
          // highlighted range. We skip the exact cursor position so the cursor
          // block itself remains distinct.
          //
          if (hs && cur >= ss && cur <= se && cur != c.position ())
          {
            push_quad (v_sld_,
                       vec2 (x, y),
                       vec2 (x + g.adv, y + lh),
                       vec2 (0.0f, 0.0f),
                       vec2 (0.0f, 0.0f),
                       c_sel_);
          }

          syntax_token_type token (syntax_token_type::none);
          size_t off (it->byte_offset);

          for (const auto& hl : highlights)
          {
            if (i > hl.start_line || (i == hl.start_line && off >= hl.start_col_byte))
            {
              if (i < hl.end_line || (i == hl.end_line && off < hl.end_col_byte))
              {
                token = hl.type;
              }
            }
          }

          vec4 color (map_token_color (token, c_txt_));

          float x0 (x + g.bx);
          float y0 (y + (asc - g.by));
          float x1 (x0 + g.w);
          float y1 (y0 + g.h);

          // Emit the actual text glyph geometry to the text buffer.
          //
          push_quad (v_txt_,
                     vec2 (x0, y0),
                     vec2 (x1, y1),
                     vec2 (g.uv.u0, g.uv.v0),
                     vec2 (g.uv.u1, g.uv.v1),
                     color);

          x += g.adv;
        }
      }
      y += lh;
    }

    int row (static_cast<int> (c.line ().value - first));

    // Draw the cursor block, but only if it currently sits inside the visible
    // screen region.
    //
    if (row >= 0 && row < static_cast<int> (last - first))
    {
      push_quad (v_sld_,
                 vec2 (cx, cy),
                 vec2 (cx + 2.0f, cy + lh),
                 vec2 (0.0f, 0.0f),
                 vec2 (0.0f, 0.0f),
                 c_cur_);
    }

    // UI Panels (Status Line and Cmdline)
    //
    float ui_h = lh * 2.0f;
    float ui_w = vp.x;
    float ui_x = 0.0f;
    float ui_y = vp.y - ui_h;

    push_ui_quad (v_ui_,
                  vec2 (ui_x, ui_y),
                  vec2 (ui_x + ui_w, ui_y + lh),
                  vec4 (0.7f, 0.7f, 0.7f, 1.0f),
                  0.0f);

    push_ui_quad (v_ui_,
                  vec2 (ui_x, ui_y + lh),
                  vec2 (ui_x + ui_w, ui_y + ui_h),
                  vec4 (0.1f, 0.1f, 0.1f, 1.0f),
                  0.0f);

    string st = " Line " + to_string (c.line ().value + 1) +
                ", Col " + to_string (c.column ().value + 1);

    if (s.modified ())
      st += " [Modified]";

    float tx = 0.0f;
    float text_y = ui_y;

    for (char ch : st)
    {
      uint32_t cp = static_cast<uint32_t> (ch);
      ensure_glyph (cp);

      auto git = gl_.find (cp);
      if (git != gl_.end ())
      {
        const auto& g = git->second;
        float x0 = tx + g.bx;
        float y0 = text_y + (asc - g.by);
        float x1 = x0 + g.w;
        float y1 = y0 + g.h;

        push_quad (v_ui_txt_,
                   vec2 (x0, y0),
                   vec2 (x1, y1),
                   vec2 (g.uv.u0, g.uv.v0),
                   vec2 (g.uv.u1, g.uv.v1),
                   vec4 (0.0f, 0.0f, 0.0f, 1.0f)); // Black text
        tx += g.adv;
      }
    }

    // Figure out what we are actually rendering. If the command line is active,
    // we prefix it with a colon to act as a prompt. Otherwise, we just fall
    // back to the status message.
    //
    string cs (s.cmdline ().active
               ? ":" + s.cmdline ().content
               : s.cmdline ().message);

    float text_c_x (0.0f);
    float text_c_y (ui_y + lh);

    // Render the command or message text.
    //
    // Note that we currently just iterate over bytes rather than doing full
    // UTF-8 decoding here. This works fine for basic ASCII text, but we might
    // need to revisit this if status messages start containing multi-byte
    // characters.
    //
    for (char c_char : cs)
    {
      uint32_t cp (static_cast<uint32_t> (c_char));
      ensure_glyph (cp);

      auto i (gl_.find (cp));
      if (i != gl_.end ())
      {
        const auto& g (i->second);

        float x0 (text_c_x + g.bx);
        float y0 (text_c_y + (asc - g.by));
        float x1 (x0 + g.w);
        float y1 (y0 + g.h);

        push_quad (v_ui_txt_,
                   vec2 (x0, y0),
                   vec2 (x1, y1),
                   vec2 (g.uv.u0, g.uv.v0),
                   vec2 (g.uv.u1, g.uv.v1),
                   vec4 (1.0f, 1.0f, 1.0f, 1.0f));

        text_c_x += g.adv;
      }
    }

    // Now deal with the cursor, but only if the user is actually text_c_yping.
    //
    if (s.cmdline ().active)
    {
      float c_x (0.0f);

      // Account for the prompt character we prepended earlier so the cursor
      // does not end up inside the colon.
      //
      ensure_glyph (':');
      c_x += gl_[':'].adv;

      string_view co (s.cmdline ().content);
      size_t p (s.cmdline ().cursor_pos);
      size_t i (0);

      // We need to calculate the exact X offset for the cursor. Because the
      // user input can contain UTF-8, we have to step through the actual
      // grapheme boundaries instead of just counting bytes up to the cursor
      // position.
      //
      while (i < p && i < co.size ())
      {
        size_t n (next_grapheme_boundary (co, i));
        string_view gs (co.substr (i, n - i));
        uint32_t cp (decode_utf8 (gs));

        ensure_glyph (cp);
        c_x += gl_[cp].adv;
        i = n;
      }

      // Draw the cursor itself. We just push a simple colored quad for it.
      //
      push_quad (v_sld_,
                 vec2 (c_x, text_c_y),
                 vec2 (c_x + 2.0f, text_c_y + lh),
                 vec2 (0.0f, 0.0f),
                 vec2 (0.0f, 0.0f),
                 c_cur_);
    }

    if (up_.is_dirty ())
    {
      auto r (up_.flush ());

      if (!r.data.empty ())
      {
        dev_.update_tex (tex_atl_,
                         r.r.x,
                         r.r.y,
                         r.r.w,
                         r.r.h,
                         r.data.data (),
                         texture_format::r8);
      }
    }

    // Setup standard alpha blending for the rendering passes. We rely on
    // pre-multiplied alpha or standard source-over depending on how the
    // text shader is written.
    //
    dev_.set_blend_enabled (true);
    dev_.set_blend_func (blend_factor::src_alpha,
                         blend_factor::one_minus_src_alpha,
                         blend_factor::one,
                         blend_factor::one_minus_src_alpha);

    mat4 proj (cam_.view_projection ());

    // Dispatch solid geometry first (selections, cursor) so the text is drawn
    // on top of it.
    //
    // World-space passes
    //
    if (!v_sld_.empty ())
    {
      size_t sz (v_sld_.size () * sizeof (text_vertex));
      dev_.update_vbo (vbo_sld_, 0, sz, v_sld_.data ());

      dev_.bind_shader (sh_sld_);
      dev_.set_uniform_mat4 (sh_sld_, "u_projection", proj.data ());

      dev_.bind_vao (vao_sld_);
      dev_.draw_arrays (primitive_topology::triangles,
                        0,
                        static_cast<uint32_t> (v_sld_.size ()));
    }

    // Dispatch the text atlas geometry second.
    //
    if (!v_txt_.empty ())
    {
      size_t sz (v_txt_.size () * sizeof (text_vertex));
      dev_.update_vbo (vbo_txt_, 0, sz, v_txt_.data ());

      dev_.bind_shader (sh_txt_);
      dev_.set_uniform_mat4 (sh_txt_, "u_projection", proj.data ());
      dev_.set_uniform (sh_txt_, "u_atlas", 0);

      dev_.bind_tex (tex_atl_, 0);
      dev_.bind_vao (vao_txt_);
      dev_.draw_arrays (primitive_topology::triangles,
                        0,
                        static_cast<uint32_t> (v_txt_.size ()));
    }

    // Screen-space (UI) passes
    //
    mat4 screen_proj = mat4::ortho (0.0f, vp.x, vp.y, 0.0f, -1.0f, 1.0f);

    if (!v_ui_.empty ())
    {
      size_t sz (v_ui_.size () * sizeof (ui_vertex));
      dev_.update_vbo (vbo_ui_, 0, sz, v_ui_.data ());

      dev_.bind_shader (sh_ui_);
      dev_.set_uniform_mat4 (sh_ui_, "u_projection", screen_proj.data ());

      dev_.bind_vao (vao_ui_);
      dev_.draw_arrays (primitive_topology::triangles,
                        0,
                        static_cast<uint32_t> (v_ui_.size ()));
    }

    if (!v_ui_txt_.empty ())
    {
      size_t sz (v_ui_txt_.size () * sizeof (text_vertex));
      dev_.update_vbo (vbo_ui_txt_, 0, sz, v_ui_txt_.data ());

      dev_.bind_shader (sh_txt_);
      dev_.set_uniform_mat4 (sh_txt_, "u_projection", screen_proj.data ());
      dev_.set_uniform (sh_txt_, "u_atlas", 0);

      dev_.bind_tex (tex_atl_, 0);
      dev_.bind_vao (vao_ui_txt_);
      dev_.draw_arrays (primitive_topology::triangles,
                        0,
                        static_cast<uint32_t> (v_ui_txt_.size ()));
    }
  }

  void window_renderer::
  resize (int w, int h)
  {
    MINE_PRECONDITION (w >= 0 && h >= 0);

    // Propagate the new dimensions to the camera for matrix calculation and to
    // the graphics device for the viewport scissor.
    //
    cam_.set_viewport (vec2 (static_cast<float> (w), static_cast<float> (h)));
    dev_.set_viewport (0, 0, w, h);
  }

  void window_renderer::
  update (float dt)
  {
    // Tick the camera to progress any active smooth scrolling animations.
    //
    cam_.update (dt);
  }

  void window_renderer::
  scroll (float dx, float dy)
  {
    float lh (rast_.line_height ());

    // Multiply the raw scroll delta by a multiple of the line height so the
    // mouse wheel feels appropriately weighted.
    //
    float m (lh * 3.0f);

    cam_.scroll_smooth (vec2 (-dx * m, -dy * m));
    cam_.clamp_scroll (0.0f, max_sy_);
  }

  bool window_renderer::
  is_animating () const
  {
    return cam_.is_animating ();
  }

  screen_position window_renderer::
  screen_to_grid (float px, float py, const editor_state& s)
  {
    float lh (rast_.line_height ());

    if (lh <= 0.0f)
      return screen_position (0, 0);

    // Convert raw screen coordinates (like a mouse click) into world space to
    // account for camera scroll and zoom.
    //
    vec2 w (cam_.screen_to_world (vec2 (px, py)));

    const auto& b (s.buffer ());
    if (b.line_count () == 0)
      return screen_position (0, 0);

    // Clamp to valid line indices to avoid out-of-bounds access if the user
    // clicks past the end of the file.
    //
    int l (static_cast<int> (w.y / lh));
    l = max (0, min (l, static_cast<int> (b.line_count () - 1)));

    const auto& ln (b.line_at (line_number (l)));
    auto txt (ln.view ());
    const auto& seg (ln.idx.get_segmentation ());
    auto rng (make_grapheme_range (seg));

    float cx (0.0f);
    uint16_t tc (0);

    // Walk the graphemes horizontally to find the column that corresponds to
    // the click's X coordinate.
    //
    for (auto it (rng.begin ()); it != rng.end (); ++it)
    {
      uint32_t cp (decode_utf8 (it->text (txt)));
      ensure_glyph (cp);

      auto git (gl_.find (cp));
      float adv (git != gl_.end () ? git->second.adv : 8.0f);

      // Stop searching if we crossed the midpoint of this character. This makes
      // the cursor naturally snap to the closest side of the glyph.
      //
      if (w.x < cx + adv * 0.5f)
        break;

      cx += adv;

      int gw (estimate_width (it->text (txt)));
      tc += static_cast<uint16_t> (gw > 0 ? gw : 1);
    }

    int row (l - static_cast<int> (s.view ().top ().value));
    return screen_position (static_cast<uint16_t> (max (0, row)), tc);
  }
}

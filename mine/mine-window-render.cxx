
#include <mine/mine-window-render.hxx>
#include <mine/mine-contract.hxx>
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
    map_token_color (syntax_token_type t, const vec4& d)
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
        default:                          return d;
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
      c_bg_ (0.0f, 0.0f, 0.0f, 1.0f)
  {
    highlighter_.init ();

    bool ok (dev_.init ());
    MINE_INVARIANT (ok);

    sh_txt_ = dev_.create_shader (shaders::text_vertex, shaders::text_fragment);
    sh_sld_ =
      dev_.create_shader (shaders::text_vertex, shaders::solid_fragment);
    sh_ui_ = dev_.create_shader (shaders::ui_vertex, shaders::ui_fragment);

    MINE_INVARIANT (sh_txt_.valid ());
    MINE_INVARIANT (sh_sld_.valid ());
    MINE_INVARIANT (sh_ui_.valid ());

    // Allocate enough space for a reasonably large screen full of text. 64k
    // vertices gives us about 10k glyphs (6 verts per quad), which is plenty
    // for a standard editor view without needing reallocation.
    //
    size_t mb (65536 * sizeof (text_vertex));
    size_t ub (4096 * sizeof (ui_vertex));

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

    // Setup UI primitives buffers.
    //
    vbo_ui_ = dev_.create_vbo (ub, nullptr, buffer_usage::dynamic_draw);
    vao_ui_ = dev_.create_vao ();

    vertex_attr ua[] {
      {0, 2, sizeof (ui_vertex), offsetof (ui_vertex, p), false},
      {1, 2, sizeof (ui_vertex), offsetof (ui_vertex, uv), false},
      {2, 4, sizeof (ui_vertex), offsetof (ui_vertex, c), false},
      {3, 2, sizeof (ui_vertex), offsetof (ui_vertex, dim), false},
      {4, 1, sizeof (ui_vertex), offsetof (ui_vertex, radius), false}};

    dev_.configure_vao (vao_ui_, vbo_ui_, ua);

    // Setup UI text buffers (using the standard text format).
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
      cams_ (move (x.cams_)),
      rast_ (move (x.rast_)),
      pack_ (move (x.pack_)),
      up_ (move (x.up_)),
      highlighter_ (move (x.highlighter_)),
      sh_txt_ (x.sh_txt_),
      sh_sld_ (x.sh_sld_),
      sh_ui_ (x.sh_ui_),
      vao_txt_ (x.vao_txt_),
      vbo_txt_ (x.vbo_txt_),
      vao_sld_ (x.vao_sld_),
      vbo_sld_ (x.vbo_sld_),
      vao_ui_ (x.vao_ui_),
      vbo_ui_ (x.vbo_ui_),
      vao_ui_txt_ (x.vao_ui_txt_),
      vbo_ui_txt_ (x.vbo_ui_txt_),
      tex_atl_ (x.tex_atl_),
      gl_ (move (x.gl_)),
      v_txt_ (move (x.v_txt_)),
      v_sld_ (move (x.v_sld_)),
      v_ui_ (move (x.v_ui_)),
      v_ui_txt_ (move (x.v_ui_txt_)),
      c_txt_ (x.c_txt_),
      c_cur_ (x.c_cur_),
      c_sel_ (x.c_sel_),
      c_bg_ (x.c_bg_),
      screen_w_ (x.screen_w_),
      screen_h_ (x.screen_h_)
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
  ensure_glyph (uint32_t c)
  {
    // See if we have this codepoint mapped already. Fast path for the common
    // case where the text hasn't introduced new characters.
    //
    if (gl_.count (c))
      return;

    // Rasterize the missing codepoint. Bail out if the font doesn't have it,
    // though ideally we should render a "missing glyph" box instead of just
    // skipping it. Something to fix later.
    //
    auto bm (rast_.rasterize (c));
    if (!bm)
      return;

    // Pack the new bitmap into the atlas. We add a 1px border on all sides to
    // avoid texture bleeding when the GPU samples with linear filtering.
    //
    auto r (pack_.pack (bm->w + 2, bm->h + 2));
    if (!r)
      return;

    // Write bitmap data to the upload buffer. Note that we check for empty data
    // because characters like 'space' have metrics but no pixels.
    //
    if (!bm->data.empty ())
      up_.write (r->x + 1, r->y + 1, bm->w, bm->h, bm->data.data ());

    // Store the layout metrics for rendering. We convert them to float here so
    // we aren't doing the cast per-glyph during the hot render loop.
    //
    glyph_info g;
    g.cp = c;
    g.id = bm->id;
    g.w = static_cast<float> (bm->w);
    g.h = static_cast<float> (bm->h);
    g.bx = bm->bx;
    g.by = bm->by;
    g.adv = bm->adv;

    float iw (1.0f / static_cast<float> (pack_.width ()));
    float ih (1.0f / static_cast<float> (pack_.height ()));

    // Calculate normalized UV coordinates for the shader, making sure to
    // account for that 1px padding we added earlier.
    //
    g.uv.u0 = static_cast<float> (r->x + 1) * iw;
    g.uv.v0 = static_cast<float> (r->y + 1) * ih;
    g.uv.u1 = static_cast<float> (r->x + 1 + bm->w) * iw;
    g.uv.v1 = static_cast<float> (r->y + 1 + bm->h) * ih;

    gl_[c] = g;
  }

  void window_renderer::
  push_quad (vector<text_vertex>& v,
             const vec2& p0,
             const vec2& p1,
             const vec2& t0,
             const vec2& t1,
             const vec4& c)
  {
    // Emit two triangles to form a quad. Standard counter-clockwise winding
    // so backface culling doesn't chew them up if we ever turn it on.
    //
    v.push_back (text_vertex {vec2 (p0.x, p0.y), vec2 (t0.x, t0.y), c});
    v.push_back (text_vertex {vec2 (p1.x, p0.y), vec2 (t1.x, t0.y), c});
    v.push_back (text_vertex {vec2 (p0.x, p1.y), vec2 (t0.x, t1.y), c});

    v.push_back (text_vertex {vec2 (p1.x, p0.y), vec2 (t1.x, t0.y), c});
    v.push_back (text_vertex {vec2 (p1.x, p1.y), vec2 (t1.x, t1.y), c});
    v.push_back (text_vertex {vec2 (p0.x, p1.y), vec2 (t0.x, t1.y), c});
  }

  void window_renderer::
  push_ui_quad (vector<ui_vertex>& v,
                const vec2& p0,
                const vec2& p1,
                const vec4& c,
                float r)
  {
    vec2 d (p1.x - p0.x, p1.y - p0.y);
    v.push_back (
      ui_vertex {vec2 (p0.x, p0.y), vec2 (0.0f, 0.0f), c, d, r});
    v.push_back (
      ui_vertex {vec2 (p1.x, p0.y), vec2 (1.0f, 0.0f), c, d, r});
    v.push_back (
      ui_vertex {vec2 (p0.x, p1.y), vec2 (0.0f, 1.0f), c, d, r});

    v.push_back (
      ui_vertex {vec2 (p1.x, p0.y), vec2 (1.0f, 0.0f), c, d, r});
    v.push_back (
      ui_vertex {vec2 (p1.x, p1.y), vec2 (1.0f, 1.0f), c, d, r});
    v.push_back (
      ui_vertex {vec2 (p0.x, p1.y), vec2 (0.0f, 1.0f), c, d, r});
  }

  void window_renderer::
  render (const editor_state& s, bool tk)
  {
    highlighter_.update (s.buffer ());

    vector<window_id> tr;

    for (const auto& [i, cm] : cams_)
    {
      try
      {
        s.get_window (i);
      }
      catch (...)
      {
        tr.push_back (i);
      }
    }

    for (auto i : tr)
      cams_.erase (i);

    dev_.clear (c_bg_.x, c_bg_.y, c_bg_.z, c_bg_.w);

    float lh (rast_.line_height ());

    // If we haven't loaded a font or the height is bogus, just bail. Better to
    // render a blank background than to divide by zero later.
    //
    if (lh <= 0.0f)
      return;

    v_ui_.clear ();
    v_ui_txt_.clear ();

    uint16_t lw (static_cast<uint16_t> (screen_w_ / (lh * 0.5f)));
    uint16_t lg (static_cast<uint16_t> ((screen_h_ - lh) / lh));

    vector<window_layout> ls;
    s.get_layout (ls, lw, lg);

    // Dynamically query max layout dimensions so we don't depend on the layout
    // engine filling the hypothetical grid up to 'lw' and 'lg'.
    //
    uint16_t mr (0);
    uint16_t mb (0);

    for (const auto& l : ls)
    {
      mr = max (mr, static_cast<uint16_t> (l.x + l.w));
      mb = max (mb, static_cast<uint16_t> (l.y + l.h));
    }

    for (const auto& l : ls)
    {
      v_txt_.clear ();
      v_sld_.clear ();

      const auto& ws (s.get_window (l.win));
      const auto& bs (s.get_buffer (ws.buf));
      bool a (l.win == s.active_window ());

      auto& cm (cams_[l.win]);

      float fx (static_cast<float> (l.x) * (lh * 0.5f));
      float fy (static_cast<float> (l.y) * lh);
      float fw (static_cast<float> (l.w) * (lh * 0.5f));
      float fh (static_cast<float> (l.h) * lh);

      bool r (l.x + l.w == mr);
      bool b (l.y + l.h == mb);

      float dw (r ? max (fw, screen_w_ - fx) : fw);
      float dh (b ? max (fh, screen_h_ - lh - fy) : fh);

      int px (static_cast<int> (fx));
      int py (static_cast<int> (fy));
      int pw (static_cast<int> (dw));
      int ph (static_cast<int> (dh));

      int th (max (0, ph - static_cast<int> (lh)));

      cm.set_viewport (vec2 (static_cast<float> (pw), static_cast<float> (th)));

      int gy (static_cast<int> (screen_h_) - py - th);
      dev_.set_viewport (px, gy, pw, th);
      dev_.set_scissor (px, gy, pw, th);
      dev_.set_scissor_enabled (true);

      const auto& bc (bs.content);
      const auto& cu (ws.cur);

      // Figure out our selection boundaries. If the cursor has an active mark,
      // we need to sort the start and end positions so our sweep logic works
      // regardless of which direction the user dragged.
      //
      bool hm (cu.has_mark ());
      cursor_position ms (cu.position ());
      cursor_position me (cu.position ());

      if (hm)
      {
        ms = min (cu.mark (), cu.position ());
        me = max (cu.mark (), cu.position ());
      }

      // Calculate the physical cursor coordinates. We have to walk the
      // graphemes on the cursor's line to sum up their advances, because
      // variable-width fonts mean we can't just multiply column *
      // average_width.
      //
      float cx (0.0f);
      float cy (static_cast<float> (cu.line ().value) * lh);

      if (cu.column ().value > 0)
      {
        const auto& cl (bc.line_at (cu.line ()));
        auto tv (cl.view ());
        const auto& sg (cl.idx.get_segmentation ());
        auto rg (make_grapheme_range (sg));

        size_t tg (cu.column ().value);

        for (auto i (rg.begin ()); i != rg.end () && tg > 0; ++i, --tg)
        {
          uint32_t cp (decode_utf8 (i->text (tv)));
          ensure_glyph (cp);
          cx += gl_[cp].adv;
        }
      }

      if (tk && a)
      {
        // Set bottom margin to 0.0f. If the camera internally calculates
        // margins natively relative to item bounds, 0.0f prevents it from
        // padding early.
        //
        cm.make_visible (vec2 (cx, cy), lh * 1.0f, lh * 3.0f, lh * 2.0f, 0.0f);
      }

      float my (max (0.0f, static_cast<float> (bc.line_count ()) * lh - th));
      cm.clamp_scroll (0.0f, my);

      vec2 cp (cm.position ());
      float zm (cm.zoom ());
      float vh (th / zm);

      // Calculate the visible range of lines based on the camera viewport. We
      // pad the end by a couple of lines to prevent glyphs from popping into
      // existence as they scroll over the bottom edge.
      //
      size_t fs (static_cast<size_t> (max (0.0f, cp.y / lh)));
      size_t le (static_cast<size_t> ((cp.y + vh) / lh) + 2);

      float y (static_cast<float> (fs) * lh);
      float as (rast_.ascender ());

      auto hl (highlighter_.query_lines (fs, le));

      // Iterate over only the visible lines. Rendering the whole buffer every
      // frame would tank the framerate on large files.
      //
      for (size_t i (fs); i < le; ++i)
      {
        if (i >= bc.line_count ())
        {
          // Draw the end-of-buffer tildes if we are past the last line.
          //
          uint32_t ch ('~');
          ensure_glyph (ch);

          auto gi (gl_.find (ch));

          if (gi != gl_.end ())
          {
            const auto& g (gi->second);
            float x0 (g.bx);
            float y0 (y + (as - g.by));
            float x1 (x0 + g.w);
            float y1 (y0 + g.h);

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

        const auto& cl (bc.line_at (line_number (i)));
        auto tv (cl.view ());
        const auto& sg (cl.idx.get_segmentation ());
        auto rg (make_grapheme_range (sg));

        float x (0.0f);
        size_t lc (0);

        for (auto it (rg.begin ()); it != rg.end (); ++it, ++lc)
        {
          uint32_t ch (decode_utf8 (it->text (tv)));
          ensure_glyph (ch);

          auto gi (gl_.find (ch));

          if (gi != gl_.end ())
          {
            const auto& g (gi->second);
            cursor_position cr {line_number (i), column_number (lc)};

            // Draw the selection background if this grapheme falls within the
            // highlighted range. We skip the exact cursor position so the
            // cursor block itself remains distinct.
            //
            if (hm && cr >= ms && cr <= me && cr != cu.position ())
            {
              push_quad (v_sld_,
                         vec2 (x, y),
                         vec2 (x + g.adv, y + lh),
                         vec2 (0.0f, 0.0f),
                         vec2 (0.0f, 0.0f),
                         c_sel_);
            }

            syntax_token_type tk (syntax_token_type::none);
            size_t bo (it->byte_offset);

            for (const auto& h : hl)
            {
              if (i > h.start_line ||
                  (i == h.start_line && bo >= h.start_col_byte))
              {
                if (i < h.end_line ||
                    (i == h.end_line && bo < h.end_col_byte))
                  tk = h.type;
              }
            }

            vec4 tc (map_token_color (tk, c_txt_));

            // Offset the glyph using its bearing metrics. The Y axis is usually
            // baseline-oriented in font files, so we push it down by the
            // ascender.
            //
            float x0 (x + g.bx);
            float y0 (y + (as - g.by));
            float x1 (x0 + g.w);
            float y1 (y0 + g.h);

            // Emit the actual text glyph geometry to the text buffer.
            //
            push_quad (v_txt_,
                       vec2 (x0, y0),
                       vec2 (x1, y1),
                       vec2 (g.uv.u0, g.uv.v0),
                       vec2 (g.uv.u1, g.uv.v1),
                       (hm && cr >= ms && cr <= me && cr != cu.position ())
                         ? c_bg_
                         : tc);

            x += g.adv;
          }
        }
        y += lh;
      }

      int rw (static_cast<int> (cu.line ().value - fs));

      // Draw the cursor block, but only if it currently sits inside the visible
      // screen region.
      //
      if (a && rw >= 0 && rw < static_cast<int> (le - fs))
      {
        push_quad (v_sld_,
                   vec2 (cx, cy),
                   vec2 (cx + 2.0f, cy + lh),
                   vec2 (0.0f, 0.0f),
                   vec2 (0.0f, 0.0f),
                   c_cur_);
      }

      if (up_.is_dirty ())
      {
        auto dr (up_.flush ());

        if (!dr.data.empty ())
        {
          dev_.update_tex (tex_atl_,
                           dr.r.x,
                           dr.r.y,
                           dr.r.w,
                           dr.r.h,
                           dr.data.data (),
                           texture_format::r8);
        }
      }

      // Setup standard alpha blending for the rendering passes. We rely on
      // pre-multiplied alpha or standard source-over depending on how the text
      // shader is written.
      //
      dev_.set_blend_enabled (true);
      dev_.set_blend_func (blend_factor::src_alpha,
                           blend_factor::one_minus_src_alpha,
                           blend_factor::one,
                           blend_factor::one_minus_src_alpha);

      mat4 pm (cm.view_projection ());

      // Dispatch solid geometry first (selections, cursor) so the text is drawn
      // on top of it.
      //
      // World-space passes.
      //
      if (!v_sld_.empty ())
      {
        size_t bs (v_sld_.size () * sizeof (text_vertex));
        dev_.update_vbo (vbo_sld_, 0, bs, v_sld_.data ());

        dev_.bind_shader (sh_sld_);
        dev_.set_uniform_mat4 (sh_sld_, "u_projection", pm.data ());
        dev_.bind_vao (vao_sld_);
        dev_.draw_arrays (primitive_topology::triangles,
                          0,
                          static_cast<uint32_t> (v_sld_.size ()));
      }

      // Dispatch the text atlas geometry second.
      //
      if (!v_txt_.empty ())
      {
        size_t bs (v_txt_.size () * sizeof (text_vertex));
        dev_.update_vbo (vbo_txt_, 0, bs, v_txt_.data ());

        dev_.bind_shader (sh_txt_);
        dev_.set_uniform_mat4 (sh_txt_, "u_projection", pm.data ());
        dev_.set_uniform (sh_txt_, "u_atlas", 0);

        dev_.bind_tex (tex_atl_, 0);
        dev_.bind_vao (vao_txt_);
        dev_.draw_arrays (primitive_topology::triangles,
                          0,
                          static_cast<uint32_t> (v_txt_.size ()));
      }
    }

    // Render global command line and UI panels.
    //
    dev_.set_viewport (0,
                       0,
                       static_cast<int> (screen_w_),
                       static_cast<int> (screen_h_));

    dev_.set_scissor_enabled (false);

    float uh (lh);
    float uw (screen_w_);
    float ux (0.0f);
    float uy (screen_h_ - uh);

    push_ui_quad (v_ui_,
                  vec2 (ux, uy),
                  vec2 (ux + uw, uy + uh),
                  vec4 (0.1f, 0.1f, 0.1f, 1.0f),
                  0.0f);

    for (const auto& l : ls)
    {
      const auto& ws (s.get_window (l.win));
      const auto& bs (s.get_buffer (ws.buf));
      bool a (l.win == s.active_window ());

      float px (static_cast<float> (l.x) * (lh * 0.5f));
      float py (static_cast<float> (l.y) * lh);
      float pw (static_cast<float> (l.w) * (lh * 0.5f));
      float ph (static_cast<float> (l.h) * lh);

      bool r (l.x + l.w == mr);
      bool b (l.y + l.h == mb);

      float dw (r ? max (pw, screen_w_ - px) : pw);
      float dh (b ? max (ph, screen_h_ - lh - py) : ph);

      float ty (py + dh - lh);

      vec4 bg (a ? vec4 (0.7f, 0.7f, 0.7f, 1.0f)
                 : vec4 (0.3f, 0.3f, 0.3f, 1.0f));

      vec4 tc (a ? vec4 (0.0f, 0.0f, 0.0f, 1.0f)
                 : vec4 (0.7f, 0.7f, 0.7f, 1.0f));

      push_ui_quad (v_ui_,
                    vec2 (px, ty),
                    vec2 (px + dw, ty + lh),
                    bg,
                    0.0f);

      string st (" Line " + to_string (ws.cur.line ().value + 1) + ", Col " +
                 to_string (ws.cur.column ().value + 1));

      if (bs.modified)
        st += " [Modified]";

      float tx (px);
      float as (rast_.ascender ());

      for (char ch : st)
      {
        uint32_t cp (static_cast<uint32_t> (ch));
        ensure_glyph (cp);

        auto gi (gl_.find (cp));

        if (gi != gl_.end ())
        {
          const auto& g (gi->second);

          if (tx + g.adv > px + dw)
            break;

          float x0 (tx + g.bx);
          float y0 (ty + (as - g.by));
          float x1 (x0 + g.w);
          float y1 (y0 + g.h);

          push_quad (v_ui_txt_,
                     vec2 (x0, y0),
                     vec2 (x1, y1),
                     vec2 (g.uv.u0, g.uv.v0),
                     vec2 (g.uv.u1, g.uv.v1),
                     tc);
          tx += g.adv;
        }
      }

      // Draw the vertical separator border if we are not the rightmost window.
      // We use a dark grey quad to cleanly separate the viewports.
      //
      if (l.x + l.w < mr)
      {
        float bx (static_cast<float> (l.x + l.w) * (lh * 0.5f));
        float bw (lh * 0.5f);

        push_ui_quad (v_ui_,
                      vec2 (bx, py),
                      vec2 (bx + bw, py + dh),
                      vec4 (0.1f, 0.1f, 0.1f, 1.0f),
                      0.0f);
      }
    }

    // Figure out what we are actually rendering. If the command line is active,
    // we prefix it with a colon to act as a prompt. Otherwise, we just fall
    // back to the status message.
    //
    string cs (s.cmdline ().active ? ":" + s.cmdline ().content
                                   : s.cmdline ().message);

    float cx (0.0f);
    float cy (uy);
    float as (rast_.ascender ());

    // Render the command or message text.
    //
    // Note that we currently just iterate over bytes rather than doing full
    // UTF-8 decoding here. This works fine for basic ASCII text, but we might
    // need to revisit this if status messages start containing multi-byte
    // characters.
    //
    for (char ch : cs)
    {
      uint32_t cp (static_cast<uint32_t> (ch));
      ensure_glyph (cp);

      auto i (gl_.find (cp));

      if (i != gl_.end ())
      {
        const auto& g (i->second);

        float x0 (cx + g.bx);
        float y0 (cy + (as - g.by));
        float x1 (x0 + g.w);
        float y1 (y0 + g.h);

        push_quad (v_ui_txt_,
                   vec2 (x0, y0),
                   vec2 (x1, y1),
                   vec2 (g.uv.u0, g.uv.v0),
                   vec2 (g.uv.u1, g.uv.v1),
                   vec4 (1.0f, 1.0f, 1.0f, 1.0f));

        cx += g.adv;
      }
    }

    // Now deal with the cursor, but only if the user is actually typing.
    //
    if (s.cmdline ().active)
    {
      float mx (0.0f);

      // Account for the prompt character we prepended earlier so the cursor
      // does not end up inside the colon.
      //
      ensure_glyph (':');
      mx += gl_[':'].adv;

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
        mx += gl_[cp].adv;
        i = n;
      }

      // Draw the cursor itself. We just push a simple colored quad for it.
      //
      push_ui_quad (v_ui_,
                    vec2 (mx, uy),
                    vec2 (mx + 2.0f, uy + lh),
                    c_cur_,
                    0.0f);
    }

    mat4 sp (mat4::ortho (0.0f, screen_w_, screen_h_, 0.0f, -1.0f, 1.0f));

    if (!v_ui_.empty ())
    {
      size_t bs (v_ui_.size () * sizeof (ui_vertex));
      dev_.update_vbo (vbo_ui_, 0, bs, v_ui_.data ());

      dev_.bind_shader (sh_ui_);
      dev_.set_uniform_mat4 (sh_ui_, "u_projection", sp.data ());

      dev_.bind_vao (vao_ui_);
      dev_.draw_arrays (primitive_topology::triangles,
                        0,
                        static_cast<uint32_t> (v_ui_.size ()));
    }

    if (!v_ui_txt_.empty ())
    {
      size_t bs (v_ui_txt_.size () * sizeof (text_vertex));
      dev_.update_vbo (vbo_ui_txt_, 0, bs, v_ui_txt_.data ());

      dev_.bind_shader (sh_txt_);
      dev_.set_uniform_mat4 (sh_txt_, "u_projection", sp.data ());
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
    // Propagate the new dimensions to the graphics device for the viewport
    // scissor.
    //
    screen_w_ = static_cast<float> (w);
    screen_h_ = static_cast<float> (h);
    dev_.set_viewport (0, 0, w, h);
  }

  void window_renderer::
  update (float dt)
  {
    // Tick the camera to progress any active smooth scrolling animations.
    //
    for (auto& [i, c] : cams_)
      c.update (dt);
  }

  void window_renderer::
  scroll (float dx, float dy, const editor_state& s)
  {
    float lh (rast_.line_height ());

    // Multiply the raw scroll delta by a multiple of the line height so the
    // mouse wheel feels appropriately weighted.
    //
    float m (lh * 3.0f);

    auto it (cams_.find (s.active_window ()));
    if (it != cams_.end ())
    {
      it->second.scroll_smooth (vec2 (-dx * m, -dy * m));

      uint16_t lw (static_cast<uint16_t> (screen_w_ / (lh * 0.5f)));
      uint16_t lg (static_cast<uint16_t> ((screen_h_ - lh) / lh));

      vector<window_layout> ls;
      s.get_layout (ls, lw, lg);

      uint16_t mb (0);
      for (const auto& l : ls)
        mb = max (mb, static_cast<uint16_t> (l.y + l.h));

      for (const auto& l : ls)
      {
        if (l.win == s.active_window ())
        {
          const auto& ws (s.get_window (l.win));
          const auto& bs (s.get_buffer (ws.buf));

          float py (static_cast<float> (l.y) * lh);
          float ph (static_cast<float> (l.h) * lh);

          bool bm (l.y + l.h == mb);
          float dh (bm ? max (ph, screen_h_ - lh - py) : ph);

          float th (max (0.0f, dh - lh));
          float ms (
            max (0.0f,
                 static_cast<float> (bs.content.line_count ()) * lh - th));

          it->second.clamp_scroll (0.0f, ms);
          break;
        }
      }
    }
  }

  bool window_renderer::
  is_animating () const
  {
    for (const auto& [i, c] : cams_)
      if (c.is_animating ())
        return true;

    return false;
  }

  screen_position window_renderer::
  screen_to_grid (float px, float py, const editor_state& s)
  {
    float lh (rast_.line_height ());
    if (lh <= 0.0f)
      return screen_position (0, 0);

    uint16_t lw (static_cast<uint16_t> (screen_w_ / (lh * 0.5f)));
    uint16_t lg (static_cast<uint16_t> ((screen_h_ - lh) / lh));

    vector<window_layout> ls;
    s.get_layout (ls, lw, lg);

    uint16_t mr (0), mb (0);
    for (const auto& l : ls)
    {
      mr = max (mr, static_cast<uint16_t> (l.x + l.w));
      mb = max (mb, static_cast<uint16_t> (l.y + l.h));
    }

    for (const auto& l : ls)
    {
      float wx (static_cast<float> (l.x) * (lh * 0.5f));
      float wy (static_cast<float> (l.y) * lh);
      float ww (static_cast<float> (l.w) * (lh * 0.5f));
      float wh (static_cast<float> (l.h) * lh);

      bool r (l.x + l.w == mr);
      bool b (l.y + l.h == mb);

      float dw (r ? max (ww, screen_w_ - wx) : ww);
      float dh (b ? max (wh, screen_h_ - lh - wy) : wh);

      if (px >= wx && px <= wx + dw && py >= wy && py <= wy + dh)
      {
        auto it (cams_.find (l.win));
        if (it == cams_.end ())
          continue;

        // Convert raw screen coordinates (like a mouse click) into world space
        // to account for camera scroll and zoom.
        //
        vec2 w (it->second.screen_to_world (vec2 (px - wx, py - wy)));

        const auto& bf (s.get_buffer (s.get_window (l.win).buf).content);
        if (bf.line_count () == 0)
          return screen_position (l.y, l.x);

        // Clamp to valid line indices to avoid out-of-bounds access if the user
        // clicks past the end of the file.
        //
        int ln (static_cast<int> (w.y / lh));
        ln = max (0, min (ln, static_cast<int> (bf.line_count () - 1)));

        const auto& cl (bf.line_at (line_number (ln)));
        auto tv (cl.view ());
        const auto& sg (cl.idx.get_segmentation ());
        auto rg (make_grapheme_range (sg));

        float cx (0.0f);
        uint16_t tc (0);

        // Walk the graphemes horizontally to find the column that corresponds
        // to the click's X coordinate.
        //
        for (auto i (rg.begin ()); i != rg.end (); ++i)
        {
          uint32_t cp (decode_utf8 (i->text (tv)));
          ensure_glyph (cp);

          auto gi (gl_.find (cp));
          float adv (gi != gl_.end () ? gi->second.adv : 8.0f);

          // Stop searching if we crossed the midpoint of this character. This
          // makes the cursor naturally snap to the closest side of the glyph.
          //
          if (w.x < cx + adv * 0.5f)
            break;

          cx += adv;

          int gw (estimate_width (i->text (tv)));
          tc += static_cast<uint16_t> (gw > 0 ? gw : 1);
        }

        int rw (ln - static_cast<int> (s.get_window (l.win).vw.top ().value));
        return screen_position (l.y + static_cast<uint16_t> (max (0, rw)),
                                l.x + tc);
      }
    }

    return screen_position (0, 0);
  }
}

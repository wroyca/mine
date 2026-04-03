#include <mine/mine-window-opengl-device.hxx>
#include <mine/mine-contract.hxx>

#include <mine/glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>

using namespace std;

namespace mine
{
  namespace
  {
    GLenum
    to_gl_use (buffer_usage u)
    {
      switch (u)
      {
        case buffer_usage::static_draw:  return GL_STATIC_DRAW;
        case buffer_usage::dynamic_draw: return GL_DYNAMIC_DRAW;
        case buffer_usage::stream_draw:  return GL_STREAM_DRAW;
      }
      return GL_STATIC_DRAW;
    }

    GLenum
    to_gl_prim (primitive_topology t)
    {
      switch (t)
      {
        case primitive_topology::points:         return GL_POINTS;
        case primitive_topology::lines:          return GL_LINES;
        case primitive_topology::line_strip:     return GL_LINE_STRIP;
        case primitive_topology::triangles:      return GL_TRIANGLES;
        case primitive_topology::triangle_strip: return GL_TRIANGLE_STRIP;
        case primitive_topology::triangle_fan:   return GL_TRIANGLE_FAN;
      }
      return GL_TRIANGLES;
    }

    GLenum
    to_gl_blend (blend_factor f)
    {
      switch (f)
      {
        case blend_factor::zero:                     return GL_ZERO;
        case blend_factor::one:                      return GL_ONE;
        case blend_factor::src_color:                return GL_SRC_COLOR;
        case blend_factor::one_minus_src_color:      return GL_ONE_MINUS_SRC_COLOR;
        case blend_factor::dst_color:                return GL_DST_COLOR;
        case blend_factor::one_minus_dst_color:      return GL_ONE_MINUS_DST_COLOR;
        case blend_factor::src_alpha:                return GL_SRC_ALPHA;
        case blend_factor::one_minus_src_alpha:      return GL_ONE_MINUS_SRC_ALPHA;
        case blend_factor::dst_alpha:                return GL_DST_ALPHA;
        case blend_factor::one_minus_dst_alpha:      return GL_ONE_MINUS_DST_ALPHA;
        case blend_factor::constant_color:           return GL_CONSTANT_COLOR;
        case blend_factor::one_minus_constant_color: return GL_ONE_MINUS_CONSTANT_COLOR;
        case blend_factor::constant_alpha:           return GL_CONSTANT_ALPHA;
        case blend_factor::one_minus_constant_alpha: return GL_ONE_MINUS_CONSTANT_ALPHA;
        case blend_factor::src_alpha_saturate:       return GL_SRC_ALPHA_SATURATE;
      }
      return GL_ONE;
    }

    GLenum
    to_gl_fmt (texture_format f)
    {
      switch (f)
      {
        case texture_format::r8:       return GL_RED;
        case texture_format::rg8:      return GL_RG;
        case texture_format::rgb8:     return GL_RGB;
        case texture_format::rgba8:    return GL_RGBA;
        case texture_format::r16f:     return GL_RED;
        case texture_format::rg16f:    return GL_RG;
        case texture_format::rgb16f:   return GL_RGB;
        case texture_format::rgba16f:  return GL_RGBA;
        case texture_format::r32f:     return GL_RED;
        case texture_format::rg32f:    return GL_RG;
        case texture_format::rgb32f:   return GL_RGB;
        case texture_format::rgba32f:  return GL_RGBA;
        default:                       return GL_RGBA;
      }
    }

    GLenum
    to_gl_ifmt (texture_format f)
    {
      switch (f)
      {
        case texture_format::r8:       return GL_R8;
        case texture_format::rg8:      return GL_RG8;
        case texture_format::rgb8:     return GL_RGB8;
        case texture_format::rgba8:    return GL_RGBA8;
        case texture_format::r16f:     return GL_R16F;
        case texture_format::rg16f:    return GL_RG16F;
        case texture_format::rgb16f:   return GL_RGB16F;
        case texture_format::rgba16f:  return GL_RGBA16F;
        case texture_format::r32f:     return GL_R32F;
        case texture_format::rg32f:    return GL_RG32F;
        case texture_format::rgb32f:   return GL_RGB32F;
        case texture_format::rgba32f:  return GL_RGBA32F;
        default:                       return GL_RGBA8;
      }
    }

    GLenum
    to_gl_filt (texture_filter f)
    {
      switch (f)
      {
        case texture_filter::nearest:                return GL_NEAREST;
        case texture_filter::linear:                 return GL_LINEAR;
        case texture_filter::nearest_mipmap_nearest: return GL_NEAREST_MIPMAP_NEAREST;
        case texture_filter::linear_mipmap_nearest:  return GL_LINEAR_MIPMAP_NEAREST;
        case texture_filter::nearest_mipmap_linear:  return GL_NEAREST_MIPMAP_LINEAR;
        case texture_filter::linear_mipmap_linear:   return GL_LINEAR_MIPMAP_LINEAR;
      }
      return GL_LINEAR;
    }

    GLenum
    to_gl_wrap (texture_wrap w)
    {
      switch (w)
      {
        case texture_wrap::repeat:          return GL_REPEAT;
        case texture_wrap::mirrored_repeat: return GL_MIRRORED_REPEAT;
        case texture_wrap::clamp_to_edge:   return GL_CLAMP_TO_EDGE;
        case texture_wrap::clamp_to_border: return GL_CLAMP_TO_BORDER;
      }
      return GL_CLAMP_TO_EDGE;
    }

    GLenum
    to_gl_stg (shader_stage s)
    {
      switch (s)
      {
        case shader_stage::vertex:   return GL_VERTEX_SHADER;
        case shader_stage::fragment: return GL_FRAGMENT_SHADER;
        case shader_stage::geometry: return GL_GEOMETRY_SHADER;
        case shader_stage::compute:  return GL_COMPUTE_SHADER;
      }
      return GL_VERTEX_SHADER;
    }

    bool
    check_compile (GLuint h)
    {
      GLint k (0);
      glGetShaderiv (h, GL_COMPILE_STATUS, &k);

      if (!k)
      {
        char m[512];
        glGetShaderInfoLog (h, 512, nullptr, m);
        cerr << "mine: shader compile error: " << m << '\n';
        return false;
      }

      return true;
    }

    bool
    check_link (GLuint p)
    {
      GLint k (0);
      glGetProgramiv (p, GL_LINK_STATUS, &k);

      if (!k)
      {
        char m[512];
        glGetProgramInfoLog (p, 512, nullptr, m);
        cerr << "mine: shader link error: " << m << '\n';
        return false;
      }

      return true;
    }
  }

  // Render Device Implementation
  //

  bool render_device::
  init ()
  {
    if (init_)
      return true;

    int r (
      gladLoadGLLoader (reinterpret_cast<GLADloadproc> (glfwGetProcAddress)));

    if (!r)
    {
      cerr << "mine: failed to initialize glad bindings\n";
      return false;
    }

    return init_ = true;
  }

  vertex_buf_handle render_device::
  create_vbo (size_t s, const void* d, buffer_usage u)
  {
    MINE_PRECONDITION (init_);

    GLuint i (0);

    glGenBuffers (1, &i);
    glBindBuffer (GL_ARRAY_BUFFER, i);
    glBufferData (GL_ARRAY_BUFFER, static_cast<GLsizeiptr> (s), d, to_gl_use (u));
    glBindBuffer (GL_ARRAY_BUFFER, 0);

    return vertex_buf_handle (i);
  }

  void render_device::
  update_vbo (vertex_buf_handle h, size_t o, size_t s, const void* d)
  {
    MINE_PRECONDITION (h.valid ());

    glBindBuffer    (GL_ARRAY_BUFFER, h.id);
    glBufferSubData (GL_ARRAY_BUFFER, static_cast<GLintptr> (o),
                                      static_cast<GLsizeiptr> (s), d);
    glBindBuffer    (GL_ARRAY_BUFFER, 0);
  }

  void render_device::
  destroy_vbo (vertex_buf_handle h)
  {
    if (h.valid ())
      glDeleteBuffers (1, &h.id);
  }

  index_buf_handle render_device::
  create_ibo (size_t s, const void* d, buffer_usage u)
  {
    MINE_PRECONDITION (init_);

    GLuint i (0);

    glGenBuffers (1, &i);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, i);
    glBufferData (GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr> (s),
                                           d, to_gl_use (u));
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

    return index_buf_handle (i);
  }

  void render_device::
  update_ibo (index_buf_handle h, size_t o, size_t s, const void* d)
  {
    MINE_PRECONDITION (h.valid ());

    glBindBuffer    (GL_ELEMENT_ARRAY_BUFFER, h.id);
    glBufferSubData (GL_ELEMENT_ARRAY_BUFFER, static_cast<GLintptr> (o),
                                              static_cast<GLsizeiptr> (s), d);
    glBindBuffer    (GL_ELEMENT_ARRAY_BUFFER, 0);
  }

  void render_device::
  destroy_ibo (index_buf_handle h)
  {
    if (h.valid ())
      glDeleteBuffers (1, &h.id);
  }

  vertex_arr_handle render_device::
  create_vao ()
  {
    MINE_PRECONDITION (init_);

    GLuint i (0);

    glGenVertexArrays (1, &i);

    return vertex_arr_handle (i);
  }

  void render_device::
  configure_vao (vertex_arr_handle a, vertex_buf_handle b, span<const vertex_attr> ts)
  {
    MINE_PRECONDITION (a.valid ());
    MINE_PRECONDITION (b.valid ());

    glBindVertexArray (a.id);
    glBindBuffer (GL_ARRAY_BUFFER, b.id);

    for (const auto& t : ts)
    {
      glEnableVertexAttribArray (t.loc);
      glVertexAttribPointer (
        t.loc,
        static_cast<GLint> (t.sz),
        GL_FLOAT,
        t.norm ? GL_TRUE : GL_FALSE,
        static_cast<GLsizei> (t.st),
        reinterpret_cast<const void*> (static_cast<uintptr_t> (t.off))
      );
    }

    glBindVertexArray (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
  }

  void render_device::
  configure_vao_indexed (vertex_arr_handle a,
                         vertex_buf_handle b,
                         index_buf_handle i,
                         span<const vertex_attr> ts)
  {
    MINE_PRECONDITION (a.valid ());
    MINE_PRECONDITION (b.valid ());
    MINE_PRECONDITION (i.valid ());

    glBindVertexArray (a.id);
    glBindBuffer (GL_ARRAY_BUFFER, b.id);

    // Remember, the IBO binding is stored in the VAO state. We bind it here but
    // we strictly avoid unbinding it before unbinding the VAO itself, otherwise
    // the VAO will record a null IBO.
    //
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, i.id);

    for (const auto& t : ts)
    {
      glEnableVertexAttribArray (t.loc);
      glVertexAttribPointer (
        t.loc,
        static_cast<GLint> (t.sz),
        GL_FLOAT,
        t.norm ? GL_TRUE : GL_FALSE,
        static_cast<GLsizei> (t.st),
        reinterpret_cast<const void*> (static_cast<uintptr_t> (t.off))
      );
    }

    glBindVertexArray (0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
  }

  void render_device::
  destroy_vao (vertex_arr_handle h)
  {
    if (h.valid ())
      glDeleteVertexArrays (1, &h.id);
  }

  shader_prog_handle render_device::
  create_shader (span<const shader_src> ss)
  {
    MINE_PRECONDITION (init_);

    vector<GLuint> hs;
    hs.reserve (ss.size ());

    for (const auto& s : ss)
    {
      GLuint i (glCreateShader (to_gl_stg (s.stg)));
      const char* c (s.code.data ());
      GLint l (static_cast<GLint> (s.code.size ()));

      glShaderSource (i, 1, &c, &l);
      glCompileShader (i);

      // If compilation fails, we need to carefully clean up any shaders we have
      // already compiled in this loop before bailing out. Otherwise we leak
      // OpenGL resources on error.
      //
      if (!check_compile (i))
      {
        glDeleteShader (i);

        for (auto x : hs)
          glDeleteShader (x);

        return shader_prog_handle (0);
      }

      hs.push_back (i);
    }

    GLuint p (glCreateProgram ());

    for (auto s : hs)
      glAttachShader (p, s);

    glLinkProgram (p);

    // After linking, the individual shader objects are no longer needed by the
    // program so we can detach and delete them to free memory.
    //
    for (auto s : hs)
    {
      glDetachShader (p, s);
      glDeleteShader (s);
    }

    if (!check_link (p))
    {
      glDeleteProgram (p);
      return shader_prog_handle (0);
    }

    return shader_prog_handle (p);
  }

  shader_prog_handle render_device::
  create_shader (string_view v, string_view f)
  {
    shader_src a[2] {{shader_stage::vertex,   v},
                     {shader_stage::fragment, f}};

    return create_shader (a);
  }

  void render_device::
  destroy_shader (shader_prog_handle h)
  {
    if (h.valid ())
      glDeleteProgram (h.id);
  }

  optional<int> render_device::
  get_loc (shader_prog_handle h, string_view n)
  {
    MINE_PRECONDITION (h.valid ());

    string s (n);
    int l (glGetUniformLocation (h.id, s.c_str ()));

    // We return an optional rather than a raw -1. It is perfectly legal and
    // quite common for a shader compiler to optimize away an unused uniform.
    //
    if (l < 0)
      return nullopt;

    return l;
  }

  void render_device::
  set_uniform (shader_prog_handle h, string_view n, float v)
  {
    optional<int> l (get_loc (h, n));

    // Notice we just quietly skip the GL call if the location is missing.
    // Setting a non-existent uniform shouldn't be a fatal error.
    //
    if (l)
    {
      glUseProgram (h.id);
      glUniform1f (*l, v);
    }
  }

  void render_device::
  set_uniform (shader_prog_handle h, string_view n, const vec2& v)
  {
    optional<int> l (get_loc (h, n));

    if (l)
    {
      glUseProgram (h.id);
      glUniform2f (*l, v.x, v.y);
    }
  }

  void render_device::
  set_uniform (shader_prog_handle h, string_view n, const vec3& v)
  {
    optional<int> l (get_loc (h, n));

    if (l)
    {
      glUseProgram (h.id);
      glUniform3f (*l, v.x, v.y, v.z);
    }
  }

  void render_device::
  set_uniform (shader_prog_handle h, string_view n, const vec4& v)
  {
    optional<int> l (get_loc (h, n));

    if (l)
    {
      glUseProgram (h.id);
      glUniform4f (*l, v.x, v.y, v.z, v.w);
    }
  }

  void render_device::
  set_uniform (shader_prog_handle h, string_view n, int v)
  {
    optional<int> l (get_loc (h, n));

    if (l)
    {
      glUseProgram (h.id);
      glUniform1i (*l, v);
    }
  }

  void render_device::
  set_uniform_mat4 (shader_prog_handle h, string_view n, const float* d, bool t)
  {
    optional<int> l (get_loc (h, n));

    if (l)
    {
      glUseProgram (h.id);
      glUniformMatrix4fv (*l, 1, t ? GL_TRUE : GL_FALSE, d);
    }
  }

  texture_2d_handle render_device::
  create_tex (const tex_desc& c, const void* d)
  {
    MINE_PRECONDITION (init_);

    GLuint i (0);

    glGenTextures (1, &i);
    glBindTexture (GL_TEXTURE_2D, i);

    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, to_gl_filt (c.min));
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, to_gl_filt (c.mag));
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, to_gl_wrap (c.ws));
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, to_gl_wrap (c.wt));

    GLenum i_f (to_gl_ifmt (c.fmt));
    GLenum f (to_gl_fmt (c.fmt));
    GLenum t (GL_UNSIGNED_BYTE);

    // For our single-channel R8 font atlas, we swizzle the reads so the shader
    // receives the alpha value automatically.
    //
    if (c.fmt == texture_format::r8)
    {
      glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_ONE);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_ONE);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_ONE);
      glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);
    }

    if (c.fmt == texture_format::r16f ||
        c.fmt == texture_format::rg16f ||
        c.fmt == texture_format::rgb16f ||
        c.fmt == texture_format::rgba16f ||
        c.fmt == texture_format::r32f ||
        c.fmt == texture_format::rg32f ||
        c.fmt == texture_format::rgb32f ||
        c.fmt == texture_format::rgba32f)
    {
      t = GL_FLOAT;
    }

    glTexImage2D (GL_TEXTURE_2D,
                  0,
                  static_cast<GLint> (i_f),
                  static_cast<GLsizei> (c.w),
                  static_cast<GLsizei> (c.h),
                  0,
                  f,
                  t,
                  d);

    if (c.mip)
      glGenerateMipmap (GL_TEXTURE_2D);

    glBindTexture (GL_TEXTURE_2D, 0);

    return texture_2d_handle (i);
  }

  void render_device::
  update_tex (texture_2d_handle h, uint32_t x, uint32_t y,
              uint32_t w, uint32_t ht, const void* d, texture_format f)
  {
    MINE_PRECONDITION (h.valid ());

    glBindTexture (GL_TEXTURE_2D, h.id);

    // If we are dealing with our single-byte formats, bypass standard alignment
    // constraints to avoid unpack errors.
    //
    if (f == texture_format::r8)
      glPixelStorei (GL_UNPACK_ALIGNMENT, 1);

    glTexSubImage2D (
      GL_TEXTURE_2D,
      0,
      static_cast<GLint> (x),
      static_cast<GLint> (y),
      static_cast<GLsizei> (w),
      static_cast<GLsizei> (ht),
      to_gl_fmt (f),
      GL_UNSIGNED_BYTE,
      d
    );

    glBindTexture (GL_TEXTURE_2D, 0);
  }

  void render_device::
  destroy_tex (texture_2d_handle h)
  {
    if (h.valid ())
      glDeleteTextures (1, &h.id);
  }

  void render_device::
  bind_shader (shader_prog_handle h)
  {
    glUseProgram (h.valid () ? h.id : 0);
  }

  void render_device::
  bind_vao (vertex_arr_handle h)
  {
    glBindVertexArray (h.valid () ? h.id : 0);
  }

  void render_device::
  bind_tex (texture_2d_handle h, uint32_t u)
  {
    glActiveTexture (GL_TEXTURE0 + u);
    glBindTexture (GL_TEXTURE_2D, h.valid () ? h.id : 0);
  }

  void render_device::
  draw_arrays (primitive_topology t, uint32_t f, uint32_t c)
  {
    glDrawArrays (to_gl_prim (t),
                  static_cast<GLint> (f),
                  static_cast<GLsizei> (c));
  }

  void render_device::
  draw_elements (primitive_topology t, uint32_t c, uint32_t o)
  {
    glDrawElements (
      to_gl_prim (t),
      static_cast<GLsizei> (c),
      GL_UNSIGNED_INT,
      reinterpret_cast<const void*> (static_cast<uintptr_t> (o * sizeof (uint32_t)))
    );
  }

  void render_device::
  set_viewport (int32_t x, int32_t y, uint32_t w, uint32_t h)
  {
    glViewport (x, y, static_cast<GLsizei> (w), static_cast<GLsizei> (h));
  }

  void render_device::
  set_scissor (int32_t x, int32_t y, uint32_t w, uint32_t h)
  {
    glScissor (x, y, static_cast<GLsizei> (w), static_cast<GLsizei> (h));
  }

  void render_device::
  set_scissor_enabled (bool e)
  {
    if (e)
      glEnable (GL_SCISSOR_TEST);
    else
      glDisable (GL_SCISSOR_TEST);
  }

  void render_device::
  set_blend_enabled (bool e)
  {
    if (e)
      glEnable (GL_BLEND);
    else
      glDisable (GL_BLEND);
  }

  void render_device::
  set_blend_func (blend_factor s_r, blend_factor d_r, blend_factor s_a, blend_factor d_a)
  {
    glBlendFuncSeparate (to_gl_blend (s_r),
                         to_gl_blend (d_r),
                         to_gl_blend (s_a),
                         to_gl_blend (d_a));
  }

  void render_device::
  clear (float r, float g, float b, float a)
  {
    glClearColor (r, g, b, a);
    glClear (GL_COLOR_BUFFER_BIT);
  }
}

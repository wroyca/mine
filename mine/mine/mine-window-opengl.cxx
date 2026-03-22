#include <mine/mine-window-opengl.hxx>

// Note that glad must always be included before glfw to avoid header
// definition collisions.
//
#include <mine/glad/glad.h>
#include <GLFW/glfw3.h>

using namespace std;

namespace mine
{
  opengl_context::
  opengl_context ()
  {
    // Load OpenGL function pointers via glad.
    //
    // We pass the GLFW proc address getter. If this fails, it usually means
    // there is no current context bound to the thread, or the machine
    // fundamentally lacks OpenGL support. We consider this a hard invariant
    // violation because we cannot draw anything without it.
    //
    int r (gladLoadGLLoader (reinterpret_cast<GLADloadproc> (glfwGetProcAddress)));
    MINE_INVARIANT (r != 0);
  }

  string_view opengl_context::
  version () const
  {
    const char* v (reinterpret_cast<const char*> (glGetString (GL_VERSION)));
    MINE_INVARIANT (v != nullptr);

    return string_view (v);
  }

  string_view opengl_context::
  renderer () const
  {
    const char* r (reinterpret_cast<const char*> (glGetString (GL_RENDERER)));
    MINE_INVARIANT (r != nullptr);

    return string_view (r);
  }

  string_view opengl_context::
  vendor () const
  {
    const char* v (reinterpret_cast<const char*> (glGetString (GL_VENDOR)));
    MINE_INVARIANT (v != nullptr);

    return string_view (v);
  }

  bool opengl_context::
  has_extension (string_view ext) const
  {
    MINE_PRECONDITION (!ext.empty ());

    // Get the total number of available extensions.
    //
    GLint n (0);
    glGetIntegerv (GL_NUM_EXTENSIONS, &n);

    // Iterate through the extensions array and compare.
    //
    // We explicitly use glGetStringi because the old glGetString(GL_EXTENSIONS)
    // is deprecated and will return null in modern core profiles.
    //
    for (GLint i (0); i < n; ++i)
    {
      const char* e (reinterpret_cast<const char*> (
        glGetStringi (GL_EXTENSIONS, static_cast<GLuint> (i))));

      if (e != nullptr && string_view (e) == ext)
        return true;
    }

    return false;
  }
}

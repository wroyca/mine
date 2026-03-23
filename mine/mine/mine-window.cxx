#include <mine/mine-window.hxx>

#include <GLFW/glfw3.h>

using namespace std;

namespace mine
{
  namespace
  {
    // Track active windows so we only init GLFW once and tear it down when
    // the last one disappears.
    //
    size_t refs (0);

    void
    error_callback (int c, const char* m)
    {
      // For now, just pipe GLFW errors straight into our invariant handler.
      // Getting a failure here usually means we're running headless without a
      // display server or basic OpenGL support.
      //
      (void)c;
      MINE_INVARIANT (false && m);
    }
  }

  window::
  window (int w, int h, const string& t)
    : w_ (nullptr)
  {
    MINE_PRECONDITION (w > 0 && h > 0);

    // Bootstrap GLFW if we are the first window.
    //
    if (refs == 0)
    {
      glfwSetErrorCallback (error_callback);

      int r (glfwInit ());
      MINE_INVARIANT (r == GLFW_TRUE);
    }
    ++refs;

    // Ask for a modern, core-profile context. We don't really need the legacy
    // fixed-function baggage just to draw some textured quads later.
    //
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint (GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint (GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    // Actually construct the window. Panic if it fails.
    //
    w_ = glfwCreateWindow (w, h, t.c_str (), nullptr, nullptr);
    MINE_INVARIANT (w_ != nullptr);

    glfwMakeContextCurrent (w_);

    // Turn on vsync by default. No need to burn 100% CPU drawing thousands
    // of identical frames when nothing is moving.
    //
    glfwSwapInterval (1);
  }

  window::
  ~window ()
  {
    if (w_ != nullptr)
    {
      glfwDestroyWindow (w_);
      w_ = nullptr;

      MINE_INVARIANT (refs > 0);
      --refs;

      // Tear everything down if we were the last window standing.
      //
      if (refs == 0)
        glfwTerminate ();
    }
  }

  window::
  window (window&& x) noexcept
    : w_ (exchange (x.w_, nullptr))
  {
  }

  window& window::
  operator= (window&& x) noexcept
  {
    if (this != &x)
    {
      // Make sure we tear down our current window before taking ownership of
      // the new one. This is basically the destructor logic again.
      //
      if (w_ != nullptr)
      {
        glfwDestroyWindow (w_);

        MINE_INVARIANT (refs > 0);
        --refs;

        if (refs == 0)
          glfwTerminate ();
      }

      w_ = exchange (x.w_, nullptr);
    }
    return *this;
  }

  bool window::
  update () const
  {
    MINE_PRECONDITION (w_ != nullptr);

    glfwPollEvents ();
    return !closing ();
  }

  void window::
  swap_buffers () const
  {
    MINE_PRECONDITION (w_ != nullptr);
    glfwSwapBuffers (w_);
  }

  pair<int, int> window::
  framebuffer_size () const
  {
    MINE_PRECONDITION (w_ != nullptr);

    int w (0);
    int h (0);
    glfwGetFramebufferSize (w_, &w, &h);

    return {w, h};
  }

  bool window::
  closing () const
  {
    MINE_PRECONDITION (w_ != nullptr);
    return glfwWindowShouldClose (w_) != 0;
  }
}

#include <mine/mine-window-render.hxx>

#include <mine/glad/glad.h>

#include <mine/mine-assert.hxx>

namespace mine
{
  window_renderer::
  window_renderer ()
  {
  }

  void window_renderer::
  render (const editor_state& s)
  {
    (void)s;

    // For the foundation, we just clear the screen to a dark editor-like
    // background color. This proves our render loop and context are wired
    // up correctly.
    //
    glClearColor (0.1f, 0.1f, 0.12f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT);

    // TODO: Build and submit geometry for the text and cursor based on 's'.
    //
  }

  void window_renderer::
  resize (int w, int h)
  {
    MINE_PRECONDITION (w >= 0 && h >= 0);

    // Tells the driver how to map the (-1, 1) normalized device
    // coordinates to the physical window pixels.
    //
    glViewport (0, 0, w, h);
  }
}

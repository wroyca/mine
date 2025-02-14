#pragma once

#include <adwaita.h>

#include <epoxy/gl.h>
#include <epoxy/glx.h>

#include <mine/mine-window.hxx>

namespace mine
{
  // GTK window implementation.
  //
  // Provides modern GTK4 window with hardware-accelerated OpenGL rendering.
  // Context is configured for OpenGL 2.0 or OpenGL ES 2.0, whichever is
  // available.
  //
  // Note: GTK operations must be performed from main thread.
  //
  class gtk : public window
  {
  public:
    // Create GTK window.
    //
    // Initializes Adwaita and creates GTK application with default flags and
    // dummy application ID.
    //
    // Does not show window; that happens when run() is called.
    //
    explicit
    gtk () noexcept;

    // Destroy GTK window.
    //
    // Releases GTK resources, including application object.
    //
    ~gtk () noexcept override;

    // Run GTK event loop.
    //
    // Creates and shows window, then runs GTK main loop. Blocks until
    // window is closed.
    //
    // Throws std::runtime_error if GTK application fail to initialize.
    //
    void
    run () override;

    // Queue content redraw.
    //
    // Requests redraw of OpenGL area. Actual rendering happens
    // asynchronously in render_cb() callback.
    //
    // Safe to call from any thread.
    //
    void
    render ();

  private:
    // GTK signal callbacks.
    //
    // Static callbacks connected to GTK signals. Receive window instance
    // through user_data parameter.
    //
    static void
    activate_cb (GtkApplication*, gpointer);

    static void
    realize_cb (GtkGLArea*, gpointer);

    static void
    render_cb (GtkGLArea*, GdkGLContext*, gpointer);

    static void
    unrealize_cb (GtkGLArea*, gpointer);

  private:
    AdwApplication* app_;
    AdwApplicationWindow* win_;
    GtkGLArea* gl_area_;
  };
}

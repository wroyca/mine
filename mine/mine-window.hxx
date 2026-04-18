#pragma once

#include <string>
#include <utility>

#include <mine/mine-types.hxx>

// Forward declare GLFW handle.
//
struct GLFWwindow;

namespace mine
{
  // Application window.
  //
  // This wraps the GLFW window creation and the basic event pump. Actually,
  // we assume OpenGL as the backing API here since we already specified it
  // in our rendering traits.
  //
  class window
  {
  public:
    // We pass the title as a string reference rather than a string_view.
    //
    // The reason is that GLFW expects a null-terminated C-string under the
    // hood, so a string_view could easily trip us up if we aren't careful.
    //
    window (int w, int h, const std::string& t);
    ~window ();

    // Move-only semantics.
    //
    // A window represents a unique physical resource on the screen, so copying
    // it makes absolutely no sense.
    //
    window (window&& x) noexcept;
    window& operator= (window&& x) noexcept;

    window (const window&) = delete;
    window& operator= (const window&) = delete;

    // Pump the OS event queue.
    //
    // Note that we return false if the user clicked the close button or the OS
    // requested termination.
    //
    bool
    update () const;

    // Swap the front and back buffers.
    //
    // Just a straightforward pass-through to the underlying GLFW swap call.
    //
    void
    swap_buffers () const;

    // Query the current framebuffer size.
    //
    // Note that this might actually differ from the requested window size on
    // high-DPI displays (like Apple Retina). We return a pair of integers
    // here since OpenGL naturally expects raw pixel dimensions.
    //
    std::pair<int, int>
    framebuffer_size () const;

    // Check if the window is flagged to close.
    //
    // Useful if we need to query the state outside of the standard update pump.
    //
    bool
    closing () const;

    // Access the raw GLFW handle.
    //
    // Required for registering external subsystems (like our input router)
    // without polluting this class with every possible callback hook.
    //
    GLFWwindow*
    handle () const { return w_; }

  private:
    GLFWwindow* w_ {nullptr};
  };
}

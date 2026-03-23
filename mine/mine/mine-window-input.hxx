#pragma once

#include <functional>

#include <mine/mine-terminal-input.hxx>

struct GLFWwindow;

namespace mine
{
  // Window Input Router.
  //
  // Sits between the raw GLFW event pump and the editor logic.
  // We explicitly segregate terminal-compatible inputs (key presses, clicks)
  // from GUI-exclusive inputs (high-resolution smooth scrolling).
  //
  class window_input
  {
  public:
    using event_cb  = std::function<void (input_event)>;
    using scroll_cb = std::function<void (double, double)>;
    using mouse_cb  = std::function<void (double, double, mouse_state, key_modifier)>;

    window_input (GLFWwindow* w, event_cb ecb, scroll_cb scb, mouse_cb mcb);
    ~window_input ();

    window_input (const window_input&) = delete;
    window_input& operator= (const window_input&) = delete;

  private:
    static void
    on_key (GLFWwindow* w, int k, int s, int a, int m);

    static void
    on_char (GLFWwindow* w, unsigned int cp);

    static void
    on_scroll (GLFWwindow* w, double x, double y);

    static void
    on_mouse_button (GLFWwindow* w, int b, int a, int m);

    static void
    on_cursor_pos (GLFWwindow* w, double x, double y);

  private:
    GLFWwindow* w_ {nullptr};
    event_cb    ecb_;
    scroll_cb   scb_;
    mouse_cb    mcb_;

    // Mouse tracking state.
    //
    double mx_   {0.0};
    double my_   {0.0};
    bool   drag_ {false};
  };
}

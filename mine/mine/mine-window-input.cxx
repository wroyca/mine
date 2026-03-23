#include <mine/mine-window-input.hxx>
#include <mine/mine-assert.hxx>

#include <GLFW/glfw3.h>

#include <string>
#include <utility>

using namespace std;

namespace mine
{
  namespace
  {
    // UTF-8 Encoder.
    //
    string
    encode_utf8 (uint32_t cp)
    {
      string r;

      if (cp <= 0x7F)
      {
        r.push_back (static_cast<char> (cp));
      }
      else if (cp <= 0x7FF)
      {
        r.push_back (static_cast<char> (0xC0 | ((cp >> 6) & 0x1F)));
        r.push_back (static_cast<char> (0x80 | (cp & 0x3F)));
      }
      else if (cp <= 0xFFFF)
      {
        r.push_back (static_cast<char> (0xE0 | ((cp >> 12) & 0x0F)));
        r.push_back (static_cast<char> (0x80 | ((cp >> 6) & 0x3F)));
        r.push_back (static_cast<char> (0x80 | (cp & 0x3F)));
      }
      else if (cp <= 0x10FFFF)
      {
        r.push_back (static_cast<char> (0xF0 | ((cp >> 18) & 0x07)));
        r.push_back (static_cast<char> (0x80 | ((cp >> 12) & 0x3F)));
        r.push_back (static_cast<char> (0x80 | ((cp >> 6) & 0x3F)));
        r.push_back (static_cast<char> (0x80 | (cp & 0x3F)));
      }

      return r;
    }
  }

  window_input::
  window_input (GLFWwindow* w, event_cb ecb, scroll_cb scb, mouse_cb mcb)
    : w_ (w),
      ecb_ (std::move (ecb)),
      scb_ (std::move (scb)),
      mcb_ (std::move (mcb))
  {
    MINE_PRECONDITION (w_ != nullptr);
    MINE_PRECONDITION (ecb_ != nullptr);
    MINE_PRECONDITION (scb_ != nullptr);
    MINE_PRECONDITION (mcb_ != nullptr);

    glfwSetWindowUserPointer (w_, this);

    glfwSetKeyCallback (w_, on_key);
    glfwSetCharCallback (w_, on_char);
    glfwSetScrollCallback (w_, on_scroll);
    glfwSetMouseButtonCallback (w_, on_mouse_button);
    glfwSetCursorPosCallback (w_, on_cursor_pos);
  }

  window_input::
  ~window_input ()
  {
    if (w_ != nullptr)
    {
      glfwSetKeyCallback (w_, nullptr);
      glfwSetCharCallback (w_, nullptr);
      glfwSetScrollCallback (w_, nullptr);
      glfwSetMouseButtonCallback (w_, nullptr);
      glfwSetCursorPosCallback (w_, nullptr);
      glfwSetWindowUserPointer (w_, nullptr);
    }
  }

  void window_input::
  on_key (GLFWwindow* w, int k, int /*s*/, int a, int m)
  {
    auto* self (static_cast<window_input*> (glfwGetWindowUserPointer (w)));
    MINE_INVARIANT (self != nullptr);

    if (a == GLFW_RELEASE)
      return;

    key_modifier mod (key_modifier::none);

    if (m & GLFW_MOD_SHIFT)   mod = mod | key_modifier::shift;
    if (m & GLFW_MOD_CONTROL) mod = mod | key_modifier::ctrl;
    if (m & GLFW_MOD_ALT)     mod = mod | key_modifier::alt;

    if ((m & GLFW_MOD_CONTROL) && k >= GLFW_KEY_A && k <= GLFW_KEY_Z)
    {
      char c (static_cast<char> ('a' + (k - GLFW_KEY_A)));
      string s (1, c);

      self->ecb_ (text_input_event {std::move (s), mod});
      return;
    }

    special_key sk (special_key::unknown);

    switch (k)
    {
      case GLFW_KEY_UP:        sk = special_key::up;         break;
      case GLFW_KEY_DOWN:      sk = special_key::down;       break;
      case GLFW_KEY_LEFT:      sk = special_key::left;       break;
      case GLFW_KEY_RIGHT:     sk = special_key::right;      break;
      case GLFW_KEY_HOME:      sk = special_key::home;       break;
      case GLFW_KEY_END:       sk = special_key::end;        break;
      case GLFW_KEY_PAGE_UP:   sk = special_key::page_up;    break;
      case GLFW_KEY_PAGE_DOWN: sk = special_key::page_down;  break;
      case GLFW_KEY_BACKSPACE: sk = special_key::backspace;  break;
      case GLFW_KEY_DELETE:    sk = special_key::delete_key; break;
      case GLFW_KEY_ENTER:     sk = special_key::enter;      break;
      case GLFW_KEY_TAB:       sk = special_key::tab;        break;
      case GLFW_KEY_ESCAPE:    sk = special_key::escape;     break;
      default:                                               break;
    }

    if (sk != special_key::unknown)
      self->ecb_ (special_key_event {sk, mod});
  }

  void window_input::
  on_char (GLFWwindow* w, unsigned int cp)
  {
    auto* self (static_cast<window_input*> (glfwGetWindowUserPointer (w)));
    MINE_INVARIANT (self != nullptr);

    string s (encode_utf8 (cp));
    self->ecb_ (text_input_event {std::move (s), key_modifier::none});
  }

  void window_input::
  on_scroll (GLFWwindow* w, double x, double y)
  {
    auto* self (static_cast<window_input*> (glfwGetWindowUserPointer (w)));
    MINE_INVARIANT (self != nullptr);

    self->scb_ (x, y);
  }

  void window_input::
  on_mouse_button (GLFWwindow* w, int b, int a, int m)
  {
    auto* self (static_cast<window_input*> (glfwGetWindowUserPointer (w)));
    MINE_INVARIANT (self != nullptr);

    if (b == GLFW_MOUSE_BUTTON_LEFT)
    {
      key_modifier mod (key_modifier::none);

      if (m & GLFW_MOD_SHIFT)   mod = mod | key_modifier::shift;
      if (m & GLFW_MOD_CONTROL) mod = mod | key_modifier::ctrl;
      if (m & GLFW_MOD_ALT)     mod = mod | key_modifier::alt;

      if (a == GLFW_PRESS)
      {
        self->drag_ = true;
        self->mcb_ (self->mx_, self->my_, mouse_state::press, mod);
      }
      else if (a == GLFW_RELEASE)
      {
        self->drag_ = false;
        self->mcb_ (self->mx_, self->my_, mouse_state::release, mod);
      }
    }
  }

  void window_input::
  on_cursor_pos (GLFWwindow* w, double x, double y)
  {
    auto* self (static_cast<window_input*> (glfwGetWindowUserPointer (w)));
    MINE_INVARIANT (self != nullptr);

    self->mx_ = x;
    self->my_ = y;

    if (self->drag_)
      self->mcb_ (x, y, mouse_state::drag, key_modifier::none);
  }
}

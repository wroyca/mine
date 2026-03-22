#pragma once

#include <string>

namespace mine
{
  void
  set_clipboard_text (const std::string& text);

  std::string
  get_clipboard_text ();
}

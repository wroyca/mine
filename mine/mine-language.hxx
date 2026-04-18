#pragma once

#include <string>
#include <string_view>
#include <functional>

namespace mine
{
  // A simple wrapper around the language name. We use an empty string to
  // signify the unknown or unset state so we don't have to drag std::optional
  // into this or pay for extra flags.
  //
  class language
  {
  public:
    // Default-construct to the unknown state.
    //
    language () : name_ () {}

    // We take it by value and move since we are going to store it anyway.
    //
    explicit
    language (std::string n) : name_ (std::move (n)) {}

    // Return the sentinel representing an unrecognized language.
    //
    static language
    unknown () { return language (); }

    const std::string&
    name () const noexcept { return name_; }

    bool
    is_unknown () const noexcept { return name_.empty (); }

    bool
    operator== (const language&) const = default;

    auto
    operator<=> (const language& o) const { return name_ <=> o.name_; }

  private:
    std::string name_;
  };

  // Try to figure out the language from the file path.
  //
  language
  detect_language (std::string_view p);

  // Map a raw extension to a language. Make sure the extension does not
  // include the leading dot. We assume the matching to be case-insensitive.
  //
  language
  extension_to_language (std::string_view e);
}

template <>
struct std::hash<mine::language>
{
  std::size_t
  operator() (const mine::language& l) const noexcept
  {
    return std::hash<std::string> () (l.name ());
  }
};

#include <libmine/terminal/escape/parameter/numeric.hxx>

namespace mine::terminal::escape::parameter
{
  optional<numeric>
  numeric::from_string (const string& s)
  {
    // Empty string means no explicit value
    //
    if (s.empty ())
      return numeric ();

    uint16_t val;
    auto [ptr, err] = from_chars (s.data (), s.data () + s.size (), val);

    // Verify entire string was consumed and parse succeeded
    //
    if (err == errc () && ptr == s.data () + s.size ())
      return numeric (val);

    return nullopt;
  }

  string numeric::
  to_string () const
  {
    return value ? std::to_string (*value) : string ();
  }
}

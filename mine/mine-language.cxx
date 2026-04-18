#include <mine/mine-language.hxx>

#include <algorithm>
#include <unordered_map>

using namespace std;

namespace mine
{
  static const unordered_map<string, language>&
  extension_map ()
  {
    static const unordered_map<string, language> m
    {
      {"c",    language ("c")},
      {"h",    language ("c")},
      {"cpp",  language ("cpp")},
      {"cxx",  language ("cpp")},
      {"cc",   language ("cpp")},
      {"hxx",  language ("cpp")},
      {"hpp",  language ("cpp")},
      {"hh",   language ("cpp")},
      {"ixx",  language ("cpp")},
      {"txx",  language ("cpp")},
    };

    return m;
  }

  language
  extension_to_language (string_view extension)
  {
    // Normalize to lowercase for case-insensitive matching.
    //
    string ext (extension);
    transform (ext.begin (),
               ext.end (),
               ext.begin (),
               [] (unsigned char c)
    {
      return static_cast<char> (tolower (c));
    });

    auto it (extension_map ().find (ext));
    if (it != extension_map ().end ())
      return it->second;

    return language::unknown ();
  }

  language
  detect_language (string_view path)
  {
    // Find the last dot to extract the extension. We search from the end to
    // handle paths like "foo.test.cxx" correctly (extension = "cxx").
    //
    auto dot (path.rfind ('.'));

    if (dot == string_view::npos)
      return language::unknown ();

    // A dot at position 0 with no further dots is a dotfile (e.g.,
    // ".gitignore"), not an extension.
    //
    auto ext (path.substr (dot + 1));
    if (ext.empty ())
      return language::unknown ();

    // Strip any trailing path separators that might have snuck in.
    //
    // Also isolate the filename component: we only want the part after the
    // last separator so that directory names with dots don't confuse us.
    //
    auto sep (path.rfind ('/'));

    #ifdef _WIN32
    auto bsep (path.rfind ('\\'));
    if (bsep != string_view::npos &&
        (sep == string_view::npos || bsep > sep))
      sep = bsep;
    #endif

    // If the dot is before the last separator, there's no extension in the
    // filename itself (e.g., "/foo.d/bar").
    //
    if (sep != string_view::npos && dot < sep)
      return language::unknown ();

    // If the dot is immediately after the separator, it's a dotfile.
    //
    if (sep != string_view::npos && dot == sep + 1 &&
        path.find ('.', dot + 1) == string_view::npos)
      return language::unknown ();

    return extension_to_language (ext);
  }
}

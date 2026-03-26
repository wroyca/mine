#include <mine/mine-utility.hxx>

#include <cstdlib>

#ifdef _WIN32
#  include <windows.h>
#  include <shlobj.h>
#endif

using namespace std;

namespace mine
{
  optional<filesystem::path>
  get_user_config_dir ()
  {
#ifdef _WIN32
    // Note that we use the older SHGetFolderPathA API here instead of
    // SHGetKnownFolderPath to maintain broader compatibility. We are looking
    // for the roaming application data directory.
    //
    char p[MAX_PATH];

    if (SUCCEEDED (SHGetFolderPathA (NULL, CSIDL_APPDATA, NULL, 0, p)))
      return filesystem::path (p) / "mine";

    return nullopt;
#else
    // See if the XDG config home environment variable is set.
    //
    const char* x (getenv ("XDG_CONFIG_HOME"));

    if (x && *x)
      return filesystem::path (x) / "mine";

    // Otherwise, fall back to the standard .config subdirectory in the user's
    // home.
    //
    const char* h (getenv ("HOME"));

    if (h && *h)
      return filesystem::path (h) / ".config" / "mine";

    return nullopt;
#endif
  }

  optional<filesystem::path>
  get_user_config_file ()
  {
    // Resolve the base configuration directory first. If we have one, the
    // initialization file is right inside.
    //
    auto d (get_user_config_dir ());

    if (d)
      return *d / "init.fnl";

    return nullopt;
  }
}

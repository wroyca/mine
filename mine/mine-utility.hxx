#pragma once

#include <filesystem>
#include <optional>

namespace mine
{
  extern bool build_installed;

  extern std::filesystem::path build_install_lib;       // $install.lib
  extern std::filesystem::path build_install_buildfile; // $install.buildfile
  extern std::filesystem::path build_install_data;      // $install.data
  extern std::filesystem::path build_install_root;      // $install.root

  extern std::filesystem::path build_install_root_relative;

  // Resolves the standard user configuration directory.
  //   Linux/macOS: $XDG_CONFIG_HOME/mine  (or ~/.config/mine)
  //   Windows:     %APPDATA%\mine
  //
  std::optional<std::filesystem::path>
  get_user_config_dir ();

  // Helper to directly resolve the main initialization file.
  //
  std::optional<std::filesystem::path>
  get_user_config_file ();
}

#pragma once

#include <filesystem>

namespace mine
{
  extern bool build_installed;

  extern std::filesystem::path build_install_lib;       // $install.lib
  extern std::filesystem::path build_install_buildfile; // $install.buildfile
  extern std::filesystem::path build_install_data;      // $install.data
  extern std::filesystem::path build_install_root;      // $install.root

  extern std::filesystem::path build_install_root_relative;
}

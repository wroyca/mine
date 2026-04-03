#pragma once

#include <filesystem>
#include <optional>

namespace mine
{
  std::optional<std::filesystem::path>
  get_user_config_dir ();

  std::optional<std::filesystem::path>
  get_user_config_file ();

  //

  extern bool build_installed;

  // Base installation directories.
  //

  extern std::filesystem::path build_install_root;
  extern std::filesystem::path build_install_root_relative;

  extern std::filesystem::path build_install_data_root;
  extern std::filesystem::path build_install_data_root_relative;

  extern std::filesystem::path build_install_exec_root;
  extern std::filesystem::path build_install_exec_root_relative;

  // Executables and libraries.
  //

  extern std::filesystem::path build_install_bin;
  extern std::filesystem::path build_install_bin_relative;

  extern std::filesystem::path build_install_sbin;
  extern std::filesystem::path build_install_sbin_relative;

  extern std::filesystem::path build_install_lib;
  extern std::filesystem::path build_install_lib_relative;

  extern std::filesystem::path build_install_libexec;
  extern std::filesystem::path build_install_libexec_relative;

  extern std::filesystem::path build_install_pkgconfig;
  extern std::filesystem::path build_install_pkgconfig_relative;

  // Architecture-independent data and configuration.
  //

  extern std::filesystem::path build_install_etc;
  extern std::filesystem::path build_install_etc_relative;

  extern std::filesystem::path build_install_include;
  extern std::filesystem::path build_install_include_relative;

  extern std::filesystem::path build_install_include_arch;
  extern std::filesystem::path build_install_include_arch_relative;

  extern std::filesystem::path build_install_share;
  extern std::filesystem::path build_install_share_relative;

  extern std::filesystem::path build_install_data;
  extern std::filesystem::path build_install_data_relative;

  extern std::filesystem::path build_install_buildfile;
  extern std::filesystem::path build_install_buildfile_relative;

  // Documentation and man pages.
  //

  extern std::filesystem::path build_install_doc;
  extern std::filesystem::path build_install_doc_relative;

  extern std::filesystem::path build_install_legal;
  extern std::filesystem::path build_install_legal_relative;

  extern std::filesystem::path build_install_man;
  extern std::filesystem::path build_install_man_relative;
}

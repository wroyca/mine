#include <mine/mine-utility.hxx>

#include <filesystem>

using namespace std;
using namespace filesystem;

namespace mine
{
  bool build_installed = true;

  // Base installation directories.
  //

#ifdef MINE_INSTALL_ROOT
  path build_install_root (MINE_INSTALL_ROOT);
#else
  path build_install_root;
#endif

#ifdef MINE_INSTALL_ROOT_RELATIVE
  path build_install_root_relative (MINE_INSTALL_ROOT_RELATIVE);
#else
  path build_install_root_relative;
#endif

#ifdef MINE_INSTALL_DATA_ROOT
  path build_install_data_root (MINE_INSTALL_DATA_ROOT);
#else
  path build_install_data_root;
#endif

#ifdef MINE_INSTALL_DATA_ROOT_RELATIVE
  path build_install_data_root_relative (MINE_INSTALL_DATA_ROOT_RELATIVE);
#else
  path build_install_data_root_relative;
#endif

#ifdef MINE_INSTALL_EXEC_ROOT
  path build_install_exec_root (MINE_INSTALL_EXEC_ROOT);
#else
  path build_install_exec_root;
#endif

#ifdef MINE_INSTALL_EXEC_ROOT_RELATIVE
  path build_install_exec_root_relative (MINE_INSTALL_EXEC_ROOT_RELATIVE);
#else
  path build_install_exec_root_relative;
#endif

  // Executables and libraries.
  //

#ifdef MINE_INSTALL_BIN
  path build_install_bin (MINE_INSTALL_BIN);
#else
  path build_install_bin;
#endif

#ifdef MINE_INSTALL_BIN_RELATIVE
  path build_install_bin_relative (MINE_INSTALL_BIN_RELATIVE);
#else
  path build_install_bin_relative;
#endif

#ifdef MINE_INSTALL_SBIN
  path build_install_sbin (MINE_INSTALL_SBIN);
#else
  path build_install_sbin;
#endif

#ifdef MINE_INSTALL_SBIN_RELATIVE
  path build_install_sbin_relative (MINE_INSTALL_SBIN_RELATIVE);
#else
  path build_install_sbin_relative;
#endif

#ifdef MINE_INSTALL_LIB
  path build_install_lib (MINE_INSTALL_LIB);
#else
  path build_install_lib;
#endif

#ifdef MINE_INSTALL_LIB_RELATIVE
  path build_install_lib_relative (MINE_INSTALL_LIB_RELATIVE);
#else
  path build_install_lib_relative;
#endif

#ifdef MINE_INSTALL_LIBEXEC
  path build_install_libexec (MINE_INSTALL_LIBEXEC);
#else
  path build_install_libexec;
#endif

#ifdef MINE_INSTALL_LIBEXEC_RELATIVE
  path build_install_libexec_relative (MINE_INSTALL_LIBEXEC_RELATIVE);
#else
  path build_install_libexec_relative;
#endif

#ifdef MINE_INSTALL_PKGCONFIG
  path build_install_pkgconfig (MINE_INSTALL_PKGCONFIG);
#else
  path build_install_pkgconfig;
#endif

#ifdef MINE_INSTALL_PKGCONFIG_RELATIVE
  path build_install_pkgconfig_relative (MINE_INSTALL_PKGCONFIG_RELATIVE);
#else
  path build_install_pkgconfig_relative;
#endif

  // Architecture-independent data and configuration.
  //

#ifdef MINE_INSTALL_ETC
  path build_install_etc (MINE_INSTALL_ETC);
#else
  path build_install_etc;
#endif

#ifdef MINE_INSTALL_ETC_RELATIVE
  path build_install_etc_relative (MINE_INSTALL_ETC_RELATIVE);
#else
  path build_install_etc_relative;
#endif

#ifdef MINE_INSTALL_INCLUDE
  path build_install_include (MINE_INSTALL_INCLUDE);
#else
  path build_install_include;
#endif

#ifdef MINE_INSTALL_INCLUDE_RELATIVE
  path build_install_include_relative (MINE_INSTALL_INCLUDE_RELATIVE);
#else
  path build_install_include_relative;
#endif

#ifdef MINE_INSTALL_INCLUDE_ARCH
  path build_install_include_arch (MINE_INSTALL_INCLUDE_ARCH);
#else
  path build_install_include_arch;
#endif

#ifdef MINE_INSTALL_INCLUDE_ARCH_RELATIVE
  path build_install_include_arch_relative (MINE_INSTALL_INCLUDE_ARCH_RELATIVE);
#else
  path build_install_include_arch_relative;
#endif

#ifdef MINE_INSTALL_SHARE
  path build_install_share (MINE_INSTALL_SHARE);
#else
  path build_install_share;
#endif

#ifdef MINE_INSTALL_SHARE_RELATIVE
  path build_install_share_relative (MINE_INSTALL_SHARE_RELATIVE);
#else
  path build_install_share_relative;
#endif

#ifdef MINE_INSTALL_DATA
  path build_install_data (MINE_INSTALL_DATA);
#else
  path build_install_data;
#endif

#ifdef MINE_INSTALL_DATA_RELATIVE
  path build_install_data_relative (MINE_INSTALL_DATA_RELATIVE);
#else
  path build_install_data_relative;
#endif

#ifdef MINE_INSTALL_BUILDFILE
  path build_install_buildfile (MINE_INSTALL_BUILDFILE);
#else
  path build_install_buildfile;
#endif

#ifdef MINE_INSTALL_BUILDFILE_RELATIVE
  path build_install_buildfile_relative (MINE_INSTALL_BUILDFILE_RELATIVE);
#else
  path build_install_buildfile_relative;
#endif

  // Documentation and man pages.
  //

#ifdef MINE_INSTALL_DOC
  path build_install_doc (MINE_INSTALL_DOC);
#else
  path build_install_doc;
#endif

#ifdef MINE_INSTALL_DOC_RELATIVE
  path build_install_doc_relative (MINE_INSTALL_DOC_RELATIVE);
#else
  path build_install_doc_relative;
#endif

#ifdef MINE_INSTALL_LEGAL
  path build_install_legal (MINE_INSTALL_LEGAL);
#else
  path build_install_legal;
#endif

#ifdef MINE_INSTALL_LEGAL_RELATIVE
  path build_install_legal_relative (MINE_INSTALL_LEGAL_RELATIVE);
#else
  path build_install_legal_relative;
#endif

#ifdef MINE_INSTALL_MAN
  path build_install_man (MINE_INSTALL_MAN);
#else
  path build_install_man;
#endif

#ifdef MINE_INSTALL_MAN_RELATIVE
  path build_install_man_relative (MINE_INSTALL_MAN_RELATIVE);
#else
  path build_install_man_relative;
#endif
}

#include <mine/mine-utility.hxx>

#include <filesystem>

using namespace std::filesystem;

namespace mine
{
  bool build_installed = true;

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

#ifdef MINE_INSTALL_DATA
  path build_install_data (MINE_INSTALL_DATA);
#else
  path build_install_data;
#endif
}

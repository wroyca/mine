#include <mine/mine-utility.hxx>

#include <filesystem>

using namespace std::filesystem;

namespace mine
{
  bool build_installed = false;

#ifdef MINE_INSTALL_DATA
  path build_install_data (MINE_INSTALL_DATA);
#else
  path build_install_data;
#endif

  path build_install_root;
  path build_install_root_relative;
}

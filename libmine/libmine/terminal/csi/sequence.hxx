#pragma once

#include <libmine/terminal/escape/parameter/numeric.hxx>

namespace mine::terminal::csi
{
  using namespace
  mine::terminal::escape::parameter;

  // CSI sequence handler.
  //
  class sequence
  {
  public:
    static string
    create (char command, const vector<numeric>& params = {});
  };
}

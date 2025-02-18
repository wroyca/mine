#include <libmine/terminal/csi/cursor/motion.hxx>

namespace mine::terminal::csi::cursor
{
  string
  up (const numeric& n)
  {
    return sequence::create ('A', {n});
  }

  string
  down (const numeric& n)
  {
    return sequence::create ('B', {n});
  }

  string
  forward (const numeric& n)
  {
    return sequence::create ('C', {n});
  }

  string
  backward (const numeric& n)
  {
    return sequence::create ('D', {n});
  }

  string
  position (const numeric& line, const numeric& column)
  {
    return sequence::create ('H', {line, column});
  }

  string
  report_position ()
  {
    return sequence::create ('n', {numeric (6)});
  }
}

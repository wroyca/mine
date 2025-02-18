#pragma once

#include <libmine/terminal/csi/sequence.hxx>

namespace mine::terminal::csi::cursor
{
  // Cursor Up (CUU)
  //
  string
  up (const numeric& n = numeric ());

  // Cursor Down (CUD)
  //
  string
  down (const numeric& n = numeric ());

  // Cursor Forward (CUF)
  //
  string
  forward (const numeric& n = numeric ());

  // Cursor Backward (CUB)
  //
  string
  backward (const numeric& n = numeric ());

  // Cursor Position (CUP)
  //
  string
  position (const numeric& line = numeric (), const numeric& column = numeric ());

  // Device Status Report (DSR) - Request cursor position
  //
  string
  report_position ();
}

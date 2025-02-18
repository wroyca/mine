#include <libmine/terminal/csi/sequence.hxx>

namespace mine::terminal::csi
{
  string sequence::
  create (char command, const vector<numeric>& params)
  {
    string s ("\033["); // ESC [

    // Add parameters
    //
    for (size_t i (0); i < params.size (); ++i)
    {
      if (i > 0)
        s += ';';
      s += params[i].to_string ();
    }

    // Add command and return sequence.
    //
    return s += command, s;
  }
}

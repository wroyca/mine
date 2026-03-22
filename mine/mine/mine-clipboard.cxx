#include <mine/mine-clipboard.hxx>

#include <array>
#include <cstdio>
#include <memory>
#include <string>

#if defined(_WIN32)
#  include <windows.h>
#endif

using namespace std;

namespace mine
{
#if defined(_WIN32)

  void
  set_clipboard_text (const string& t)
  {
    if (!OpenClipboard (nullptr))
      return;

    EmptyClipboard ();

    // Convert UTF-8 to UTF-16 since the Windows clipboard expects wide
    // characters. We first query the required buffer size.
    //
    int w (MultiByteToWideChar (CP_UTF8, 0, t.c_str (), -1, nullptr, 0));
    if (w > 0)
    {
      HGLOBAL h (GlobalAlloc (GMEM_MOVEABLE, w * sizeof (wchar_t)));
      if (h)
      {
        wchar_t* p (static_cast<wchar_t*> (GlobalLock (h)));
        MultiByteToWideChar (CP_UTF8, 0, t.c_str (), -1, p, w);
        GlobalUnlock (h);
        SetClipboardData (CF_UNICODETEXT, h);
      }
    }

    CloseClipboard ();
  }

  string
  get_clipboard_text ()
  {
    string r;
    if (!OpenClipboard (nullptr))
      return r;

    HANDLE d (GetClipboardData (CF_UNICODETEXT));
    if (d)
    {
      wchar_t* p (static_cast<wchar_t*> (GlobalLock (d)));
      if (p)
      {
        int n (WideCharToMultiByte (CP_UTF8,
                                    0,
                                    p,
                                    -1,
                                    nullptr,
                                    0,
                                    nullptr,
                                    nullptr));
        if (n > 0)
        {
          r.resize_and_overwrite (n - 1,
                                  [p, n] (char* b, size_t)
          {
            WideCharToMultiByte (CP_UTF8, 0, p, -1, b, n, nullptr, nullptr);
            return n - 1;
          });
        }
        GlobalUnlock (d);
      }
    }
    CloseClipboard ();

    // Windows applications love to sneak carriage returns in, so we sanitize to
    // pure LF.
    //
    erase (r, '\r');
    return r;
  }

#elif defined(__APPLE__)

  void
  set_clipboard_text (const string& t)
  {
    // Wrap the pipe in a unique pointer so we do not leak the file descriptor
    // if something unexpected happens.
    //
    unique_ptr<FILE, decltype (&pclose)> p (popen ("pbcopy", "w"), &pclose);
    if (p)
      fwrite (t.data (), 1, t.size (), p.get ());
  }

  string
  get_clipboard_text ()
  {
    string r;

    unique_ptr<FILE, decltype (&pclose)> p (popen ("pbpaste", "r"), &pclose);

    if (p)
    {
      array<char, 4096> b;

      // Read chunks from the pipe until we hit EOF. The unique_ptr guarantees
      // our pipe is closed even if the string append throws a bad_alloc.
      //
      while (true)
      {
        size_t n (fread (b.data (), 1, b.size (), p.get ()));
        if (n == 0)
          break;

        r.append (b.data (), n);
      }
    }
    return r;
  }

#else

  // Linux / BSD (Wayland via wl-clipboard).
  //
  void
  set_clipboard_text (const string& t)
  {
    unique_ptr<FILE, decltype (&pclose)> p (popen ("wl-copy", "w"), &pclose);
    if (p)
      fwrite (t.data (), 1, t.size (), p.get ());
  }

  string
  get_clipboard_text ()
  {
    string r;

    // Use -n to prevent wl-paste from appending an artificial newline to
    // the output.
    //
    unique_ptr<FILE, decltype (&pclose)> p (popen ("wl-paste -n", "r"),
                                            &pclose);

    if (p)
    {
      array<char, 256> b;
      while (true)
      {
        size_t n (fread (b.data (), 1, b.size (), p.get ()));
        if (n == 0)
          break;

        r.append (b.data (), n);
      }
    }

    // Just like on Windows, strip carriage returns in case the source app
    // included them. We prefer strict LF everywhere.
    //
    erase (r, '\r');
    return r;
  }

#endif
}

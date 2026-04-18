#include <mine/mine-syntax.hxx>
#include <mine/mine-utility.hxx>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>

using namespace std;
using namespace std::filesystem;

namespace mine
{
  namespace
  {
    // Map the raw string capture name from Tree-sitter to our abstract
    // token enumeration. This translates standard Neovim capture names.
    //
    syntax_token_type
    map_capture_name (string_view n)
    {
      if (n.starts_with ("keyword") || n.starts_with ("conditional") ||
          n.starts_with ("repeat")  || n.starts_with ("include"))
        return syntax_token_type::keyword;

      if (n.starts_with ("string") || n.starts_with ("character"))
        return syntax_token_type::string;

      if (n.starts_with ("type"))
        return syntax_token_type::type;

      if (n.starts_with ("function") || n.starts_with ("method"))
        return syntax_token_type::function;

      if (n.starts_with ("variable") || n.starts_with ("property") ||
          n.starts_with ("parameter"))
        return syntax_token_type::variable;

      if (n.starts_with ("constant") || n.starts_with ("number") ||
          n.starts_with ("boolean"))
        return syntax_token_type::constant;

      if (n.starts_with ("comment"))
        return syntax_token_type::comment;

      return syntax_token_type::none;
    }

    // Helper to abstract dynamic library unloading across platforms.
    //
    void
    close_lib (void* handle)
    {
      if (!handle)
        return;

#ifdef _WIN32
      FreeLibrary (static_cast<HMODULE> (handle));
#else
      dlclose (handle);
#endif
    }
  }

  // syntax_highlighter::dl_guard
  //

  syntax_highlighter::dl_guard::
  ~dl_guard ()
  {
    close_lib (handle);
  }

  syntax_highlighter::dl_guard::
  dl_guard (void* h)
    : handle (h)
  {
  }

  syntax_highlighter::dl_guard::
  dl_guard (dl_guard&& x) noexcept
    : handle (exchange (x.handle, nullptr))
  {
  }

  syntax_highlighter::dl_guard& syntax_highlighter::dl_guard::
  operator= (dl_guard&& x) noexcept
  {
    if (handle)
      close_lib (handle);

    handle = exchange (x.handle, nullptr);
    return *this;
  }

  // syntax_highlighter
  //

  syntax_highlighter::
  syntax_highlighter ()
  {
  }

  syntax_highlighter::
  ~syntax_highlighter ()
  {
  }

  syntax_highlighter::
  syntax_highlighter (syntax_highlighter&& x) noexcept
    : dl_handle_ (move (x.dl_handle_)),
      lang_ (exchange (x.lang_, nullptr)),
      parser_ (move (x.parser_)),
      query_ (move (x.query_)),
      tree_ (move (x.tree_)),
      last_buffer_ (move (x.last_buffer_))
  {
  }

  syntax_highlighter& syntax_highlighter::
  operator= (syntax_highlighter&& x) noexcept
  {
    if (this != &x)
    {
      this->~syntax_highlighter ();
      new (this) syntax_highlighter (move (x));
    }
    return *this;
  }

  void syntax_highlighter::
  init ()
  {
    // We load the parser dynamically from the data path.
    //
    string lang ("cpp"); // Hardcoded to C++ for now to wire up tree-sitter.

#ifdef _WIN32
    string ext (".dll");
#elif defined(__APPLE__)
    string ext (".dylib");
#else
    string ext (".so");
#endif

    path parser (build_install_data / "parser" / (lang + ext));

#ifdef _WIN32
    void* handle (
      static_cast<void*> (LoadLibraryW (parser.wstring ().c_str ())));
#else
    void* handle (dlopen (parser.string ().c_str (), RTLD_NOW));
#endif

    dl_handle_ = dl_guard (handle);

    if (dl_handle_.handle == nullptr)
      return;

    // Resolve the entry point. The tree-sitter convention is a function
    // named tree_sitter_<language>.
    //
    string func_name = "tree_sitter_" + lang;

#ifdef _WIN32
    auto* sym (reinterpret_cast<void*> (
      GetProcAddress (static_cast<HMODULE> (dl_handle_.handle),
                      func_name.c_str ())));
#else
    auto* sym (dlsym (dl_handle_.handle, func_name.c_str ()));
#endif

    if (sym == nullptr)
      return;

    using ts_func = ts_language (*) ();
    auto* func (reinterpret_cast<ts_func> (sym));

    lang_ = func ();
    parser_.set_language (lang_);

    // Load the queries from the file system.
    //
    path query (build_install_data / "queries" / lang / "highlights.scm");
    ifstream f (query);

    if (f)
    {
      ostringstream ss;
      ss << f.rdbuf ();
      query_.emplace (lang_, ss.str ());
    }
  }

  void syntax_highlighter::
  update (const content& b)
  {
    if (lang_ == nullptr)
      return;

    // Fast path: if the buffer hasn't changed since the last time we
    // parsed it, we don't need to do anything. content uses structural
    // sharing, so this is a very cheap equality check.
    //
    if (b == last_buffer_)
      return;

    last_buffer_ = b;

    // Parse the buffer from scratch.
    //
    // Since we are not computing TSInputEdit right now, we pass nullptr
    // for the old tree to force a full re-parse.
    //
    tree_ = parser_.parse_buffer (b, nullptr);
  }

  vector<highlight_span> syntax_highlighter::
  query_lines (size_t start_line, size_t end_line) const
  {
    vector<highlight_span> res;

    if (!tree_ || !query_)
      return res;

    syntax_query_cursor cur;
    cur.exec (*query_, tree_->root_node ());

    // Restrict the query to the exact lines we care about rendering.
    //
    TSPoint start { static_cast<uint32_t> (start_line), 0 };
    TSPoint end   { static_cast<uint32_t> (end_line), 0 };
    cur.set_point_range (start, end);

    TSQueryMatch m;

    while (cur.next_match (m))
    {
      // A match can have multiple captures. We iterate through them and
      // extract the boundary points.
      //
      for (uint16_t i (0); i < m.capture_count; ++i)
      {
        const auto& c (m.captures[i]);
        TSNode n (c.node);

        string_view name (query_->capture_name_for_id (c.index));
        syntax_token_type type (map_capture_name (name));

        if (type != syntax_token_type::none)
        {
          TSPoint sp (ts_node_start_point (n));
          TSPoint ep (ts_node_end_point (n));

          highlight_span s;
          s.start_line     = sp.row;
          s.start_col_byte = sp.column;
          s.end_line       = ep.row;
          s.end_col_byte   = ep.column;
          s.type           = type;

          res.push_back (move (s));
        }
      }
    }

    return res;
  }
}

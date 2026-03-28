#include <mine/mine-treesitter.hxx>
#include <mine/mine-contract.hxx>

#include <utility>

using namespace std;

namespace mine
{
  // syntax_tree
  //

  syntax_tree::
  syntax_tree (TSTree* t)
    : tree_ (t)
  {
  }

  syntax_tree::
  ~syntax_tree ()
  {
    if (tree_ != nullptr)
      ts_tree_delete (tree_);
  }

  syntax_tree::
  syntax_tree (const syntax_tree& x)
    : tree_ (x.tree_ != nullptr ? ts_tree_copy (x.tree_) : nullptr)
  {
  }

  syntax_tree& syntax_tree::
  operator= (const syntax_tree& x)
  {
    if (this != &x)
    {
      if (tree_ != nullptr)
        ts_tree_delete (tree_);

      tree_ = (x.tree_ != nullptr ? ts_tree_copy (x.tree_) : nullptr);
    }
    return *this;
  }

  syntax_tree::
  syntax_tree (syntax_tree&& x) noexcept
    : tree_ (exchange (x.tree_, nullptr))
  {
  }

  syntax_tree& syntax_tree::
  operator= (syntax_tree&& x) noexcept
  {
    if (this != &x)
    {
      if (tree_ != nullptr)
        ts_tree_delete (tree_);

      tree_ = exchange (x.tree_, nullptr);
    }
    return *this;
  }

  TSNode syntax_tree::
  root_node () const noexcept
  {
    MINE_PRECONDITION (tree_ != nullptr);
    return ts_tree_root_node (tree_);
  }

  void syntax_tree::
  edit (const TSInputEdit& e) noexcept
  {
    MINE_PRECONDITION (tree_ != nullptr);
    ts_tree_edit (tree_, &e);
  }

  // syntax_parser
  //

  syntax_parser::
  syntax_parser ()
    : parser_ (ts_parser_new ())
  {
    MINE_INVARIANT (parser_ != nullptr);
  }

  syntax_parser::
  ~syntax_parser ()
  {
    if (parser_ != nullptr)
      ts_parser_delete (parser_);
  }

  syntax_parser::
  syntax_parser (syntax_parser&& x) noexcept
    : parser_ (exchange (x.parser_, nullptr))
  {
  }

  syntax_parser& syntax_parser::
  operator= (syntax_parser&& x) noexcept
  {
    if (this != &x)
    {
      if (parser_ != nullptr)
        ts_parser_delete (parser_);

      parser_ = exchange (x.parser_, nullptr);
    }
    return *this;
  }

  bool syntax_parser::
  set_language (ts_language l) noexcept
  {
    MINE_PRECONDITION (parser_ != nullptr);
    return ts_parser_set_language (parser_, l);
  }

  syntax_tree syntax_parser::
  parse_string (string_view s, const syntax_tree* old)
  {
    MINE_PRECONDITION (parser_ != nullptr);

    TSTree* t (ts_parser_parse_string (parser_,
                                       old != nullptr ? old->get () : nullptr,
                                       s.data (),
                                       static_cast<uint32_t> (s.size ())));

    return syntax_tree (t);
  }

  namespace
  {
    // Read a chunk of text from the buffer for the parser.
    //
    // Tree-sitter calls this whenever it needs more data. Fortunately for us,
    // it provides the exact logical coordinate (row and column) alongside the
    // raw byte index. This means we don't have to scan the buffer to figure
    // out where we are; we just jump straight to the requested line.
    //
    static const char*
    read_buffer_chunk (void* p,
                       uint32_t /*byte_index*/,
                       TSPoint pos,
                       uint32_t* read)
    {
      auto* b (static_cast<const text_buffer*> (p));

      // Tree-sitter might ask for data past the end of the file when it is
      // finishing up the parse. We just return an empty string to signal EOF.
      //
      if (pos.row >= b->line_count ())
      {
        *read = 0;
        return "";
      }

      const auto& l (b->line_at (line_number (pos.row)));
      auto v (l.view ());

      // If the requested column is within the line bounds, we hand over the
      // remaining characters. Note that because immer's flex_vector chunks
      // might be discontiguous, we rely on the line's cached string view.
      //
      if (pos.column < v.size ())
      {
        *read = static_cast<uint32_t> (v.size () - pos.column);
        return v.data () + pos.column;
      }

      // If the column exactly matches the line size, tree-sitter is looking
      // at the implicit newline character. Since we strip newlines when
      // loading the buffer, we have to fake it here.
      //
      if (pos.column == v.size ())
      {
        *read = 1;
        return "\n";
      }

      // We should never really hit this unless the parser gets horribly
      // confused about the buffer topology.
      //
      *read = 0;
      return "";
    }
  }

  syntax_tree syntax_parser::
  parse_buffer (const text_buffer& b, const syntax_tree* old)
  {
    MINE_PRECONDITION (parser_ != nullptr);

    // Set up the input payload.
    //
    // We cast away constness here because the C API expects a void pointer,
    // but our read_buffer_chunk callback safely casts it back to const.
    //
    TSInput in;
    in.payload = const_cast<text_buffer*> (&b);
    in.read = read_buffer_chunk;
    in.encoding = TSInputEncodingUTF8;

    TSTree* t (ts_parser_parse (parser_,
                                old != nullptr ? old->get () : nullptr,
                                in));

    return syntax_tree (t);
  }

  // syntax_query
  //

  syntax_query::
  syntax_query (ts_language l, string_view src)
  {
    uint32_t e_off;
    TSQueryError e_type;

    // Compile the query.
    //
    // If it fails, we just leave the query pointer as null.
    //
    // TODO: propagate this back to the user
    //
    query_ = ts_query_new (l,
                           src.data (),
                           static_cast<uint32_t> (src.size ()),
                           &e_off,
                           &e_type);
  }

  syntax_query::
  ~syntax_query ()
  {
    if (query_ != nullptr)
      ts_query_delete (query_);
  }

  syntax_query::
  syntax_query (syntax_query&& x) noexcept
    : query_ (exchange (x.query_, nullptr))
  {
  }

  syntax_query& syntax_query::
  operator= (syntax_query&& x) noexcept
  {
    if (this != &x)
    {
      if (query_ != nullptr)
        ts_query_delete (query_);

      query_ = exchange (x.query_, nullptr);
    }
    return *this;
  }

  string_view syntax_query::
  capture_name_for_id (uint32_t id) const noexcept
  {
    MINE_PRECONDITION (query_ != nullptr);
    uint32_t len (0);
    const char* n (ts_query_capture_name_for_id (query_, id, &len));
    return string_view (n, len);
  }

  // syntax_query_cursor
  //

  syntax_query_cursor::
  syntax_query_cursor ()
    : cursor_ (ts_query_cursor_new ())
  {
    MINE_INVARIANT (cursor_ != nullptr);
  }

  syntax_query_cursor::
  ~syntax_query_cursor ()
  {
    if (cursor_ != nullptr)
      ts_query_cursor_delete (cursor_);
  }

  syntax_query_cursor::
  syntax_query_cursor (syntax_query_cursor&& x) noexcept
    : cursor_ (exchange (x.cursor_, nullptr))
  {
  }

  syntax_query_cursor& syntax_query_cursor::
  operator= (syntax_query_cursor&& x) noexcept
  {
    if (this != &x)
    {
      if (cursor_ != nullptr)
        ts_query_cursor_delete (cursor_);

      cursor_ = exchange (x.cursor_, nullptr);
    }
    return *this;
  }

  void syntax_query_cursor::
  exec (const syntax_query& q, TSNode n) noexcept
  {
    MINE_PRECONDITION (cursor_ != nullptr);
    MINE_PRECONDITION (q.get () != nullptr);

    ts_query_cursor_exec (cursor_, q.get (), n);
  }

  void syntax_query_cursor::
  set_point_range (TSPoint start, TSPoint end) noexcept
  {
    MINE_PRECONDITION (cursor_ != nullptr);
    ts_query_cursor_set_point_range (cursor_, start, end);
  }

  bool syntax_query_cursor::
  next_match (TSQueryMatch& m) noexcept
  {
    MINE_PRECONDITION (cursor_ != nullptr);
    return ts_query_cursor_next_match (cursor_, &m);
  }
}

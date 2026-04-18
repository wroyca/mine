#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <optional>

#include <tree_sitter/api.h>

#include <mine/mine-content.hxx>
#include <mine/mine-types.hxx>

namespace mine
{
  // Tree-sitter language identifier.
  //
  // This is an opaque pointer returned by a language parser's entry point,
  // for example tree_sitter_cpp() or tree_sitter_rust().
  //
  using ts_language = const TSLanguage*;

  // A parsed syntax tree.
  //
  // Note that tree-sitter trees are generally immutable from the perspective
  // of the reader, but they participate in incremental parsing. When text is
  // changed, we first apply the edit to the old tree, and then pass it to the
  // parser to produce a new, structurally shared tree.
  //
  class syntax_tree
  {
  public:
    syntax_tree () = default;

    explicit
    syntax_tree (TSTree* t);

    ~syntax_tree ();

    // Copying a tree is cheap because Tree-sitter shares the underlying nodes
    // automatically.
    //
    syntax_tree (const syntax_tree& x);
    syntax_tree& operator= (const syntax_tree& x);

    syntax_tree (syntax_tree&& x) noexcept;
    syntax_tree& operator= (syntax_tree&& x) noexcept;

    // Retrieve the underlying C struct.
    //
    TSTree*
    get () const noexcept
    {
      return tree_;
    }

    // Get the root node of the syntax tree.
    //
    // This is the starting point for any traversal or query execution.
    //
    TSNode
    root_node () const noexcept;

    // Apply an edit to the tree.
    //
    // We must call this before passing the tree back into the parser for an
    // incremental update. It does not actually parse the new text, but rather
    // marks the affected byte ranges as dirty.
    //
    void
    edit (const TSInputEdit& e) noexcept;

    explicit operator bool () const noexcept
    {
      return tree_ != nullptr;
    }

  private:
    TSTree* tree_ {nullptr};
  };

  // The parser state machine.
  //
  // We keep a single parser instance around and feed it buffer chunks to
  // produce syntax trees. Reusing the parser across edits is highly
  // recommended by the Tree-sitter documentation because it caches various
  // internal scratch allocations.
  //
  class syntax_parser
  {
  public:
    syntax_parser ();
    ~syntax_parser ();

    syntax_parser (const syntax_parser&) = delete;
    syntax_parser& operator= (const syntax_parser&) = delete;

    syntax_parser (syntax_parser&& x) noexcept;
    syntax_parser& operator= (syntax_parser&& x) noexcept;

    // Assign the language grammar.
    //
    // This dictates how the parser will interpret the incoming text. Returns
    // false if there was a version mismatch with the loaded grammar.
    //
    bool
    set_language (ts_language l) noexcept;

    // Parse a contiguous string.
    //
    // Useful for small snippets or the command line prompt, but generally we
    // prefer parsing directly from the buffer for large files.
    //
    syntax_tree
    parse_string (std::string_view s, const syntax_tree* old = nullptr);

    // Parse directly from our immutable text buffer.
    //
    // This uses a custom payload reader to stream the flex_vector chunks
    // straight into Tree-sitter without ever flattening the file into a single
    // contiguous string.
    //
    syntax_tree
    parse_buffer (const content& b, const syntax_tree* old = nullptr);

  private:
    TSParser* parser_ {nullptr};
  };

  // Syntax query.
  //
  // A compiled S-expression query used to extract highlights, locals, or
  // folding ranges from the parsed syntax tree.
  //
  class syntax_query
  {
  public:
    syntax_query () = default;

    syntax_query (ts_language l, std::string_view source);
    ~syntax_query ();

    syntax_query (const syntax_query&) = delete;
    syntax_query& operator= (const syntax_query&) = delete;

    syntax_query (syntax_query&& x) noexcept;
    syntax_query& operator= (syntax_query&& x) noexcept;

    TSQuery*
    get () const noexcept
    {
      return query_;
    }

    // Retrieve the name of a capture by its internal ID.
    //
    // We use this to map raw string captures from the queries (like "keyword")
    // to our semantic enumerations.
    //
    std::string_view
    capture_name_for_id (std::uint32_t id) const noexcept;

    explicit operator bool () const noexcept
    {
      return query_ != nullptr;
    }

  private:
    TSQuery* query_ {nullptr};
  };

  // Syntax query cursor.
  //
  // State machine for executing a syntax query against a specific node in the
  // syntax tree and iterating over the matches.
  //
  class syntax_query_cursor
  {
  public:
    syntax_query_cursor ();
    ~syntax_query_cursor ();

    syntax_query_cursor (const syntax_query_cursor&) = delete;
    syntax_query_cursor& operator= (const syntax_query_cursor&) = delete;

    syntax_query_cursor (syntax_query_cursor&& x) noexcept;
    syntax_query_cursor& operator= (syntax_query_cursor&& x) noexcept;

    // Start executing the query against the given node.
    //
    void
    exec (const syntax_query& q, TSNode n) noexcept;

    // Restrict the query execution to a specific line and column range.
    //
    void
    set_point_range (TSPoint start, TSPoint end) noexcept;

    // Advance to the next match.
    //
    // Returns true if a match was found and populates m.
    //
    bool
    next_match (TSQueryMatch& m) noexcept;

  private:
    TSQueryCursor* cursor_ {nullptr};
  };
}

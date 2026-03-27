#pragma once

#include <string>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <mine/mine-treesitter.hxx>
#include <mine/mine-core-buffer.hxx>

namespace mine
{
  // The semantic meaning of a highlighted token.
  //
  // We keep this abstract so that both the terminal backend (which uses
  // indexed ANSI colors) and the OpenGL backend (which uses RGBA floats)
  // can map these to their respective visual styles without bleeding graphics
  // dependencies into the core.
  //
  enum class syntax_token_type : std::uint8_t
  {
    none,
    keyword,
    string,
    type,
    function,
    variable,
    constant,
    comment
  };

  // A region of text that should be styled.
  //
  // Notice that we use the Tree-sitter row/col points rather than pure byte
  // offsets. This maps perfectly to our per-line grapheme rendering loops.
  //
  struct highlight_span
  {
    std::size_t       start_line;
    std::size_t       start_col_byte;
    std::size_t       end_line;
    std::size_t       end_col_byte;
    syntax_token_type type;
  };

  // The syntax highlighter.
  //
  // This class acts as the bridge between our text buffer and the Tree-sitter
  // ecosystem. It dynamically loads the parser, maintains the syntax tree,
  // and executes queries to extract highlighting information.
  //
  class syntax_highlighter
  {
  public:
    syntax_highlighter ();
    ~syntax_highlighter ();

    syntax_highlighter (const syntax_highlighter&) = delete;
    syntax_highlighter& operator= (const syntax_highlighter&) = delete;

    syntax_highlighter (syntax_highlighter&& x) noexcept;
    syntax_highlighter& operator= (syntax_highlighter&& x) noexcept;

    // Initialize the highlighter.
    //
    // This will attempt to load the C++ parser and queries from the
    // hardcoded Neovim paths. If they are missing, it simply degrades
    // gracefully to plain text.
    //
    void
    init ();

    // Sync the syntax tree with the current buffer state.
    //
    // Because text_buffer uses structural sharing, checking if the buffer
    // changed is an O(1) pointer comparison. If it did, we re-parse.
    //
    void
    update (const text_buffer& b);

    // Execute the highlighting query over a specific line range.
    //
    // We return a flat list of spans. Overlapping spans are resolved by
    // the renderer during the sweep.
    //
    std::vector<highlight_span>
    query_lines (std::size_t start_line, std::size_t end_line) const;

  private:
    // We wrap the library handle in a tiny RAII structure. C++ destroys members
    // bottom-up, so this guarantees the library is unloaded *after* the Tree-sitter
    // objects have safely executed their destructors.
    //
    struct dl_guard
    {
      void* handle {nullptr};
      ~dl_guard ();
      dl_guard (void* h = nullptr);
      dl_guard (dl_guard&& x) noexcept;
      dl_guard& operator= (dl_guard&& x) noexcept;
    };

    dl_guard dl_handle_;
    ts_language lang_ {nullptr};

    syntax_parser parser_;
    std::optional<syntax_query> query_;
    std::optional<syntax_tree> tree_;

    text_buffer last_buffer_;
  };
}

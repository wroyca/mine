#pragma once

#include <stdexcept>
#include <string>
#include <cstdint>

namespace mine
{
  // Base exception for all domain errors in the editor.
  //
  class editor_error : public std::runtime_error
  {
  public:
    using std::runtime_error::runtime_error;
  };

  // Document-related errors.
  //

  class document_error : public editor_error
  {
  public:
    using editor_error::editor_error;
  };

  class document_not_found : public document_error
  {
  public:
    explicit
    document_not_found (std::uint32_t id)
      : document_error (
          "document not found: id=" + std::to_string (id)),
        id_ (id)
    {
    }

    std::uint32_t
    id () const noexcept { return id_; }

  private:
    std::uint32_t id_;
  };

  class invalid_content : public document_error
  {
  public:
    using document_error::document_error;
  };

  // Cursor-related errors.
  //

  class cursor_error : public editor_error
  {
  public:
    using editor_error::editor_error;
  };

  class cursor_out_of_range : public cursor_error
  {
  public:
    cursor_out_of_range (std::size_t line,
                         std::size_t column,
                         std::size_t max_lines)
      : cursor_error (
          "cursor out of range: line=" + std::to_string (line) +
          " column=" + std::to_string (column) +
          " max_lines=" + std::to_string (max_lines)),
        line_ (line),
        column_ (column),
        max_lines_ (max_lines)
    {
    }

    std::size_t line () const noexcept { return line_; }
    std::size_t column () const noexcept { return column_; }
    std::size_t max_lines () const noexcept { return max_lines_; }

  private:
    std::size_t line_;
    std::size_t column_;
    std::size_t max_lines_;
  };

  // Window-related errors.
  //

  class window_error : public editor_error
  {
  public:
    using editor_error::editor_error;
  };

  class window_not_found : public window_error
  {
  public:
    explicit
    window_not_found (std::uint32_t id)
      : window_error (
          "window not found: id=" + std::to_string (id)),
        id_ (id)
    {
    }

    std::uint32_t
    id () const noexcept { return id_; }

  private:
    std::uint32_t id_;
  };

  // File I/O errors.
  //

  class file_error : public editor_error
  {
  public:
    using editor_error::editor_error;
  };

  class file_read_error : public file_error
  {
  public:
    explicit
    file_read_error (const std::string& path, const std::string& reason = "")
      : file_error (
          "failed to read file: " + path +
          (reason.empty () ? "" : " (" + reason + ")")),
        path_ (path)
    {
    }

    const std::string&
    path () const noexcept { return path_; }

  private:
    std::string path_;
  };

  class file_write_error : public file_error
  {
  public:
    explicit
    file_write_error (const std::string& path, const std::string& reason = "")
      : file_error (
          "failed to write file: " + path +
          (reason.empty () ? "" : " (" + reason + ")")),
        path_ (path)
    {
    }

    const std::string&
    path () const noexcept { return path_; }

  private:
    std::string path_;
  };

  // VM / scripting errors.
  //

  class vm_error : public editor_error
  {
  public:
    using editor_error::editor_error;
  };

  // Syntax / parsing errors.
  //

  class syntax_parse_error : public editor_error
  {
  public:
    using editor_error::editor_error;
  };
}

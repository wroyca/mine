#pragma once

#include <compare>
#include <exception>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include <immer/box.hpp>
#include <immer/flex_vector.hpp>

#include <mine/mine-types.hxx>
#include <mine/mine-core-buffer.hxx>

namespace mine
{
  // File states for tracking load/save operations.
  //
  // We need to model the lifecycle of a file explicitly to handle the
  // asynchronous nature of I/O. It's not enough to just know *if* we are
  // loading; we need to know *what* we are loading so we can render
  // progress bars or handle race conditions where the user types while
  // we load.
  //
  // We use `immer::box` for strings so that copying state objects remains
  // cheap (pointer copy), which occurs frequently during the reduce cycle.
  //

  // The "New File" state.
  //
  // No backing file on disk exists yet.
  //
  struct no_file
  {
    immer::box<std::string> name = "*unnamed*";
    text_buffer content = {};

    bool operator== (const no_file&) const = default;
  };

  // The "Stable" state.
  //
  // The file exists on disk and, to the best of our knowledge, matches
  // what we last read/wrote (modulo subsequent edits in the buffer).
  //
  struct existing_file
  {
    immer::box<std::string> name;
    text_buffer content;

    bool operator== (const existing_file&) const = default;
  };

  // The "Loading" state.
  //
  // We hold onto the partial content here. This allows the editor to
  // display the file "streaming in" line by line if we choose to update
  // the UI during the load.
  //
  struct loading_file
  {
    immer::box<std::string> name;
    text_buffer content;
    std::size_t loaded_bytes;
    std::size_t total_bytes;

    bool operator== (const loading_file&) const = default;

    float
    progress_percent () const noexcept
    {
      return total_bytes == 0 ? 100.0f
                              : (static_cast<float> (loaded_bytes) /
                                 static_cast<float> (total_bytes)) * 100.0f;
    }
  };

  // The "Saving" state.
  //
  // We track saved lines vs total lines.
  //
  struct saving_file
  {
    immer::box<std::string> name;
    text_buffer content;
    std::size_t saved_lines;

    bool operator== (const saving_file&) const = default;

    float
    progress_percent (std::size_t total) const noexcept
    {
      return total == 0 ? 100.0f
                        : (static_cast<float> (saved_lines) /
                           static_cast<float> (total)) * 100.0f;
    }
  };

  // The sum type of all possible file states.
  //
  using file_state = std::variant<no_file,
                                  existing_file,
                                  loading_file,
                                  saving_file>;

  // Actions.
  //
  // These are the messages sent from the background I/O threads back to
  // the main thread. We use the "Elm Architecture" pattern where actions
  // carry the data necessary to transition to the next state.
  //

  struct load_progress_action { loading_file file; };
  struct load_done_action     { existing_file file; };

  struct load_error_action
  {
    existing_file file;
    std::exception_ptr error;
  };

  struct save_progress_action { saving_file file; };
  struct save_done_action     { existing_file file; };

  struct save_error_action
  {
    existing_file file;
    std::exception_ptr error;
  };

  using file_io_action = std::variant<load_progress_action,
                                      load_done_action,
                                      load_error_action,
                                      save_progress_action,
                                      save_done_action,
                                      save_error_action>;

  // The Model.
  //
  // This aggregates the buffer content with its metadata (state).
  //
  struct file_buffer
  {
    file_state state;
    text_buffer content;

    file_buffer ()
      : state (no_file {}),
        content (make_empty_buffer ())
    {
    }

    explicit file_buffer (text_buffer c)
      : state (no_file {}),
        content (std::move (c))
    {
    }

    file_buffer (file_state s, text_buffer c)
      : state (std::move (s)),
        content (std::move (c))
    {
    }

    // State queries.
    //
    // These look into the variant to determine high-level status.
    // implementation is usually in the .cxx to avoid header bloat with
    // std::visit calls.
    //
    bool
    is_dirty () const noexcept;

    bool
    io_in_progress () const noexcept;

    bool
    load_in_progress () const noexcept;

    bool
    save_in_progress () const noexcept;

    std::optional<std::string>
    file_name () const noexcept;

    std::optional<float>
    progress_percent () const noexcept;

    bool operator== (const file_buffer&) const = default;
  };

  // Effects.
  //
  // A side-effect is a function that accepts a dispatcher. The dispatcher
  // is how the effect feeds actions back into the system.
  //
  using file_io_effect =
    std::function<void (std::function<void (file_io_action)>)>;

  using io_effect = file_io_effect; // Alias for convenience.

  // Operations.
  //
  // These are pure functions that take a state and return a pair of
  // {new_state, effect}.
  //

  std::pair<file_buffer, file_io_effect>
  load_file (file_buffer b, const std::string& name);

  std::pair<file_buffer, file_io_effect>
  save_file (file_buffer b);

  // The reducer for I/O actions.
  //
  // Returns the new buffer and an optional status message (e.g., "Saved",
  // "Error: ...") for the UI to display.
  //
  std::pair<file_buffer, std::optional<std::string>>
  update_file_buffer (file_buffer b, file_io_action a);
}

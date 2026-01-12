#include <mine/mine-io-file.hxx>

#include <algorithm>
#include <exception>
#include <fstream>
#include <sstream>
#include <thread>

#include <immer/algorithm.hpp>
#include <immer/flex_vector_transient.hpp>

#include <mine/mine-assert.hxx>

using namespace std;

namespace mine
{
  // Query operations.
  //

  bool file_buffer::
  is_dirty () const noexcept
  {
    // We are dirty if the current in-memory content differs from what we
    // believe is on disk (represented by the state's content).
    //
    return visit ([&] (const auto& s) -> bool
    {
      using T = decay_t<decltype (s)>;

      if constexpr (is_same_v<T, no_file>)
        return content != s.content;
      else if constexpr (is_same_v<T, existing_file>)
        return content != s.content;
      else if constexpr (is_same_v<T, loading_file>)
        return content != s.content;
      else if constexpr (is_same_v<T, saving_file>)
        return content != s.content;
      else
        return false;
    }, state);
  }

  bool file_buffer::
  io_in_progress () const noexcept
  {
    return !holds_alternative<existing_file> (state) &&
           !holds_alternative<no_file> (state);
  }

  bool file_buffer::
  load_in_progress () const noexcept
  {
    return holds_alternative<loading_file> (state);
  }

  bool file_buffer::
  save_in_progress () const noexcept
  {
    return holds_alternative<saving_file> (state);
  }

  optional<string> file_buffer::
  file_name () const noexcept
  {
    return visit ([] (const auto& s) -> optional<string>
    {
      return s.name.get ();
    }, state);
  }

  optional<float> file_buffer::
  progress_percent () const noexcept
  {
    if (auto* l = get_if<loading_file> (&state))
      return l->progress_percent ();
    else if (auto* s = get_if<saving_file> (&state))
      return s->progress_percent (content.line_count ());
    else
      return nullopt;
  }

  // Implementation details.
  //

  namespace
  {
    // We need a rough estimate of the file size to drive the progress bar.
    // Seeking to the end is usually cheap enough.
    //
    static size_t
    get_file_size (ifstream& f)
    {
      auto start (f.tellg ());
      f.seekg (0, ios::end);

      auto end (f.tellg ());
      f.seekg (start, ios::beg);

      return static_cast<size_t> (end - start);
    }

    // Don't flood the main loop with progress updates; throttle them.
    //
    constexpr size_t progress_report_interval_bytes (1 << 20);        // 1 MB
    constexpr size_t progress_report_interval_lines ((1 << 20) / 40); // ~25k
  }

  // Effect factories.
  //

  // Create an effect that loads the file in a background thread.
  //
  // The strategy here is to build the immer vector using a transient. This
  // avoids the O(log N) cost of persistent updates for every line, bringing
  // it closer to O(1) amortized.
  //
  static file_io_effect
  make_load_effect (immer::box<string> name)
  {
    return [name] (function<void (file_io_action)> dispatch)
    {
      // We detach the thread because the effect interface is fire-and-forget.
      //
      // @@: Maybe we should add a proper thread pool or a joining mechanism to
      // handle app shutdown gracefully. For now, we assume the OS cleans up if
      // we exit during a load.
      //
      jthread ([name, d = move (dispatch)]
      {
        // Start with a transient vector. We initialize with an empty line
        // to maintain the invariant that a buffer always has at least one line.
        //
        auto lines (text_buffer::lines_type {
          text_buffer::line_type {}}.transient ());

        ifstream f;
        f.exceptions (fstream::badbit | fstream::failbit);

        try
        {
          f.open (name.get ());
          f.exceptions (fstream::badbit); // EOF is not an exception.

          size_t size (get_file_size (f));
          size_t read (0);
          size_t last_report (0);

          loading_file progress {
            name,
            text_buffer {lines.persistent ()},
            0,
            size
          };

          string line;
          bool first (true);

          while (getline (f, line))
          {
            read += line.size () + 1; // Count newline.

            // Convert string to our immutable line type using iterator
            // range constructor for better performance.
            //
            text_buffer::line_type flex_line {line.begin (), line.end ()};

            if (first)
              lines.set (0, move (flex_line)), first = false;
            else
              lines.push_back (move (flex_line));

            // Reuse capacity.
            //
            line.clear ();

            // Report progress.
            //
            if (read - last_report >= progress_report_interval_bytes)
            {
              progress.content = text_buffer {lines.persistent ()};
              progress.loaded_bytes = read;

              d (load_progress_action {progress});
              last_report = read;
            }
          }

          // Commit the transient to a persistent structure and notify.
          //
          d (load_done_action {
            existing_file {name, text_buffer {lines.persistent ()}}
          });
        }
        catch (...)
        {
          // If we failed, give the user whatever partial content we managed
          // to read so they can at least salvage something.
          //
          d (load_error_action {
            existing_file {name, text_buffer {lines.persistent ()}},
            current_exception ()
          });
        }
      }).detach ();
    };
  }

  // Create an effect that saves the file in a background thread.
  //
  static file_io_effect
  make_save_effect (immer::box<string> name,
                    text_buffer old_content,
                    text_buffer new_content)
  {
    return [name, old_content, new_content] (
             function<void (file_io_action)> dispatch)
    {
      jthread ([name, old_content, new_content, d = move (dispatch)]
      {
        ofstream f;
        f.exceptions (fstream::badbit | fstream::failbit);

        saving_file progress {name, new_content, 0};

        try
        {
          f.open (name.get ());

          size_t last_report (0);
          size_t total (new_content.line_count ());

          // Iterate and write.
          //
          // Note that iterating an immer vector is reasonably fast, but
          // strictly slower than a contiguous memory block.
          //
          for (size_t i (0); i < total; ++i)
          {
            const auto& line (new_content.line_at (line_number {i}));

            // Use for_each_chunk for chunk-based writing.
            //
            immer::for_each_chunk (line,
                                   [&] (auto first, auto last)
            {
              f.write (first, last - first);
            });
            f.put ('\n');

            ++progress.saved_lines;

            if (progress.saved_lines - last_report >=
                progress_report_interval_lines)
            {
              d (save_progress_action {progress});
              last_report = progress.saved_lines;
            }
          }

          d (save_done_action {existing_file {name, new_content}});
        }
        catch (...)
        {
          // Attempt recovery logic.
          //
          // If the save failed halfway, the file on disk is likely corrupted.
          // We try to reconstruct what we *think* is on disk by combining
          // the saved portion with the old tail.
          //
          auto recovered (new_content.lines ().take (progress.saved_lines) +
                          old_content.lines ().drop (progress.saved_lines));

          d (save_error_action {
            existing_file {name, text_buffer {recovered}},
            current_exception ()
          });
        }
      }).detach ();
    };
  }

  // External API
  //

  pair<file_buffer, file_io_effect>
  load_file (file_buffer b, const string& name)
  {
    // Transition to loading state.
    //
    loading_file l {
      immer::box<string> {name},
      text_buffer {},
      0,
      1 // Placeholder size until the thread opens the file.
    };

    b.state = l;
    b.content = text_buffer {};

    return {b, make_load_effect (immer::box<string> {name})};
  }

  pair<file_buffer, file_io_effect>
  save_file (file_buffer b)
  {
    auto* existing (get_if<existing_file> (&b.state));
    MINE_PRECONDITION (existing != nullptr);

    auto name (existing->name);
    auto old_c (existing->content);
    auto new_c (b.content);

    // Transition to saving state.
    //
    saving_file s {name, new_c, 0};
    b.state = s;

    return {b, make_save_effect (name, old_c, new_c)};
  }

  pair<file_buffer, optional<string>>
  update_file_buffer (file_buffer b, file_io_action a)
  {
    // Reduce the action into the new state.
    //
    return visit ([&] (auto&& act) -> pair<file_buffer, optional<string>>
    {
      using T = decay_t<decltype (act)>;

      if constexpr (is_same_v<T, load_progress_action>)
      {
        b.content = act.file.content;
        b.state = act.file;
        return {b, nullopt};
      }
      else if constexpr (is_same_v<T, load_done_action>)
      {
        b.content = act.file.content;
        b.state = act.file;
        return {b, "Loaded: " + act.file.name.get ()};
      }
      else if constexpr (is_same_v<T, load_error_action>)
      {
        b.content = act.file.content;
        b.state = act.file;
        return {b, "Error loading: " + act.file.name.get ()};
      }
      else if constexpr (is_same_v<T, save_progress_action>)
      {
        b.state = act.file;
        return {b, nullopt};
      }
      else if constexpr (is_same_v<T, save_done_action>)
      {
        b.state = act.file;
        return {b, "Saved: " + act.file.name.get ()};
      }
      else if constexpr (is_same_v<T, save_error_action>)
      {
        b.state = act.file;
        return {b, "Error saving: " + act.file.name.get ()};
      }
      else
      {
        return {b, nullopt};
      }
    }, a);
  }
}

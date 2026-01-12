#include <mine/mine-io-file.hxx>

#include <thread>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <exception>

#include <immer/algorithm.hpp>
#include <immer/flex_vector_transient.hpp>

#include <mine/mine-assert.hxx>

using namespace std;

namespace mine
{
  // Query
  //

  bool file_buffer::
  is_dirty () const noexcept
  {
    // A buffer is "dirty" if the text currently in memory (what the user sees)
    // differs from the text we believe is on disk (stored in the state
    // variant).
    //
    return visit ([&] (const auto& s) -> bool
    {
      using type = decay_t<decltype (s)>;

      if constexpr (is_same_v<type, no_file>)
        return content != s.content;
      else if constexpr (is_same_v<type, existing_file>)
        return content != s.content;
      else if constexpr (is_same_v<type, loading_file>)
        return content != s.content;
      else if constexpr (is_same_v<type, saving_file>)
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

  // Implementation Details (Effects)
  //

  namespace
  {
    // Helper: Seek to end to get file size.
    //
    // We need this for the progress bar. Users get anxious if they don't know
    // if we are 10% or 90% done.
    //
    static size_t
    file_size (ifstream& f)
    {
      auto b (f.tellg ());
      f.seekg (0, ios::end);

      auto e (f.tellg ());
      f.seekg (b, ios::beg);

      return static_cast<size_t> (e - b);
    }

    // Throttle progress updates to avoid flooding the main event loop.
    //
    constexpr size_t progress_bytes_ (1 << 20);        // 1 MB
    constexpr size_t progress_lines_ ((1 << 20) / 40); // ~25k lines
  }

  // Load file in background.
  //
  // @@: Consider using a thread pool here.
  //
  static file_io_effect
  load_effect (immer::box<string> name)
  {
    return [name] (function<void (file_io_action)> dispatch)
    {
      // Detach because this is fire-and-forget. The effect is done when the
      // lambda returns, but the work happens asynchronously.
      //
      jthread ([name, d = move (dispatch)]
      {
        // Performance Note:
        //
        // We use a `transient` vector here. `immer` vectors are persistent
        // (immutable), so pushing back line-by-line on a persistent vector
        // would be O(log N) * N. By using a transient, we get mutability
        // within this thread and amortized O(1) push_back, making the load
        // significantly faster.
        //
        auto ls (text_buffer::lines_type {text_buffer::line {}}.transient ());

        ifstream f;
        f.exceptions (fstream::badbit | fstream::failbit);

        try
        {
          f.open (name.get ());
          f.exceptions (fstream::badbit); // We handle EOF manually.

          size_t total (file_size (f));
          size_t read (0);
          size_t last (0);

          loading_file prog {
            name,
            text_buffer (ls.persistent ()),
            0,
            total
          };

          string s;
          bool first (true);

          while (getline (f, s))
          {
            read += s.size () + 1; // +1 for the newline we stripped.

            // Construct the line object.
            //
            text_buffer::line l (string_view (s.data (), s.size ()));

            if (first)
              ls.set (0, move (l)), first = false;
            else
              ls.push_back (move (l));

            s.clear (); // Recycle buffer capacity.

            // Throttle progress reports.
            //
            if (read - last >= progress_bytes_)
            {
              // Snapshot the transient to update the UI.
              //
              prog.content = text_buffer (ls.persistent ());
              prog.loaded_bytes = read;

              d (load_progress_action {prog});
              last = read;
            }
          }

          // Freeze the transient into a persistent structure and ship it.
          //
          d (load_done_action {
            existing_file {name, text_buffer (ls.persistent ())}
          });
        }
        catch (...)
        {
          // On error, we return whatever partial content we managed to read.
          //
          d (load_error_action {
            existing_file {name, text_buffer {ls.persistent ()}},
            current_exception ()
          });
        }
      }).detach ();
    };
  }

  // Save file in background.
  //
  static file_io_effect
  save_effect (immer::box<string> name,
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

        saving_file prog {name, new_content, 0};

        try
        {
          f.open (name.get ());

          size_t last (0);
          size_t n (new_content.line_count ());

          // Write line by line.
          //
          for (size_t i (0); i < n; ++i)
          {
            const auto& l (new_content.line_at (line_number {i}));

            // Chunk iteration.
            //
            // The `data` in a line is an immer vector, which might be
            // non-contiguous. `for_each_chunk` gives us contiguous ranges
            // (pointers) that we can pass directly to `f.write`.
            //
            immer::for_each_chunk (l.data,
                                   [&] (auto b, auto e)
            {
              f.write (b, e - b);
            });
            f.put ('\n');

            ++prog.saved_lines;

            if (prog.saved_lines - last >= progress_lines_)
            {
              d (save_progress_action {prog});
              last = prog.saved_lines;
            }
          }

          d (save_done_action {existing_file {name, new_content}});
        }
        catch (...)
        {
          // Recovery Strategy:
          //
          // If the save crashes halfway, the file on disk is trash. We try to
          // construct a "best guess" of what is physically on disk by taking
          // the lines we successfully wrote from `new_content` and appending
          // the rest from `old_content` (assuming the tail wasn't touched,
          // which is a shaky assumption but better than nothing).
          //
          auto rec (new_content.lines ().take (prog.saved_lines) +
                    old_content.lines ().drop (prog.saved_lines));

          d (save_error_action {
            existing_file {name, text_buffer {rec}},
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
    // Initialize loading state.
    //
    // We start with an empty buffer and a placeholder size (1) until the
    // thread opens the file and finds the real size.
    //
    loading_file l {
      immer::box<string> {name},
      text_buffer {},
      0,
      1
    };

    b.state = l;
    b.content = text_buffer {};

    return {b, load_effect (immer::box<string> {name})};
  }

  pair<file_buffer, file_io_effect>
  save_file (file_buffer b)
  {
    auto* ex (get_if<existing_file> (&b.state));
    MINE_PRECONDITION (ex != nullptr);

    auto name (ex->name);
    auto old_c (ex->content);
    auto new_c (b.content);

    // Transition to saving state.
    //
    saving_file s {name, new_c, 0};
    b.state = s;

    return {b, save_effect (name, old_c, new_c)};
  }

  pair<file_buffer, optional<string>>
  update_file_buffer (file_buffer b, file_io_action a)
  {
    // The Reducer.
    //
    // This function merges the asynchronous result back into the main state.
    // We simply return the new buffer state and an optional status message
    // for the UI.
    //
    return visit ([&] (auto&& x) -> pair<file_buffer, optional<string>>
    {
      using type = decay_t<decltype (x)>;

      if constexpr (is_same_v<type, load_progress_action>)
      {
        b.content = x.file.content;
        b.state = x.file;
        return {b, nullopt};
      }
      else if constexpr (is_same_v<type, load_done_action>)
      {
        b.content = x.file.content;
        b.state = x.file;
        return {b, "Loaded: " + x.file.name.get ()};
      }
      else if constexpr (is_same_v<type, load_error_action>)
      {
        b.content = x.file.content;
        b.state = x.file;
        return {b, "Error loading: " + x.file.name.get ()};
      }
      else if constexpr (is_same_v<type, save_progress_action>)
      {
        b.state = x.file;
        return {b, nullopt};
      }
      else if constexpr (is_same_v<type, save_done_action>)
      {
        b.state = x.file;
        return {b, "Saved: " + x.file.name.get ()};
      }
      else if constexpr (is_same_v<type, save_error_action>)
      {
        b.state = x.file;
        return {b, "Error saving: " + x.file.name.get ()};
      }
      else
      {
        return {b, nullopt};
      }
    }, a);
  }
}

#include <mine/mine-content.hxx>

#include <mine/mine-contract.hxx>

using namespace std;

namespace mine
{
  content::line::
  line (immer::flex_vector<char> d)
    : data (move (d))
  {
    update_idx ();
  }

  content::line::
  line (string_view s)
    : data (s.begin (), s.end ()),
      str_cache (s),
      str_valid (true)
  {
    update_idx ();
  }

  content::line::
  line (const line& x)
    : data (x.data)
  {
    update_idx ();
  }

  content::line&
  content::line::
  operator= (const line& x)
  {
    if (this != &x)
    {
      data = x.data;
      str_valid = false;
      update_idx ();
    }
    return *this;
  }

  content::line::
  line (line&& x) noexcept
    : data (move (x.data)),
      str_cache (move (x.str_cache)),
      str_valid (x.str_valid),
      idx (move (x.idx))
  {
  }

  content::line&
  content::line::
  operator= (line&& x) noexcept
  {
    if (this != &x)
    {
      data = move (x.data);
      str_cache = move (x.str_cache);
      str_valid = x.str_valid;
      idx = move (x.idx);
    }
    return *this;
  }

  bool content::line::
  operator== (const line& x) const
  {
    return data == x.data;
  }

  string_view content::line::
  view () const
  {
    if (!str_valid)
    {
      str_cache.clear ();
      str_cache.reserve (data.size ());
      immer::for_each_chunk (data,
                             [&] (auto first, auto last)
      {
        str_cache.append (first, last);
      });
      str_valid = true;
    }
    return str_cache;
  }

  void content::line::
  update_idx ()
  {
    idx.update (view ());
  }

  const grapheme_index& content::line::
  get_index () const
  {
    if (!idx.valid ())
      idx.update (view ());

    return idx;
  }

  size_t content::line::
  count () const
  {
    return idx.size ();
  }

  optional<string_view> content::
  grapheme_at (cursor_position p) const
  {
    if (!contains (p.line))
      return nullopt;

    const auto& l (lines_[p.line.value]);
    const auto* c (l.idx.cluster_at_index (p.column.value));

    if (c == nullptr)
      return nullopt;

    return c->text (l.view ());
  }

  content content::
  insert_graphemes (cursor_position p, string_view s) const
  {
    MINE_PRECONDITION (contains (p.line));

    const auto& l (lines_[p.line.value]);
    MINE_PRECONDITION (p.column.value <= l.count ());

    size_t n (l.idx.index_to_byte (p.column.value));

    auto pre (l.data.take (n));
    auto suf (l.data.drop (n));

    immer::flex_vector<char> v (s.begin (), s.end ());

    auto t (pre + v + suf);
    line nl (move (t));

    return content (lines_.set (p.line.value, move (nl)));
  }

  content content::
  delete_previous_grapheme (cursor_position p) const
  {
    MINE_PRECONDITION (contains (p.line));

    if (p.column.value == 0)
    {
      if (p.line.value == 0)
        return *this;

      return merge_lines (line_number (p.line.value - 1), p.line);
    }

    const auto& l (lines_[p.line.value]);

    const auto* c (l.idx.cluster_at_index (p.column.value - 1));
    MINE_INVARIANT (c != nullptr);

    auto t (l.data.erase (c->byte_offset,
                          c->byte_offset + c->byte_length));
    line nl (move (t));

    return content (lines_.set (p.line.value, move (nl)));
  }

  content content::
  delete_next_grapheme (cursor_position p) const
  {
    MINE_PRECONDITION (contains (p.line));

    const auto& l (lines_[p.line.value]);

    if (p.column.value >= l.count ())
    {
      if (p.line.value + 1 >= lines_.size ())
        return *this;

      return merge_lines (p.line, line_number (p.line.value + 1));
    }

    const auto* c (l.idx.cluster_at_index (p.column.value));
    MINE_INVARIANT (c != nullptr);

    auto t (l.data.erase (c->byte_offset,
                          c->byte_offset + c->byte_length));
    line nl (move (t));

    return content (lines_.set (p.line.value, move (nl)));
  }

  content content::
  insert_newline (cursor_position p) const
  {
    MINE_PRECONDITION (contains (p.line));

    const auto& l (lines_[p.line.value]);
    MINE_PRECONDITION (p.column.value <= l.count ());

    size_t n (l.idx.index_to_byte (p.column.value));

    auto lhs (l.data.take (n));
    auto rhs (l.data.drop (n));

    line ll (move (lhs));
    line rl (move (rhs));

    auto r (lines_);
    r = r.set (p.line.value, move (ll));
    r = r.insert (p.line.value + 1, move (rl));

    return content (move (r));
  }

  content content::
  delete_range (cursor_position b, cursor_position e) const
  {
    MINE_PRECONDITION (b.line.value <= e.line.value);
    MINE_PRECONDITION (contains (b.line));
    MINE_PRECONDITION (contains (e.line));

    if (b.line == e.line)
    {
      if (b.column == e.column)
        return *this;

      const auto& l (lines_[b.line.value]);

      size_t b_off (l.idx.index_to_byte (b.column.value));
      size_t e_off (l.idx.index_to_byte (e.column.value));

      auto t (l.data.erase (b_off, e_off));
      line nl (move (t));

      return content (lines_.set (b.line.value, move (nl)));
    }

    const auto& bl (lines_[b.line.value]);
    const auto& el (lines_[e.line.value]);

    size_t b_off (bl.idx.index_to_byte (b.column.value));
    size_t e_off (el.idx.index_to_byte (e.column.value));

    auto pre (bl.data.take (b_off));
    auto suf (el.data.drop (e_off));

    auto t (pre + suf);
    line nl (move (t));

    auto r (lines_.set (b.line.value, move (nl)));

    auto r_pre (r.take (b.line.value + 1));
    auto r_suf (r.drop (e.line.value + 1));

    return content (r_pre + r_suf);
  }

  string content::
  get_range (cursor_position b, cursor_position e) const
  {
    MINE_PRECONDITION (b.line.value <= e.line.value);
    MINE_PRECONDITION (contains (b.line));
    MINE_PRECONDITION (contains (e.line));

    if (b.line == e.line)
    {
      if (b.column == e.column)
        return "";

      const auto& l (lines_[b.line.value]);

      size_t bo (l.idx.index_to_byte (b.column.value));
      size_t eo (l.idx.index_to_byte (e.column.value));

      return string (l.view ().substr (bo, eo - bo));
    }

    string r;

    const auto& bl (lines_[b.line.value]);
    size_t bo (bl.idx.index_to_byte (b.column.value));

    r.append (bl.view ().substr (bo));
    r.push_back ('\n');

    immer::for_each (
      lines_.drop (b.line.value + 1).take (e.line.value - b.line.value - 1),
      [&] (const line& l)
    {
      r.append (l.view ());
      r.push_back ('\n');
    });

    const auto& el (lines_[e.line.value]);
    size_t eo (el.idx.index_to_byte (e.column.value));

    r.append (el.view ().substr (0, eo));

    return r;
  }

  content content::
  merge_lines (line_number f, line_number s) const
  {
    MINE_PRECONDITION (f.value + 1 == s.value);
    MINE_PRECONDITION (contains (f));
    MINE_PRECONDITION (contains (s));

    const auto& fl (lines_[f.value]);
    const auto& sl (lines_[s.value]);

    auto t (fl.data + sl.data);
    line nl (move (t));

    auto r (lines_.set (f.value, move (nl)));
    r = r.erase (s.value);

    return content (move (r));
  }

  content
  make_empty_content ()
  {
    return content ();
  }

  content
  make_content_from_string (string_view s)
  {
    content::lines_type ls;

    size_t b (0);
    size_t e (s.find ('\n'));

    while (e != string_view::npos)
    {
      auto sub (s.substr (b, e - b));
      content::line l (sub);
      ls = ls.push_back (move (l));

      b = e + 1;
      e = s.find ('\n', b);
    }

    auto sub (s.substr (b));
    content::line l (sub);
    ls = ls.push_back (move (l));

    return content (move (ls));
  }

  string
  content_to_string (const content& b)
  {
    string r;
    bool first (true);

    immer::for_each (b.lines (),
                     [&] (const content::line& l)
    {
      if (!first)
        r.push_back ('\n');

      first = false;
      auto v = l.view ();

      r.append (v.begin (), v.end ());
    });

    return r;
  }
}

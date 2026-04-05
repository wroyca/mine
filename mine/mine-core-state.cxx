#include <mine/mine-core-state.hxx>

#include <algorithm>
#include <cmath>

using namespace std;

namespace mine
{
  state::
  state ()
    : buffers_ (),
      windows_ (),
      layout_ (split_ptr (
        split_node {true, window_id {1}, split_dir::vertical, 0.5f, nullptr, nullptr})),
      active_window_ (window_id {1}),
      next_buffer_id_ (buffer_id {2}),
      next_window_id_ (window_id {2}),
      cmd_ ()
  {
    // Bootstrap the initial state with a single empty buffer and a default
    // window layout.
    //
    buffers_ = buffers_.set (buffer_id {1}, buffer_state {make_empty_buffer (), false, ""});
    windows_ = windows_.set (
      window_id {1},
      window_state {buffer_id {1},
                    mine::cursor {},
                    mine::view {line_number (0), screen_size (24, 80)}});
  }

  state::
  state (text_buffer b,
         mine::cursor c,
         mine::view v,
         bool m,
         cmdline_state cs)
    : buffers_ (),
      windows_ (),
      layout_ (split_ptr (
        split_node {true, window_id {1}, split_dir::vertical, 0.5f, nullptr, nullptr})),
      active_window_ (window_id {1}),
      next_buffer_id_ (buffer_id {2}),
      next_window_id_ (window_id {2}),
      cmd_ (move (cs))
  {
    buffers_ = buffers_.set (buffer_id {1}, buffer_state {move (b), m, ""});
    windows_ = windows_.set (window_id {1}, window_state {buffer_id {1}, c, v});
  }

  // Traverse the binary split tree and resolve the absolute grid coordinates
  // for every active window leaf.
  //
  void state::
  compute_layout (const split_ptr& n,
                  uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h,
                  vector<window_layout>& o)
  {
    // If we hit a leaf, we've found a concrete window. Just record its
    // spatial boundaries.
    //
    if (n->is_leaf)
    {
      o.push_back ({n->win, x, y, w, h});
      return;
    }

    // Otherwise, partition the available space according to the split
    // ratio and recurse down both branches.
    //
    if (n->dir == split_dir::horizontal)
    {
      uint16_t h1 (static_cast<uint16_t> (h * n->ratio));
      uint16_t h2 (h - h1);

      compute_layout (n->child1, x, y, w, h1, o);
      compute_layout (n->child2, x, y + h1, w, h2, o);
    }
    else
    {
      // Reserve exactly one column for the vertical border. We give the
      // remaining space to the two children.
      //
      uint16_t w1 (static_cast<uint16_t> ((w > 0 ? w - 1 : 0) * n->ratio));
      uint16_t w2 ((w > 0 ? w - 1 : 0) - w1);

      compute_layout (n->child1, x, y, w1, h, o);
      compute_layout (n->child2, x + w1 + 1, y, w2, h, o);
    }
  }

  void state::
  get_layout (vector<window_layout>& o, uint16_t w, uint16_t h) const
  {
    if (layout_)
      compute_layout (layout_, 0, 0, w, h, o);
  }

  split_ptr state::
  insert_split (const split_ptr& n, window_id t, window_id nw, split_dir d)
  {
    // Walk the tree looking for the target window. Once found, replace it with
    // a new split node that contains both the old and the newly created window.
    //
    if (n->is_leaf)
    {
      if (n->win == t)
      {
        split_node sn;
        sn.is_leaf = false;
        sn.dir = d;
        sn.ratio = 0.5f;
        sn.child1 = n;

        split_node c2;
        c2.is_leaf = true;
        c2.win = nw;
        sn.child2 = split_ptr (c2);

        return split_ptr (sn);
      }
      return n;
    }

    auto c1 (insert_split (n->child1, t, nw, d));
    auto c2 (insert_split (n->child2, t, nw, d));

    // Rebuild the current node if any of its children were modified during the
    // traversal.
    //
    if (c1 != n->child1 || c2 != n->child2)
    {
      split_node sn (*n);
      sn.child1 = c1;
      sn.child2 = c2;
      return split_ptr (sn);
    }

    return n;
  }

  split_ptr state::
  remove_window (const split_ptr& n, window_id t, bool& rm)
  {
    // Find the target leaf and signal its removal. The parent will then
    // collapse and promote the surviving child.
    //
    if (n->is_leaf)
    {
      if (n->win == t)
      {
        rm = true;
        return nullptr;
      }
      return n;
    }

    bool r1 (false), r2 (false);
    auto c1 (remove_window (n->child1, t, r1));
    auto c2 (remove_window (n->child2, t, r2));

    // If one child was removed, seamlessly promote the other up the tree.
    //
    if (r1) return c2;
    if (r2) return c1;

    if (c1 != n->child1 || c2 != n->child2)
    {
      split_node sn (*n);
      sn.child1 = c1;
      sn.child2 = c2;
      return split_ptr (sn);
    }

    return n;
  }

  window_id state::
  find_first_leaf (const split_ptr& n)
  {
    // Dive down the left side of the tree until we hit a leaf. This serves as a
    // safe fallback when deciding which window gets focus after the active one
    // closes.
    //
    if (!n)
      return invalid_window;

    if (n->is_leaf)
      return n->win;

    return find_first_leaf (n->child1);
  }

  state state::
  with_buffer (text_buffer b) const
  {
    return update (move (b), get_cursor ());
  }

  state state::
  with_cursor (mine::cursor c) const
  {
    buffer_id bid (active_buffer_id ());
    const auto& b (buffers_.at (bid).content);

    auto nc (c.clamp_to_buffer (b));

    auto ns (*this);
    window_state ws (ns.windows_.at (active_window_));
    ws.cur = nc;
    ws.vw = ws.vw.scroll_to_cursor (nc, b);

    ns.windows_ = ns.windows_.set (active_window_, ws);
    return ns;
  }

  state state::
  with_view (mine::view v) const
  {
    auto ns (*this);
    window_state ws (ns.windows_.at (active_window_));
    ws.vw = v;
    ns.windows_ = ns.windows_.set (active_window_, ws);
    return ns;
  }

  state state::
  with_modified (bool m) const
  {
    auto ns (*this);
    buffer_id bid (active_buffer_id ());
    buffer_state bs (ns.buffers_.at (bid));
    bs.modified = m;
    ns.buffers_ = ns.buffers_.set (bid, bs);
    return ns;
  }

  state state::
  with_cmdline (cmdline_state cs) const
  {
    auto ns (*this);
    ns.cmd_ = move (cs);
    return ns;
  }

  state state::
  with_cmdline_message (string m) const
  {
    auto ns (*this);
    ns.cmd_.message = move (m);
    return ns;
  }

  // Update a buffer and synchronize all windows tracking it.
  //
  // Notice that because text flow modifications dictate boundaries, we have to
  // clamp cursors on inactive windows to make sure they don't unexpectedly read
  // out-of-bounds memory when evaluating rendering highlights or future steps.
  //
  state state::
  update (text_buffer b, mine::cursor c) const
  {
    buffer_id bid (active_buffer_id ());

    auto ns (*this);

    buffer_state bs (ns.buffers_.at (bid));
    bs.content = b;
    bs.modified = true;
    ns.buffers_ = ns.buffers_.set (bid, bs);

    for (auto it (ns.windows_.begin ()); it != ns.windows_.end (); ++it)
    {
      const auto& wid (it->first);
      const auto& ws (it->second);

      if (ws.buf == bid)
      {
        window_state nws (ws);

        if (wid == active_window_)
          nws.cur = c.clamp_to_buffer (b);
        else
          nws.cur = nws.cur.clamp_to_buffer (b);

        nws.vw = nws.vw.scroll_to_cursor (nws.cur, b);
        ns.windows_ = ns.windows_.set (wid, nws);
      }
    }

    return ns;
  }

  state state::
  with_new_buffer (text_buffer b, string n) const
  {
    auto ns (*this);
    buffer_state bs {move (b), false, move (n)};
    ns.buffers_ = ns.buffers_.set (ns.next_buffer_id_, move (bs));
    ns.next_buffer_id_++;
    return ns;
  }

  state state::
  update_buffer (buffer_id id, text_buffer b) const
  {
    auto ns (*this);
    buffer_state bs (ns.buffers_.at (id));
    bs.content = b;
    ns.buffers_ = ns.buffers_.set (id, bs);

    for (auto it (ns.windows_.begin ()); it != ns.windows_.end (); ++it)
    {
      const auto& wid (it->first);
      const auto& ws (it->second);

      if (ws.buf == id)
      {
        window_state nws (ws);
        nws.cur = nws.cur.clamp_to_buffer (b);
        nws.vw = nws.vw.scroll_to_cursor (nws.cur, b);
        ns.windows_ = ns.windows_.set (wid, nws);
      }
    }

    return ns;
  }

  state state::
  switch_buffer (buffer_id id) const
  {
    auto ns (*this);
    window_state ws (ns.windows_.at (active_window_));
    ws.buf = id;

    const auto& b (ns.buffers_.at (id).content);
    ws.cur = ws.cur.clamp_to_buffer (b);
    ws.vw = ws.vw.scroll_to_cursor (ws.cur, b);

    ns.windows_ = ns.windows_.set (active_window_, ws);
    return ns;
  }

  state state::
  split_active_window (split_dir d) const
  {
    auto ns (*this);
    window_id nid (ns.next_window_id_++);

    window_state ws (ns.windows_.at (active_window_));
    ns.windows_ = ns.windows_.set (nid, ws);

    ns.layout_ = insert_split (ns.layout_, active_window_, nid, d);

    // We must immediately apply the new layout constraints. Otherwise, the view
    // object will retain its pre-split dimensions, and cursor movement commands
    // won't trigger a scroll when crossing the smaller boundary.
    //
    return ns.resize_layout (ns.screen_size_);
  }

  state state::
  close_active_window () const
  {
    auto ns (*this);

    bool rm (false);
    auto nl (remove_window (ns.layout_, active_window_, rm));

    if (!nl)
    {
      // Refuse to close the very last window so we don't collapse into an
      // undefined void layout.
      //
      return *this;
    }

    ns.layout_ = nl;
    ns.windows_ = ns.windows_.erase (active_window_);
    ns.active_window_ = find_first_leaf (ns.layout_);

    // Recompute the grid space for the surviving windows so they expand
    // to fill the void left by the closed window.
    //
    return ns.resize_layout (ns.screen_size_);
  }

  state state::
  switch_window (int dx, int dy) const
  {
    auto ns (*this);

    // Fake the spatial computation using arbitrary values to navigate
    // adjacency purely on the logical aspect ratio structure.
    //
    vector<window_layout> lays;
    compute_layout (ns.layout_, 0, 0, 1000, 1000, lays);

    const window_layout* aw (nullptr);

    for (const auto& l : lays)
    {
      if (l.win == active_window_)
      {
        aw = &l;
        break;
      }
    }

    if (!aw)
      return ns;

    window_id best (invalid_window);
    int best_dist (1000000);

    for (const auto& l : lays)
    {
      if (l.win == active_window_)
        continue;

      int cx (l.x + l.w / 2);
      int cy (l.y + l.h / 2);
      int ax (aw->x + aw->w / 2);
      int ay (aw->y + aw->h / 2);

      bool match (false);

      if (dx > 0 && l.x >= aw->x + aw->w &&
          (l.y < aw->y + aw->h && l.y + l.h > aw->y))
        match = true;

      if (dx < 0 && l.x + l.w <= aw->x &&
          (l.y < aw->y + aw->h && l.y + l.h > aw->y))
        match = true;

      if (dy > 0 && l.y >= aw->y + aw->h &&
          (l.x < aw->x + aw->w && l.x + l.w > aw->x))
        match = true;

      if (dy < 0 && l.y + l.h <= aw->y &&
          (l.x < aw->x + aw->w && l.x + l.w > aw->x))
        match = true;

      if (match)
      {
        int dist (abs (cx - ax) + abs (cy - ay));

        if (dist < best_dist)
        {
          best_dist = dist;
          best = l.win;
        }
      }
    }

    if (best != invalid_window)
      ns.active_window_ = best;

    return ns;
  }

  state state::
  switch_window_direct (window_id id) const
  {
    auto ns (*this);

    if (ns.windows_.count (id))
      ns.active_window_ = id;

    return ns;
  }

  state state::
  resize_layout (screen_size s) const
  {
    auto ns (*this);
    ns.screen_size_ = s;

    vector<window_layout> lays;

    // Notice we reserve the final global row manually for the command line.
    // The layouts returned below map natively to terminal space geometry.
    //
    compute_layout (ns.layout_,
                    0,
                    0,
                    s.cols,
                    s.rows > 0 ? s.rows - 1 : 0,
                    lays);

    for (const auto& l : lays)
    {
      window_state ws (ns.windows_.at (l.win));
      const auto& b (ns.buffers_.at (ws.buf).content);

      ws.vw = ws.vw.resize (screen_size (l.h, l.w));

      // Force the viewport to clamp the scroll offset so the cursor doesn't
      // end up stranded out of bounds after the window shrinks.
      //
      ws.vw = ws.vw.scroll_to_cursor (ws.cur, b);

      ns.windows_ = ns.windows_.set (l.win, ws);
    }

    return ns;
  }
}

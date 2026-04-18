#include <mine/mine-workspace.hxx>

#include <algorithm>
#include <cmath>

using namespace std;

namespace mine
{
  workspace::
  workspace ()
    : buffers_ (),
      windows_ (),
      layout_ (layout_tree (
        layout_node {true, window_id {1}, layout_direction::vertical, 0.5f, nullptr, nullptr})),
      active_window_ (window_id {1}),
      next_document_id_ (document_id {2}),
      next_window_id_ (window_id {2}),
      cmd_ ()
  {
    // Bootstrap the initial state with a single empty buffer and a default
    // window layout.
    //
    buffers_ = buffers_.set (
      document_id {1},
      document {make_empty_content (), false, "", language::unknown ()});
    windows_ = windows_.set (
      window_id {1},
      editor_window {document_id {1},
                    mine::cursor {},
                    mine::viewport {line_number (0), screen_size (24, 80)}});
  }

  workspace::
  workspace (content b,
         mine::cursor c,
         mine::viewport v,
         bool m,
         command_line cs)
    : buffers_ (),
      windows_ (),
      layout_ (layout_tree (
        layout_node {true, window_id {1}, layout_direction::vertical, 0.5f, nullptr, nullptr})),
      active_window_ (window_id {1}),
      next_document_id_ (document_id {2}),
      next_window_id_ (window_id {2}),
      cmd_ (move (cs))
  {
    buffers_ = buffers_.set (document_id {1}, document {move (b), m, ""});
    windows_ = windows_.set (window_id {1}, editor_window {document_id {1}, c, v});
  }

  // Traverse the binary split tree and resolve the absolute grid coordinates
  // for every active window leaf.
  //
  void workspace::
  compute_layout (const layout_tree& n,
                  uint16_t x, uint16_t y,
                  uint16_t w, uint16_t h,
                  vector<window_partition>& o)
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
    if (n->dir == layout_direction::horizontal)
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

  void workspace::
  get_layout (vector<window_partition>& o, uint16_t w, uint16_t h) const
  {
    if (layout_)
      compute_layout (layout_, 0, 0, w, h, o);
  }

  layout_tree workspace::
  insert_split (const layout_tree& n, window_id t, window_id nw, layout_direction d)
  {
    // Walk the tree looking for the target window. Once found, replace it with
    // a new split node that contains both the old and the newly created window.
    //
    if (n->is_leaf)
    {
      if (n->win == t)
      {
        layout_node sn;
        sn.is_leaf = false;
        sn.dir = d;
        sn.ratio = 0.5f;
        sn.child1 = n;

        layout_node c2;
        c2.is_leaf = true;
        c2.win = nw;
        sn.child2 = layout_tree (c2);

        return layout_tree (sn);
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
      layout_node sn (*n);
      sn.child1 = c1;
      sn.child2 = c2;
      return layout_tree (sn);
    }

    return n;
  }

  layout_tree workspace::
  remove_window (const layout_tree& n, window_id t, bool& rm)
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
      layout_node sn (*n);
      sn.child1 = c1;
      sn.child2 = c2;
      return layout_tree (sn);
    }

    return n;
  }

  window_id workspace::
  find_first_leaf (const layout_tree& n)
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

  workspace workspace::
  with_content (content b) const
  {
    return update (move (b), get_cursor ());
  }

  workspace workspace::
  with_cursor (mine::cursor c) const
  {
    document_id bid (active_document_id ());
    const auto& b (buffers_.at (bid).text);

    auto nc (c.clamp_to_buffer (b));

    auto ns (*this);
    editor_window ws (ns.windows_.at (active_window_));
    ws.cursor = nc;
    ws.viewport = ws.viewport.scroll_to_cursor (nc, b);

    ns.windows_ = ns.windows_.set (active_window_, ws);
    return ns;
  }

  workspace workspace::
  with_view (mine::viewport v) const
  {
    auto ns (*this);
    editor_window ws (ns.windows_.at (active_window_));
    ws.viewport = v;
    ns.windows_ = ns.windows_.set (active_window_, ws);
    return ns;
  }

  workspace workspace::
  with_modified (bool m) const
  {
    auto ns (*this);
    document_id bid (active_document_id ());
    document bs (ns.buffers_.at (bid));
    bs.modified = m;
    ns.buffers_ = ns.buffers_.set (bid, bs);
    return ns;
  }

  workspace workspace::
  with_cmdline (command_line cs) const
  {
    auto ns (*this);
    ns.cmd_ = move (cs);
    return ns;
  }

  workspace workspace::
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
  workspace workspace::
  update (content b, mine::cursor c) const
  {
    document_id bid (active_document_id ());

    auto ns (*this);

    document bs (ns.buffers_.at (bid));
    bs.text = b;
    bs.modified = true;
    ns.buffers_ = ns.buffers_.set (bid, bs);

    for (auto it (ns.windows_.begin ()); it != ns.windows_.end (); ++it)
    {
      const auto& wid (it->first);
      const auto& ws (it->second);

      if (ws.doc == bid)
      {
        editor_window nws (ws);

        if (wid == active_window_)
          nws.cursor = c.clamp_to_buffer (b);
        else
          nws.cursor = nws.cursor.clamp_to_buffer (b);

        nws.viewport = nws.viewport.scroll_to_cursor (nws.cursor, b);
        ns.windows_ = ns.windows_.set (wid, nws);
      }
    }

    return ns;
  }

  workspace workspace::
  with_new_document (content b, string n, language lang) const
  {
    auto ns (*this);
    document bs {move (b), false, move (n), move (lang)};
    ns.buffers_ = ns.buffers_.set (ns.next_document_id_, move (bs));
    ns.next_document_id_++;
    return ns;
  }

  workspace workspace::
  with_document_language (document_id id, language lang) const
  {
    auto ns (*this);
    document d (ns.buffers_.at (id));
    d.lang = move (lang);
    ns.buffers_ = ns.buffers_.set (id, move (d));
    return ns;
  }

  workspace workspace::
  update_document (document_id id, content b) const
  {
    auto ns (*this);
    document bs (ns.buffers_.at (id));
    bs.text = b;
    ns.buffers_ = ns.buffers_.set (id, bs);

    for (auto it (ns.windows_.begin ()); it != ns.windows_.end (); ++it)
    {
      const auto& wid (it->first);
      const auto& ws (it->second);

      if (ws.doc == id)
      {
        editor_window nws (ws);
        nws.cursor = nws.cursor.clamp_to_buffer (b);
        nws.viewport = nws.viewport.scroll_to_cursor (nws.cursor, b);
        ns.windows_ = ns.windows_.set (wid, nws);
      }
    }

    return ns;
  }

  workspace workspace::
  switch_document (document_id id) const
  {
    auto ns (*this);
    editor_window ws (ns.windows_.at (active_window_));
    ws.doc = id;

    const auto& b (ns.buffers_.at (id).text);
    ws.cursor = ws.cursor.clamp_to_buffer (b);
    ws.viewport = ws.viewport.scroll_to_cursor (ws.cursor, b);

    ns.windows_ = ns.windows_.set (active_window_, ws);
    return ns;
  }

  workspace workspace::
  split_active_window (layout_direction d) const
  {
    auto ns (*this);
    window_id nid (ns.next_window_id_++);

    editor_window ws (ns.windows_.at (active_window_));
    ns.windows_ = ns.windows_.set (nid, ws);

    ns.layout_ = insert_split (ns.layout_, active_window_, nid, d);

    // We must immediately apply the new layout constraints. Otherwise, the view
    // object will retain its pre-split dimensions, and cursor movement commands
    // won't trigger a scroll when crossing the smaller boundary.
    //
    return ns.resize_layout (ns.screen_size_);
  }

  workspace workspace::
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

  workspace workspace::
  switch_window (int dx, int dy) const
  {
    auto ns (*this);

    // Fake the spatial computation using arbitrary values to navigate
    // adjacency purely on the logical aspect ratio structure.
    //
    vector<window_partition> lays;
    compute_layout (ns.layout_, 0, 0, 1000, 1000, lays);

    const window_partition* aw (nullptr);

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

  workspace workspace::
  switch_window_direct (window_id id) const
  {
    auto ns (*this);

    if (ns.windows_.count (id))
      ns.active_window_ = id;

    return ns;
  }

  workspace workspace::
  resize_layout (screen_size s) const
  {
    auto ns (*this);
    ns.screen_size_ = s;

    vector<window_partition> lays;

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
      editor_window ws (ns.windows_.at (l.win));
      const auto& b (ns.buffers_.at (ws.doc).text);

      ws.viewport = ws.viewport.resize (screen_size (l.h, l.w));

      // Force the viewport to clamp the scroll offset so the cursor doesn't
      // end up stranded out of bounds after the window shrinks.
      //
      ws.viewport = ws.viewport.scroll_to_cursor (ws.cursor, b);

      ns.windows_ = ns.windows_.set (l.win, ws);
    }

    return ns;
  }
}

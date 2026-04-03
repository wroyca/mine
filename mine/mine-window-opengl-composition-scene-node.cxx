#include <mine/mine-window-opengl-composition-scene-node.hxx>

#include <algorithm>

using namespace std;

namespace mine
{
  mat4 scene_node::
  local_transform () const
  {
    // Start with the identity matrix and apply the TRS (Translate, Rotate,
    // Scale) transformations.
    //
    mat4 r (mat4::identity ());

    r = mat4::translate (pos.x, pos.y) * r;

    if (rot != 0.0f)
    {
      // Calculate the anchor offset. We have to translate to the anchor
      // pivot point, rotate, and then translate back.
      //
      vec2 o (sz * anc);
      r = mat4::translate (o.x, o.y) * r;

      float c (cos (rot));
      float s (sin (rot));
      mat4  m (mat4::identity ());

      m (0, 0) = c;
      m (0, 1) = -s;
      m (1, 0) = s;
      m (1, 1) = c;

      r = m * r;
      r = mat4::translate (-o.x, -o.y) * r;
    }

    if (scale.x != 1.0f || scale.y != 1.0f)
      r = mat4::scale (scale.x, scale.y, 1.0f) * r;

    return r;
  }

  scene_graph::
  scene_graph ()
  {
    root_ = create ("root");
  }

  node_id scene_graph::
  create (string n)
  {
    return create_child (root_, move (n));
  }

  node_id scene_graph::
  create_child (node_id p, string n)
  {
    node_id i (next_++);

    // Grow the parallel arrays if necessary. We use a flat vector approach here
    // to maintain cache locality for the nodes.
    //
    if (i >= nodes_.size ())
    {
      nodes_.resize (i + 1);
      active_.resize (i + 1, false);
    }

    scene_node& c (nodes_[i]);
    c.id      = i;
    c.parent  = p;
    c.name    = move (n);
    c.visible = true;
    c.dirty   = true;

    active_[i] = true;

    // Wire up the parent if it is valid and currently active.
    //
    if (p != invalid_node && p < nodes_.size () && active_[p])
      nodes_[p].children.push_back (i);

    return i;
  }

  void scene_graph::
  destroy (node_id i)
  {
    if (i == invalid_node || i >= nodes_.size () || !active_[i])
      return;

    scene_node& n (nodes_[i]);

    // Recursively clean up the subtree.
    //
    for (node_id c : n.children)
      destroy (c);

    // Detach from the parent to keep the tree intact. Note that this requires
    // an O(N) lookup and erase in the siblings list, but it is usually fine
    // given typical child counts.
    //
    if (n.parent != invalid_node && n.parent < nodes_.size () && active_[n.parent])
    {
      auto& s (nodes_[n.parent].children);
      s.erase (remove (s.begin (), s.end (), i), s.end ());
    }

    active_[i] = false;
  }

  scene_node* scene_graph::
  get (node_id i)
  {
    if (i == invalid_node || i >= nodes_.size () || !active_[i])
      return nullptr;

    return &nodes_[i];
  }

  void scene_graph::
  update ()
  {
    if (root_ == invalid_node)
      return;

    update_node (nodes_[root_], mat4::identity ());
  }

  void scene_graph::
  update_node (scene_node& n, const mat4& p)
  {
    // If the node is dirty, calculate the new world transform and propagate
    // the dirty flag down to the immediate children.
    //
    if (n.dirty)
    {
      n.world = p * n.local_transform ();
      n.dirty = false;

      // Force all children to update if the parent moved. Note that we just
      // mark them dirty here. That is, the subsequent traversal loop will
      // handle the actual matrix multiplication.
      //
      for (node_id c : n.children)
      {
        if (c < nodes_.size () && active_[c])
          nodes_[c].dirty = true;
      }
    }

    for (node_id c : n.children)
    {
      if (c < nodes_.size () && active_[c])
        update_node (nodes_[c], n.world);
    }
  }
}

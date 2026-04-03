#pragma once

#include <mine/mine-window-opengl-composition-linear-algebra-vec.hxx>
#include <mine/mine-window-opengl-composition-linear-algebra-mat4.hxx>

#include <vector>
#include <string>
#include <cstdint>

namespace mine
{
  // Keep it simple. We use a standard integer for the node id so we don't
  // have to carry heavy pointers around. Zero means invalid, sort of like
  // null.
  //
  using node_id = std::uint32_t;
  constexpr node_id invalid_node = 0;

  // Scene node.
  //
  // This is basically just a transform with some hierarchy info glued on. Group
  // the bools together here so the compiler can pack them nicely without
  // wasting padding.
  //
  struct scene_node
  {
    node_id     id     {invalid_node};
    node_id     parent {invalid_node};
    std::string name;

    bool visible {true};
    bool dirty   {true};

    vec2  pos   {0.0f, 0.0f}; // position
    vec2  sz    {0.0f, 0.0f}; // size
    float rot   {0.0f};       // rotation
    vec2  scale {1.0f, 1.0f}; // scale
    vec2  anc   {0.0f, 0.0f}; // anchor point (0 to 1)

    mat4 world = mat4::identity ();
    int  z     = 0;

    std::vector<node_id> children;

    // Compute the local matrix from scale, rotation, and translation.
    //
    [[nodiscard]] mat4
    local_transform () const;

    // Pluck the translation right out of the world matrix so we don't have to
    // do any extra math.
    //
    [[nodiscard]] vec2
    world_position () const
    {
      return {world (0, 3), world (1, 3)};
    }
  };

  // Scene graph.
  //
  // We just stash everything in vectors to be cache-friendly. The active array
  // acts as a free list mask so we can quickly reuse slots when nodes get
  // destroyed instead of reallocating.
  //
  class scene_graph
  {
  public:
    scene_graph ();

    node_id
    create (std::string n = "");

    node_id
    create_child (node_id p, std::string n = "");

    void
    destroy (node_id id);

    scene_node*
    get (node_id id);

    node_id
    root () const { return root_; }

    // Walk the tree and bake down the world matrices. We only really need
    // to touch the branches that are currently marked dirty.
    //
    void
    update ();

  private:
    void
    update_node (scene_node& n, const mat4& p);

  private:
    node_id                 next_ {1};
    node_id                 root_ {invalid_node};
    std::vector<scene_node> nodes_;
    std::vector<bool>       active_;
  };
}

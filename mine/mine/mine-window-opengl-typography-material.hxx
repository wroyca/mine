#pragma once

// Typography Materials
//
// These are the GLSL shader sources used for rendering the text and UI
// elements. We keep them isolated here to avoid polluting the core
// rendering logic with multi-line string literals.

namespace mine
{
  namespace shaders
  {
    // Simple text vertex shader.
    //
    constexpr const char* text_vertex = R"glsl(
#version 330 core

layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in vec4 a_color;

out vec2 v_uv;
out vec4 v_color;

uniform mat4 u_projection;

void main()
{
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_uv = a_uv;
    v_color = a_color;
}
)glsl";

    // Simple text fragment shader (non-SDF).
    //
    constexpr const char* text_fragment = R"glsl(
#version 330 core

in vec2 v_uv;
in vec4 v_color;

out vec4 frag_color;

uniform sampler2D u_atlas;

void main()
{
    // The texture is R8 with swizzle: R,G,B -> 1, A -> RED.
    // So the glyph data (originally in R) is now in A channel.
    float alpha = texture(u_atlas, v_uv).a;
    frag_color = vec4(v_color.rgb, v_color.a * alpha);
}
)glsl";

    // SDF text fragment shader.
    //
    constexpr const char* text_sdf_fragment = R"glsl(
#version 330 core

in vec2 v_uv;
in vec4 v_color;

out vec4 frag_color;

uniform sampler2D u_atlas;
uniform float u_smoothing;
uniform float u_threshold;

void main()
{
    float distance = texture(u_atlas, v_uv).r;
    float alpha = smoothstep(u_threshold - u_smoothing, u_threshold + u_smoothing, distance);
    frag_color = vec4(v_color.rgb, v_color.a * alpha);
}
)glsl";

    // Selection/cursor fragment shader.
    //
    constexpr const char* solid_fragment = R"glsl(
#version 330 core

in vec2 v_uv;
in vec4 v_color;

out vec4 frag_color;

void main()
{
    frag_color = v_color;
}
)glsl";

    // Obviously move this elsewhere

    // SDF UI vertex shader.
    //
    constexpr const char* ui_vertex = R"glsl(
#version 330 core

layout (location = 0) in vec2 a_position;
layout (location = 1) in vec2 a_uv;
layout (location = 2) in vec4 a_color;
layout (location = 3) in vec2 a_dim;
layout (location = 4) in float a_radius;

out vec2 v_uv;
out vec4 v_color;
out vec2 v_dim;
out float v_radius;

uniform mat4 u_projection;

void main()
{
    gl_Position = u_projection * vec4(a_position, 0.0, 1.0);
    v_uv = a_uv;
    v_color = a_color;
    v_dim = a_dim;
    v_radius = a_radius;
}
)glsl";

    constexpr const char* ui_fragment = R"glsl(
#version 330 core

in vec2 v_uv;
in vec4 v_color;
in vec2 v_dim;
in float v_radius;

out vec4 frag_color;

float sdBox(vec2 p, vec2 b)
{
    // Evaluates distance covering all four quadrants symmetrically.
    vec2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

void main()
{
    // Map UVs [0..1] to local position [-width/2 .. width/2].
    vec2 p = (v_uv - 0.5) * v_dim;
    vec2 b = v_dim * 0.5;

    // Evaluate the SDF and apply corner radius.
    float d = sdBox(p, b - v_radius) - v_radius;

    // Perfect 1px anti-aliasing interpolation using the distance field.
    float alpha = 1.0 - smoothstep(-0.5, 0.5, d);

    frag_color = vec4(v_color.rgb, v_color.a * alpha);
}
)glsl";
  }
}

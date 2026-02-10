#version 300 es

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 face_tag;

out vec4 vertex_colour;

uniform mat4 projection_view_model;
uniform mat4 u_rotate;

void main() {
    gl_Position = u_rotate * (projection_view_model * vec4(position, 1.0f));
    vertex_colour = vec4(face_tag, 0.0f, 1.0f);
}

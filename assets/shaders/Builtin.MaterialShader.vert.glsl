#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_texcoord;

layout (set = 0, binding = 0) uniform GlobalUniformObject {
    mat4 projection;
    mat4 view;
} global_ubo;

layout (push_constant) uniform PushConstants {
    //only guaranteed to be minimum 128 bytes
    mat4 model;
} u_push_constants;

layout (location = 0) out int out_mode;

layout (location = 1) out dto {
    vec2 tex_coord;
} out_dto;

void main() {
    out_dto.tex_coord = in_texcoord;
    gl_Position = global_ubo.projection * global_ubo.view * u_push_constants.model * vec4(in_position, 1.0f);
}
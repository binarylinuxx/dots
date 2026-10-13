#version 440
layout(location = 0) in vec4 qt_VertexPosition;
layout(location = 1) in vec2 qt_VertexTexCoord;
layout(location = 0) out vec2 texcoord;
layout(std140, binding = 0) uniform buf { mat4 qt_Matrix; float qt_Opacity; float outerX; float outerY; float outerHX; float outerHY; float innerX; float innerY; float innerHX; float innerHY; float radius; float smoothing; float eigenvalue; int rectCount; vec4 innerRadii; vec4 color; vec4 outline; vec4 rects[16]; vec4 rectRadii[16]; };
void main() { gl_Position = qt_Matrix * qt_VertexPosition; texcoord = qt_VertexTexCoord; }

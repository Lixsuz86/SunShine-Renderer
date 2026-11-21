#version 450

// Binding 0: Per-vertex geometry
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;

// Binding 1: Per-instance data
layout(location = 4) in vec4 inModelCol0;
layout(location = 5) in vec4 inModelCol1;
layout(location = 6) in vec4 inModelCol2;
layout(location = 7) in vec4 inModelCol3;
layout(location = 8) in vec4 inPbrFactors;
layout(location = 9) in vec4 inColor;

layout(binding = 0) uniform CascadeUBO {
    mat4 lightSpaceMatrix;
} cascade;

void main() {
    mat4 model = mat4(inModelCol0, inModelCol1, inModelCol2, inModelCol3);
    gl_Position = cascade.lightSpaceMatrix * model * vec4(inPosition, 1.0);
}

#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;

layout(binding = 0) uniform ShadowUniforms {
    mat4 lightSpaceMatrix;
    mat4 modelMatrix;
};

void main() {
    gl_Position = lightSpaceMatrix * modelMatrix * vec4(inPosition, 1.0);
}

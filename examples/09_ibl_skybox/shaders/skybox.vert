#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 outUVW;

layout(binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
    vec4 lightDir;
    vec4 lightColor;
} scene;

void main() {
    outUVW = inPosition;
    // Remove camera translation from view matrix so skybox stays centered at camera
    mat4 viewNoTranslation = mat4(mat3(scene.view));
    vec4 clipPos = scene.proj * viewNoTranslation * vec4(inPosition, 1.0);
    // Setting z = w ensures clip-space depth = 1.0 (far clip plane)
    gl_Position = clipPos.xyww;
}

#version 450

layout(location = 0) in vec4 inPos;
layout(location = 1) in vec4 inVel;
layout(location = 2) in vec4 inCol;

layout(binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
} cam;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outDist;

void main() {
    vec4 viewPos = cam.view * vec4(inPos.xyz, 1.0);
    gl_Position = cam.proj * viewPos;

    float dist = max(-viewPos.z, 0.1);
    float pointSize = clamp((inVel.w * 900.0) / dist, 1.5, 20.0);
    gl_PointSize = pointSize;

    outColor = inCol;
    outDist = dist;
}

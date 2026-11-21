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
layout(location = 8) in vec4 inPbrFactors; // x: roughness, y: metallic, z: specular, w: tileScale
layout(location = 9) in vec4 inColor;      // baseColorFactor

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outViewPos;
layout(location = 2) out vec3 outNormal;
layout(location = 3) out vec2 outTexCoord;
layout(location = 4) out vec4 outColor;
layout(location = 5) out vec4 outPbrFactors;

layout(binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
} scene;

void main() {
    mat4 model = mat4(inModelCol0, inModelCol1, inModelCol2, inModelCol3);
    vec4 worldPos = model * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;

    vec4 viewPos = scene.view * worldPos;
    outViewPos = viewPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    outNormal = normalize(normalMatrix * inNormal);
    outTexCoord = inTexCoord * inPbrFactors.w;
    outColor = inColor;
    outPbrFactors = inPbrFactors;

    gl_Position = scene.proj * viewPos;
}

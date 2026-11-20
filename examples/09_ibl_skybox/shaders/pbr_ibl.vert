#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;
layout(location = 3) out vec3 outTangent;
layout(location = 4) out vec3 outBitangent;

layout(binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
    vec4 lightDir;
    vec4 lightColor;
} scene;

layout(binding = 1) uniform ModelUBO {
    mat4 model;
    vec4 baseColorFactor;
    vec4 pbrFactors; // x: metallic, y: roughness, z: occlusion, w: flags (useTextures)
} modelData;

void main() {
    vec4 worldPos = modelData.model * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    outTexCoord = inTexCoord;

    mat3 normalMatrix = transpose(inverse(mat3(modelData.model)));
    outNormal = normalize(normalMatrix * inNormal);

    vec3 T = normalize(normalMatrix * inTangent.xyz);
    // Gram-Schmidt orthogonalize
    T = normalize(T - dot(T, outNormal) * outNormal);
    vec3 B = cross(outNormal, T) * inTangent.w;

    outTangent = T;
    outBitangent = B;

    gl_Position = scene.proj * scene.view * worldPos;
}

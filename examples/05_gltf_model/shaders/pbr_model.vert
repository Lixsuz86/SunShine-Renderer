#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out mat3 fragTBN;

struct Light {
    vec4 position; // xyz = pos, w = intensity
    vec4 color;    // rgb = color, w = radius
};

layout(binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
    Light lights[4];
} scene;

layout(binding = 1) uniform ModelUBO {
    mat4 model;
    vec4 baseColorFactor;
    vec4 pbrFactors; // x: metallic, y: roughness, z: normalScale, w: occlusionStrength
} object;

void main() {
    vec4 worldPos = object.model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    fragTexCoord = inTexCoord;

    mat3 normalMatrix = transpose(inverse(mat3(object.model)));
    vec3 N = normalize(normalMatrix * inNormal);
    vec3 T = normalize(mat3(object.model) * inTangent.xyz);
    // Gram-Schmidt re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * inTangent.w;

    fragTBN = mat3(T, B, N);

    gl_Position = scene.proj * scene.view * worldPos;
}

#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inTangent;

layout(binding = 0) uniform SceneUniforms {
    mat4 view;
    mat4 proj;
    mat4 model;
    mat4 lightSpaceMatrix;
    vec3 camPos;
    float padding0;
    vec3 lightDir;
    float lightIntensity;
    vec3 lightColor;
    float ambientStrength;
};

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragTangent;
layout(location = 4) out vec3 fragBitangent;
layout(location = 5) out vec4 fragPosLightSpace;

void main() {
    vec4 worldPos = model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    fragTexCoord = inTexCoord;

    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMatrix * inNormal);
    vec3 T = normalize(normalMatrix * inTangent.xyz);
    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T) * inTangent.w;

    fragNormal = N;
    fragTangent = T;
    fragBitangent = B;

    // Light space position for shadow coordinate transformation
    fragPosLightSpace = lightSpaceMatrix * worldPos;

    gl_Position = proj * view * worldPos;
}

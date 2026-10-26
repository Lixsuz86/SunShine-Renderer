#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragTangent;
layout(location = 4) in vec3 fragBitangent;
layout(location = 5) in vec4 fragPosLightSpace;

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

layout(binding = 1) uniform sampler2D albedoMap;
layout(binding = 2) uniform sampler2D normalMap;
layout(binding = 3) uniform sampler2D metallicRoughnessMap;
layout(binding = 4) uniform sampler2D shadowMap;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// 3x3 PCF (Percentage Closer Filtering) Soft Shadows with slope-dependent bias
float CalculateShadow(vec4 fragPosLight, vec3 normal, vec3 lightD) {
    vec3 projCoords = fragPosLight.xyz / fragPosLight.w;

    // Convert from Vulkan NDC [-1, 1] to UV texture coordinates [0, 1]
    projCoords.x = projCoords.x * 0.5 + 0.5;
    projCoords.y = projCoords.y * 0.5 + 0.5;

    // Fragments outside the light frustum are not shadowed
    if (projCoords.z > 1.0 || projCoords.z < 0.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    // Slope-scaled depth bias to prevent shadow acne and self-shadow artifacts
    float bias = max(0.003 * (1.0 - dot(normal, lightD)), 0.0005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    return shadow;
}

void main() {
    vec4 albedoSample = texture(albedoMap, fragTexCoord);
    vec3 albedo = albedoSample.rgb;

    vec3 normalTex = texture(normalMap, fragTexCoord).rgb * 2.0 - 1.0;
    mat3 TBN = mat3(fragTangent, fragBitangent, fragNormal);
    vec3 N = normalize(TBN * normalTex);

    vec4 mrSample = texture(metallicRoughnessMap, fragTexCoord);
    float roughness = clamp(mrSample.g, 0.04, 1.0);
    float metallic = mrSample.b;

    vec3 V = normalize(camPos - fragWorldPos);
    vec3 L = normalize(lightDir);
    vec3 H = normalize(V + L);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 radiance = lightColor * lightIntensity;

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    float NdotL = max(dot(N, L), 0.0);
    vec3 directLighting = (kD * albedo / PI + specular) * radiance * NdotL;

    // Calculate real-time soft shadow attenuation
    float shadow = CalculateShadow(fragPosLightSpace, N, L);

    // Ambient lighting retains indirect illumination in shadowed areas
    vec3 ambient = ambientStrength * albedo;

    vec3 color = ambient + (1.0 - shadow) * directLighting;

    // ACES Filmic Tone Mapping
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    vec3 mapped = clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);

    // Gamma correction
    mapped = pow(mapped, vec3(1.0 / 2.2));

    outColor = vec4(mapped, 1.0);
}

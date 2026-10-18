#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in mat3 fragTBN;

layout(location = 0) out vec4 outColor;

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

layout(binding = 2) uniform sampler2D baseColorMap;
layout(binding = 3) uniform sampler2D normalMap;
layout(binding = 4) uniform sampler2D metallicRoughnessMap;

const float PI = 3.14159265359;

// GGX Normal Distribution
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / max(denom, 0.000001);
}

// Schlick-GGX Geometry
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.000001);
}

// Smith Geometry Shadowing
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel-Schlick
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ACES Filmic Tone Mapping curve (Narkowicz)
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    // 1. Sample Albedo & Material Properties
    vec4 texColor = texture(baseColorMap, fragTexCoord);
    vec3 albedo = pow(texColor.rgb, vec3(2.2)) * object.baseColorFactor.rgb; // Convert sRGB to linear

    vec4 mrSample = texture(metallicRoughnessMap, fragTexCoord);
    float roughness = clamp(mrSample.g * object.pbrFactors.y, 0.05, 1.0);
    float metallic  = clamp(mrSample.b * object.pbrFactors.x, 0.0, 1.0);

    // 2. Tangent-Space Normal Mapping
    vec3 tangentNormal = texture(normalMap, fragTexCoord).xyz * 2.0 - 1.0;
    tangentNormal.xy *= object.pbrFactors.z; // Apply normal scale
    vec3 N = normalize(fragTBN * tangentNormal);

    vec3 V = normalize(scene.camPos.xyz - fragWorldPos);

    // Base reflectivity F0
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // 3. Direct Lighting Accumulation
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < 4; ++i) {
        vec3 lightPos = scene.lights[i].position.xyz;
        float intensity = scene.lights[i].position.w;
        vec3 lightColor = scene.lights[i].color.rgb;

        vec3 L = normalize(lightPos - fragWorldPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPos - fragWorldPos);
        float attenuation = intensity / max(distance * distance, 0.01);
        vec3 radiance = lightColor * attenuation;

        // Cook-Torrance Specular BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);
        vec3 F    = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // 4. Ambient Lighting
    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = ambient + Lo;

    // 5. Post-Processing: Exposure + ACES Filmic + Gamma 2.2
    color *= 1.2; // Exposure
    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, texColor.a * object.baseColorFactor.a);
}

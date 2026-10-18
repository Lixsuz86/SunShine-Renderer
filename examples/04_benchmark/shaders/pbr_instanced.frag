#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec4 fragMaterial;
layout(location = 4) in vec4 fragColor;

layout(location = 0) out vec4 outColor;

struct Light {
    vec4 position; // xyz: world pos, w: intensity
    vec4 color;    // rgb: color, w: radius
};

layout(binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
    Light lights[4];
    vec4 postProcessParams; // x: enableTonemap (1/0), y: exposure, z: vignette, w: time
} ubo;

const float PI = 3.14159265359;

// Trowbridge-Reitz GGX Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / max(denom, 0.000001);
}

// Schlick-GGX Geometry Shadowing Function
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.000001);
}

// Smith's method for Geometry Shadowing
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ACES Filmic Tone Mapping curve (Narkowicz 2015)
vec3 ACESFilm(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 albedo = fragColor.rgb;
    float metallic = clamp(fragMaterial.x, 0.0, 1.0);
    float roughness = clamp(fragMaterial.y, 0.05, 1.0);
    float ao = clamp(fragMaterial.z, 0.0, 1.0);

    vec3 N = normalize(fragNormal);
    vec3 V = normalize(ubo.camPos.xyz - fragWorldPos);

    // Base reflectivity: 0.04 for dielectrics, albedo for metals
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Reflectance equation: accumulate radiance from all 4 dynamic lights
    vec3 Lo = vec3(0.0);

    for (int i = 0; i < 4; ++i) {
        vec3 lightPos = ubo.lights[i].position.xyz;
        float intensity = ubo.lights[i].position.w;
        vec3 lightColor = ubo.lights[i].color.rgb;

        vec3 L = normalize(lightPos - fragWorldPos);
        vec3 H = normalize(V + L);
        float distance = length(lightPos - fragWorldPos);
        float attenuation = intensity / max(distance * distance, 0.01);
        vec3 radiance = lightColor * attenuation;

        // Cook-Torrance BRDF
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= (1.0 - metallic);

        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Ambient lighting
    vec3 ambient = vec3(0.04) * albedo * ao;
    vec3 color = ambient + Lo;

    // --- Post-Processing Stage ---
    float enableTonemap = ubo.postProcessParams.x;
    float exposure = ubo.postProcessParams.y;
    float vignetteStrength = ubo.postProcessParams.z;

    // Exposure
    color *= exposure;

    if (enableTonemap > 0.5) {
        // 1. ACES Filmic Tone Mapping
        color = ACESFilm(color);

        // 2. Cinematic Vignette (darken edges based on screen coordinates)
        vec2 uv = fragTexCoord;
        float distFromCenter = length(uv - vec2(0.5));
        float vignette = smoothstep(0.75, 0.25, distFromCenter * vignetteStrength * 1.5);
        color *= mix(0.4, 1.0, vignette);

        // 3. Gamma Correction (sRGB gamma 2.2)
        color = pow(color, vec3(1.0 / 2.2));
    } else {
        // Without Tonemapping: Raw linear clamp with hard highlight clipping
        color = clamp(color, 0.0, 1.0);
    }

    outColor = vec4(color, 1.0);
}

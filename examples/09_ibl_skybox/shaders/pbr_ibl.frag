#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inTangent;
layout(location = 4) in vec3 inBitangent;

layout(location = 0) out vec4 outColor;

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
    vec4 pbrFactors; // x: metallic, y: roughness, z: occlusion, w: flags (useTextures > 0.5)
} modelData;

layout(binding = 2) uniform sampler2D albedoMap;
layout(binding = 3) uniform sampler2D normalMap;
layout(binding = 4) uniform sampler2D metallicRoughnessMap;
layout(binding = 5) uniform sampler2D aoMap;

// IBL Textures
layout(binding = 6) uniform samplerCube irradianceMap;
layout(binding = 7) uniform samplerCube prefilteredMap;
layout(binding = 8) uniform sampler2D brdfLUT;

const float PI = 3.14159265358979323846;

// GGX / Trowbridge-Reitz Normal Distribution Function
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return num / max(denom, 0.0000001);
}

// Schlick-GGX Geometric Shadowing Function
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / max(denom, 0.0000001);
}

// Smith Geometry Shadowing
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel-Schlick Approximation
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Fresnel-Schlick with Roughness for IBL
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ACES Filmic Tone Mapping Curve
vec3 ACESFilmicTonemap(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    bool useTextures = (modelData.pbrFactors.w > 0.5);

    // 1. Material Properties
    vec4 baseColor = modelData.baseColorFactor;
    float metallic = modelData.pbrFactors.x;
    float roughness = modelData.pbrFactors.y;
    float ao = modelData.pbrFactors.z;

    vec3 N = normalize(inNormal);

    if (useTextures) {
        vec4 texColor = texture(albedoMap, inTexCoord);
        baseColor *= texColor;

        // Tangent-Space Normal Mapping
        vec3 normalSample = texture(normalMap, inTexCoord).rgb * 2.0 - 1.0;
        mat3 TBN = mat3(normalize(inTangent), normalize(inBitangent), normalize(inNormal));
        N = normalize(TBN * normalSample);

        // Metallic-Roughness (glTF: Green = Roughness, Blue = Metallic)
        vec4 mr = texture(metallicRoughnessMap, inTexCoord);
        roughness *= mr.g;
        metallic *= mr.b;

        // Ambient Occlusion
        ao *= texture(aoMap, inTexCoord).r;
    }

    roughness = clamp(roughness, 0.04, 1.0);
    metallic = clamp(metallic, 0.0, 1.0);

    vec3 V = normalize(scene.camPos.xyz - inWorldPos);
    vec3 R = reflect(-V, N);

    // Surface reflection at zero incidence (F0)
    // 0.04 for typical dielectrics, tinted by baseColor for metals
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, baseColor.rgb, metallic);

    // -------------------------------------------------------------------------
    // 2. Direct Directional Light (Cook-Torrance BRDF)
    // -------------------------------------------------------------------------
    vec3 Lo = vec3(0.0);
    if (scene.lightColor.w > 0.01) {
        vec3 L = normalize(-scene.lightDir.xyz);
        vec3 H = normalize(V + L);

        float NdotL = max(dot(N, L), 0.0);
        float NdotV = max(dot(N, V), 0.0001);

        if (NdotL > 0.0) {
            float NDF = DistributionGGX(N, H, roughness);
            float G = GeometrySmith(N, V, L, roughness);
            vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

            vec3 numerator = NDF * G * F;
            float denominator = 4.0 * NdotV * NdotL + 0.0001;
            vec3 specular = numerator / denominator;

            vec3 kS = F;
            vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

            vec3 radiance = scene.lightColor.rgb * scene.lightColor.w;
            Lo += (kD * baseColor.rgb / PI + specular) * radiance * NdotL;
        }
    }

    // -------------------------------------------------------------------------
    // 3. Image-Based Lighting (IBL: Diffuse + Specular Split-Sum)
    // -------------------------------------------------------------------------
    float NdotV = max(dot(N, V), 0.0001);
    vec3 F_IBL = FresnelSchlickRoughness(NdotV, F0, roughness);

    vec3 kS_IBL = F_IBL;
    vec3 kD_IBL = (vec3(1.0) - kS_IBL) * (1.0 - metallic);

    // Diffuse Irradiance IBL
    vec3 irradiance = texture(irradianceMap, N).rgb;
    vec3 diffuseIBL = irradiance * baseColor.rgb;

    // Specular Pre-filtered IBL
    const float MAX_REFLECTION_LOD = 4.0;
    vec3 prefilteredColor = textureLod(prefilteredMap, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 envBRDF = texture(brdfLUT, vec2(NdotV, roughness)).rg;
    vec3 specularIBL = prefilteredColor * (F_IBL * envBRDF.x + envBRDF.y);

    vec3 ambient = (kD_IBL * diffuseIBL + specularIBL) * ao;

    // -------------------------------------------------------------------------
    // 4. Final Tone Mapping & Gamma Correction
    // -------------------------------------------------------------------------
    vec3 finalColor = ambient + Lo;

    // ACES Filmic Tone Mapping
    finalColor = ACESFilmicTonemap(finalColor);

    // Gamma correction to sRGB (gamma 2.2)
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    outColor = vec4(finalColor, baseColor.a);
}

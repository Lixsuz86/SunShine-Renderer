#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inViewPos;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec4 inColor;
layout(location = 5) in vec4 inPbrFactors;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform SceneUBO {
    mat4 view;
    mat4 proj;
    vec4 camPos;
} scene;

layout(binding = 1) uniform CSMUBO {
    mat4 lightSpaceMatrices[4];
    vec4 cascadeSplits; // Split depths in view space (x, y, z, w)
    vec4 lightDir;      // Direction towards light
    vec4 lightColor;    // rgb = color, w = intensity
    vec4 csmParams;     // x: biasMult, y: blendBand (1.0 or 0.0), z: debugColors (1.0 or 0.0), w: cascadeCount
} csm;

layout(binding = 2) uniform sampler2DArray shadowMapArray;
layout(binding = 3) uniform sampler2D albedoTexture;

// ACES Filmic Tone Mapping Curve
vec3 ACESFilmicTonemap(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// 3x3 Percentage Closer Filtering (PCF) on specific cascade layer
float SampleCascadeShadow(int layer, vec3 worldPos, vec3 normal, vec3 L) {
    vec4 sc = csm.lightSpaceMatrices[layer] * vec4(worldPos, 1.0);
    vec3 projCoords = sc.xyz / sc.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;

    // Check outside frustum
    if (projCoords.z > 1.0 || projCoords.z < 0.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 1.0;
    }

    // Adaptive slope-scale depth bias scaling with cascade distance
    float NdotL = max(dot(normal, L), 0.0);
    float baseBias = max(0.0015 * (1.0 - NdotL), 0.0003);
    float bias = baseBias * (1.0 + float(layer) * 0.75) * csm.csmParams.x;

    float currentDepth = projCoords.z - bias;

    // 3x3 PCF Kernel
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMapArray, 0).xy);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 offset = vec2(x, y) * texelSize;
            float pcfDepth = texture(shadowMapArray, vec3(projCoords.xy + offset, float(layer))).r;
            shadow += (currentDepth > pcfDepth ? 0.0 : 1.0);
        }
    }
    return shadow / 9.0;
}

void main() {
    vec3 N = normalize(inNormal);
    vec3 L = normalize(csm.lightDir.xyz);
    vec3 V = normalize(scene.camPos.xyz - inWorldPos);
    vec3 H = normalize(L + V);

    // 1. Determine active cascade from view-space depth
    float depthVal = abs(inViewPos.z);
    int cascadeIndex = 0;
    for (int i = 0; i < 3; ++i) {
        if (depthVal > csm.cascadeSplits[i]) {
            cascadeIndex = i + 1;
        }
    }

    // 2. Sample shadow for current cascade
    float shadow = SampleCascadeShadow(cascadeIndex, inWorldPos, N, L);

    // 3. Cascade Blend Band Transition
    bool blendEnabled = (csm.csmParams.y > 0.5);
    float blendAlpha = 1.0;

    if (blendEnabled && cascadeIndex < 3) {
        float prevSplit = (cascadeIndex == 0) ? 0.0 : csm.cascadeSplits[cascadeIndex - 1];
        float curSplit = csm.cascadeSplits[cascadeIndex];
        float range = curSplit - prevSplit;
        float bandSize = range * 0.20; // 20% transition band
        float distToBoundary = curSplit - depthVal;

        if (distToBoundary < bandSize) {
            blendAlpha = clamp(distToBoundary / bandSize, 0.0, 1.0);
            float nextShadow = SampleCascadeShadow(cascadeIndex + 1, inWorldPos, N, L);
            shadow = mix(nextShadow, shadow, blendAlpha);
        }
    }

    // 4. Material and Lighting
    vec4 texColor = texture(albedoTexture, inTexCoord);
    vec3 albedo = inColor.rgb * texColor.rgb;

    // Optional Cascade Debug Coloring
    bool debugColors = (csm.csmParams.z > 0.5);
    vec3 cascadeTint = vec3(1.0);
    if (debugColors) {
        const vec3 tints[4] = vec3[4](
            vec3(1.3, 0.4, 0.4), // Cascade 0: Red
            vec3(0.4, 1.3, 0.4), // Cascade 1: Green
            vec3(0.4, 0.5, 1.4), // Cascade 2: Blue
            vec3(1.3, 1.3, 0.3)  // Cascade 3: Yellow
        );

        cascadeTint = tints[cascadeIndex];
        if (blendEnabled && cascadeIndex < 3 && blendAlpha < 1.0) {
            cascadeTint = mix(tints[cascadeIndex + 1], tints[cascadeIndex], blendAlpha);
        }
    }

    // Ambient Lighting
    vec3 ambient = vec3(0.12, 0.15, 0.20) * albedo;

    // Direct Directional Lighting
    float NdotL = max(dot(N, L), 0.0);
    vec3 radiance = csm.lightColor.rgb * csm.lightColor.w;
    vec3 diffuse = albedo * radiance * NdotL;

    // Specular Highlight (Blinn-Phong)
    float NdotH = max(dot(N, H), 0.0);
    float shininess = mix(128.0, 8.0, inPbrFactors.x);
    float specFactor = pow(NdotH, shininess);
    vec3 specular = radiance * specFactor * inPbrFactors.z * NdotL;

    vec3 directLight = (diffuse + specular) * shadow;
    vec3 finalColor = (ambient + directLight) * cascadeTint;

    // Tone Mapping & Gamma Correction
    finalColor = ACESFilmicTonemap(finalColor);
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    outColor = vec4(finalColor, inColor.a);
}

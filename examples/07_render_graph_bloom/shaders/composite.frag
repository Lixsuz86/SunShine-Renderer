#version 450

layout(location = 0) in vec2 inUV;

layout(binding = 0) uniform sampler2D sceneHDRTexture;
layout(binding = 1) uniform sampler2D bloomTexture;

layout(binding = 2) uniform CompositeParams {
    float bloomIntensity;
    float exposure;
    int bloomEnabled;
    float padding;
};

layout(location = 0) out vec4 outColor;

vec3 ACESFilmic(vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 scene = texture(sceneHDRTexture, inUV).rgb;
    vec3 bloom = texture(bloomTexture, inUV).rgb;

    vec3 finalHDR = scene;
    if (bloomEnabled != 0) {
        finalHDR += bloom * bloomIntensity;
    }

    finalHDR *= exposure;

    vec3 mapped = ACESFilmic(finalHDR);
    mapped = pow(mapped, vec3(1.0 / 2.2));

    outColor = vec4(mapped, 1.0);
}

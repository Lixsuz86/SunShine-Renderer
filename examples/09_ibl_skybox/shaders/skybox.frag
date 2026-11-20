#version 450

layout(location = 0) in vec3 inUVW;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform samplerCube skyboxMap;

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
    vec3 envColor = texture(skyboxMap, inUVW).rgb;

    // HDR Exposure & Tonemapping
    envColor = ACESFilmicTonemap(envColor * 1.0);

    // Gamma correction to sRGB (gamma 2.2)
    envColor = pow(envColor, vec3(1.0 / 2.2));

    outColor = vec4(envColor, 1.0);
}

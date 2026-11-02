#version 450

layout(location = 0) in vec2 inUV;

layout(binding = 0) uniform sampler2D inputTexture;

layout(binding = 1) uniform BlurParams {
    vec2 texelSize;
    vec2 padding;
};

layout(location = 0) out vec4 outColor;

void main() {
    vec2 halfPixel = texelSize * 0.5;

    vec3 sum = vec3(0.0);
    sum += texture(inputTexture, inUV + vec2(-halfPixel.x * 2.0, 0.0)).rgb;
    sum += texture(inputTexture, inUV + vec2(-halfPixel.x, halfPixel.y)).rgb * 2.0;
    sum += texture(inputTexture, inUV + vec2(0.0, halfPixel.y * 2.0)).rgb;
    sum += texture(inputTexture, inUV + vec2(halfPixel.x, halfPixel.y)).rgb * 2.0;
    sum += texture(inputTexture, inUV + vec2(halfPixel.x * 2.0, 0.0)).rgb;
    sum += texture(inputTexture, inUV + vec2(halfPixel.x, -halfPixel.y)).rgb * 2.0;
    sum += texture(inputTexture, inUV + vec2(0.0, -halfPixel.y * 2.0)).rgb;
    sum += texture(inputTexture, inUV + vec2(-halfPixel.x, -halfPixel.y)).rgb * 2.0;

    outColor = vec4(sum * (1.0 / 12.0), 1.0);
}

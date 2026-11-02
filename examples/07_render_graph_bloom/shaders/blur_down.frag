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

    vec3 sum = texture(inputTexture, inUV).rgb * 4.0;
    sum += texture(inputTexture, inUV - halfPixel).rgb;
    sum += texture(inputTexture, inUV + halfPixel).rgb;
    sum += texture(inputTexture, inUV + vec2(halfPixel.x, -halfPixel.y)).rgb;
    sum += texture(inputTexture, inUV + vec2(-halfPixel.x, halfPixel.y)).rgb;

    outColor = vec4(sum * 0.125, 1.0);
}

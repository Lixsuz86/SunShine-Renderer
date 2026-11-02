#version 450

layout(location = 0) in vec2 inUV;

layout(binding = 0) uniform sampler2D inputTexture;

layout(binding = 1) uniform BrightParams {
    float threshold;
    float softKnee;
    vec2 padding;
};

layout(location = 0) out vec4 outBright;

void main() {
    vec3 color = texture(inputTexture, inUV).rgb;
    float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));

    float knee = max(softKnee, 0.0001);
    float soft = lum - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee);

    float weight = max(soft, lum - threshold) / max(lum, 0.0001);
    weight = clamp(weight, 0.0, 1.0);

    outBright = vec4(color * weight, 1.0);
}

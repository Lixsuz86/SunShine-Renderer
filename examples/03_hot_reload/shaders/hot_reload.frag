#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

// TIP: While this demo is running, edit this color, save the file, and watch it live update!
void main() {
    vec3 lightDir = normalize(vec3(1.0, 1.8, 1.2));
    vec3 norm = normalize(fragNormal);
    float diff = max(dot(norm, lightDir), 0.25);

    vec4 texColor = texture(texSampler, fragTexCoord);

    // Dynamic tint: Sunset Orange
    vec3 tint = vec3(1.0, 0.45, 0.1);
    
    vec3 finalRgb = mix(texColor.rgb, tint, 0.45) * diff;
    outColor = vec4(finalRgb, 1.0);
}

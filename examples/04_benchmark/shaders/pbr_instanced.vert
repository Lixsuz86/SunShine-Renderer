#version 450

// Binding 0: Per-vertex geometry
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// Binding 1: Per-instance data
layout(location = 3) in vec4 inModelCol0;
layout(location = 4) in vec4 inModelCol1;
layout(location = 5) in vec4 inModelCol2;
layout(location = 6) in vec4 inModelCol3;
layout(location = 7) in vec4 inMaterial; // x: metallic, y: roughness, z: ao, w: unused
layout(location = 8) in vec4 inColor;    // rgb: albedo, a: opacity

// Outputs to fragment shader
layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec4 fragMaterial;
layout(location = 4) out vec4 fragColor;

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

void main() {
    mat4 model = mat4(inModelCol0, inModelCol1, inModelCol2, inModelCol3);
    
    vec4 worldPos = model * vec4(inPosition, 1.0);
    fragWorldPos = worldPos.xyz;
    
    // Normal matrix: inverse transpose of 3x3 model
    mat3 normalMatrix = mat3(model);
    fragNormal = normalize(normalMatrix * inNormal);
    
    fragTexCoord = inTexCoord;
    fragMaterial = inMaterial;
    fragColor = inColor;
    
    gl_Position = ubo.proj * ubo.view * worldPos;
}

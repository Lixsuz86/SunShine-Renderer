#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in float inDist;

layout(location = 0) out vec4 outFragColor;

void main() {
    // Soft circular point sprite with gaussian falloff
    vec2 coord = gl_PointCoord - vec2(0.5);
    float distSq = dot(coord, coord);
    if (distSq > 0.25) {
        discard;
    }

    // Radial exponential falloff for cosmic glow
    float alpha = exp(-distSq * 9.0);
    outFragColor = vec4(inColor.rgb * alpha * 1.6, inColor.a * alpha);
}


#version 330 core

// Per-vertex attributes (base sphere geometry)
layout(location = 0) in vec3 in_Position;   // Unit sphere vertex position
layout(location = 1) in vec3 in_Normal;     // Unit sphere normal

// Per-instance attributes (one per atom)
layout(location = 2) in vec3 in_InstancePos;    // Atom world position
layout(location = 3) in float in_InstanceRadius; // Atom radius
layout(location = 4) in vec3 in_InstanceColor;   // Atom color (RGB)

// Uniforms
uniform mat4 u_ViewProjection;  // Camera view * projection matrix

// Output to fragment shader
out vec3 frag_WorldPos;
out vec3 frag_Normal;
out vec3 frag_Color;

void main() {
    // Scale unit sphere by radius and translate to atom position
    vec3 world_pos = in_InstancePos + in_Position * in_InstanceRadius;
    
    // Transform to clip space
    gl_Position = u_ViewProjection * vec4(world_pos, 1.0);
    
    // Pass to fragment shader
    frag_WorldPos = world_pos;
    frag_Normal = in_Normal;  // Already unit sphere normal (no scaling needed)
    frag_Color = in_InstanceColor;
}

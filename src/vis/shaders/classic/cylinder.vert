#version 330 core

// Per-vertex attributes (base cylinder geometry)
layout(location = 0) in vec3 in_Position;   // Unit cylinder vertex position
layout(location = 1) in vec3 in_Normal;     // Unit cylinder normal

// Per-instance attributes (one per bond)
layout(location = 2) in vec3 in_StartPos;   // Bond start position
layout(location = 3) in vec3 in_EndPos;     // Bond end position
layout(location = 4) in float in_Radius;    // Bond radius
layout(location = 5) in vec3 in_Color;      // Bond color (RGB)

// Uniforms
uniform mat4 u_ViewProjection;  // Camera view * projection matrix

// Output to fragment shader
out vec3 frag_WorldPos;
out vec3 frag_Normal;
out vec3 frag_Color;

// Compute rotation matrix to align Z-axis cylinder with bond direction
mat4 compute_cylinder_transform(vec3 start, vec3 end, float radius) {
    vec3 direction = end - start;
    float length = length(direction);
    
    if (length < 1e-6) {
        // Degenerate bond (zero length) - return identity
        return mat4(1.0);
    }
    
    vec3 axis = direction / length;  // Normalized bond direction
    
    // Rodrigues' rotation formula to align Z-axis with bond direction
    vec3 z_axis = vec3(0.0, 0.0, 1.0);
    vec3 rot_axis = cross(z_axis, axis);
    float rot_axis_len = length(rot_axis);
    
    mat4 rotation;
    if (rot_axis_len < 1e-6) {
        // Already aligned (or anti-aligned)
        if (dot(z_axis, axis) < 0.0) {
            // Anti-aligned: rotate 180° around X
            rotation = mat4(
                1.0,  0.0,  0.0, 0.0,
                0.0, -1.0,  0.0, 0.0,
                0.0,  0.0, -1.0, 0.0,
                0.0,  0.0,  0.0, 1.0
            );
        } else {
            rotation = mat4(1.0);  // Identity
        }
    } else {
        // General rotation
        rot_axis /= rot_axis_len;
        float cos_theta = dot(z_axis, axis);
        float sin_theta = rot_axis_len;
        
        // Rodrigues' formula: R = I + [K] sin(θ) + [K]² (1 - cos(θ))
        float c = cos_theta;
        float s = sin_theta;
        float t = 1.0 - c;
        vec3 k = rot_axis;
        
        rotation = mat4(
            t*k.x*k.x + c,     t*k.x*k.y - s*k.z, t*k.x*k.z + s*k.y, 0.0,
            t*k.x*k.y + s*k.z, t*k.y*k.y + c,     t*k.y*k.z - s*k.x, 0.0,
            t*k.x*k.z - s*k.y, t*k.y*k.z + s*k.x, t*k.z*k.z + c,     0.0,
            0.0,               0.0,               0.0,               1.0
        );
    }
    
    // Scale: radial (radius), axial (length)
    mat4 scale = mat4(
        radius, 0.0,    0.0,    0.0,
        0.0,    radius, 0.0,    0.0,
        0.0,    0.0,    length, 0.0,
        0.0,    0.0,    0.0,    1.0
    );
    
    // Translate to bond start
    mat4 translation = mat4(
        1.0, 0.0, 0.0, 0.0,
        0.0, 1.0, 0.0, 0.0,
        0.0, 0.0, 1.0, 0.0,
        start.x, start.y, start.z, 1.0
    );
    
    // Combine: translate * rotate * scale
    return translation * rotation * scale;
}

void main() {
    // Compute transformation matrix for this cylinder instance
    mat4 model = compute_cylinder_transform(in_StartPos, in_EndPos, in_Radius);
    
    // Transform cylinder vertex to world space
    vec4 world_pos = model * vec4(in_Position, 1.0);
    
    // Transform to clip space
    gl_Position = u_ViewProjection * world_pos;
    
    // Transform normal (rotation only, no scale/translation)
    vec3 direction = normalize(in_EndPos - in_StartPos);
    vec3 z_axis = vec3(0.0, 0.0, 1.0);
    vec3 rot_axis = cross(z_axis, direction);
    float rot_axis_len = length(rot_axis);
    
    vec3 world_normal;
    if (rot_axis_len < 1e-6) {
        // No rotation or 180° flip
        world_normal = (dot(z_axis, direction) < 0.0) ? -in_Normal : in_Normal;
    } else {
        // Rotate normal by same rotation as cylinder
        rot_axis /= rot_axis_len;
        float cos_theta = dot(z_axis, direction);
        float sin_theta = rot_axis_len;
        
        // Rodrigues' formula for normal rotation
        vec3 n = in_Normal;
        vec3 k = rot_axis;
        float c = cos_theta;
        float s = sin_theta;
        
        world_normal = n * c + cross(k, n) * s + k * dot(k, n) * (1.0 - c);
    }
    
    // Pass to fragment shader
    frag_WorldPos = world_pos.xyz;
    frag_Normal = normalize(world_normal);
    frag_Color = in_Color;
}

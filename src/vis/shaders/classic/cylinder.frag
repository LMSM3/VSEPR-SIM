#version 330 core

// Input from vertex shader
in vec3 frag_WorldPos;
in vec3 frag_Normal;
in vec3 frag_Color;

// Output color
out vec4 out_Color;

// Lighting uniforms (same as sphere shader)
uniform vec3 u_LightDir;        // Directional light direction (normalized)
uniform vec3 u_ViewPos;         // Camera position
uniform vec3 u_AmbientColor;    // Ambient light color
uniform vec3 u_LightColor;      // Directional light color
uniform float u_Shininess;      // Specular shininess (Phong exponent)

void main() {
    // Normalize interpolated normal
    vec3 N = normalize(frag_Normal);
    
    // Light direction
    vec3 L = u_LightDir;
    
    // View direction
    vec3 V = normalize(u_ViewPos - frag_WorldPos);
    
    // Blinn-Phong halfway vector
    vec3 H = normalize(L + V);
    
    // ========================================================================
    // Phong Lighting (identical to sphere shader for consistent appearance)
    // ========================================================================
    
    // Ambient term
    vec3 ambient = u_AmbientColor * frag_Color;
    
    // Diffuse term
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = u_LightColor * frag_Color * NdotL;
    
    // Specular term
    float NdotH = max(dot(N, H), 0.0);
    float specular_intensity = pow(NdotH, u_Shininess);
    vec3 specular = u_LightColor * specular_intensity * 0.3;
    
    // Combine
    vec3 final_color = ambient + diffuse + specular;
    
    // Gamma correction
    final_color = pow(final_color, vec3(1.0/2.2));
    
    out_Color = vec4(final_color, 1.0);
}

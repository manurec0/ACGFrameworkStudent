#version 330 core

in vec3 a_vertex;           // Vertex position in object space
in vec3 a_normal;           // Vertex normal
in vec4 a_color;            // Vertex color
in vec2 a_uv;               // Vertex UV coordinates

uniform mat4 u_model;
uniform mat4 u_viewprojection;
uniform vec3 u_camera_position;

out vec3 v_position;        // Position in object space
out vec3 v_world_position;  // Position in world space
out vec3 v_normal;          // Normal in world space
out vec4 v_color;           // Color from vertex
out vec2 v_uv;              // UV coordinates
out vec3 viewDir;           // View direction from camera to fragment

void main()
{	
    // Calculate the normal in world space
    v_normal = (u_model * vec4(a_normal, 0.0)).xyz;

    // Set vertex position in object space
    v_position = a_vertex;
    
    // Calculate world position of the vertex
    v_world_position = (u_model * vec4(v_position, 1.0)).xyz;

    // Set vertex color and UV coordinates
    v_color = a_color;
    v_uv = a_uv;

    // Calculate the view direction from the camera to the fragment
    viewDir = normalize(v_world_position - u_camera_position);

    // Calculate the position of the vertex in clip space
    gl_Position = u_viewprojection * vec4(v_world_position, 1.0);
}

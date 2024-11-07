#version 330 core

in vec3 v_world_position;  // World position passed from the vertex shader
in vec3 viewDir;           // View direction from the vertex shader

uniform vec3 u_camera_position;
uniform vec3 u_color;
uniform float u_absorption_coefficient;
uniform vec3 u_boxMin;
uniform vec3 u_boxMax;

//in vec3 ta; //starting position
//in vec3 tb; //ending position

//in u_absorption_coefficient;
//in u_viewprojection;
//in u_camera_position;
//in u_model;


//out vec4 u_color;
//out vec4 u_background_color;

out vec4 FragColor;

vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir, vec3 boxMin, vec3 boxMax) {
    vec3 tMin = (boxMin - rayOrigin) / rayDir;
    vec3 tMax = (boxMax - rayOrigin) / rayDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return vec2(tNear, tFar);
};

void main()
{
    // Ray setup
    //vec3 r = normalize(tb - ta);
    //float dist = length(frag_end - frag_start);

    //Initialize Ray
    vec3 rayOrigin = u_camera_position;
    vec3 rayDir = normalize(v_world_position - u_camera_position);

    //Intersect with Volume Bounding Box
    vec2 tValues = intersectAABB(rayOrigin, rayDir, u_boxMin, u_boxMax);
    float fragEnd = tValues.x;
    float fragStart = tValues.y;

    // Check if the ray intersects the volume
    if (fragEnd > fragStart || fragStart < 0.0) {
        discard;  // No intersection, discard the fragment
    }
    float dist = length(fragEnd - fragStart);

    float optical_thickness = dist * u_absorption_coefficient;

    float transmittance = exp(-optical_thickness);
   
    vec3 lightColor = u_color *transmittance;

    FragColor = vec4(lightColor, transmittance);
}
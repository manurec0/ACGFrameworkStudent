#version 330 core

uniform float u_start_position;    // Starting position (ta)
uniform float u_ending_position;    // Ending position (tb)

uniform vec3 u_camera_position;
uniform vec3 u_color;
uniform vec4 u_ambient_color;
uniform vec4 u_background_color;
uniform float u_absorption_coefficient;
uniform vec3 u_boxMin;
uniform vec3 u_boxMax;




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
    // Calculate the optical thickness
    vec3 B = vec3(u_background_color.x, u_background_color.y, u_background_color.z);
    float opticalThickness = u_absorption_coefficient * (u_ending_position - u_start_position);

    // Compute transmittance using Beer-Lambert Law
    float transmittance = exp(-opticalThickness);

    // Final color blending with background color
    vec4 volumeColor = u_ambient_color * transmittance;
    //vec4 finalColor = mix(u_background_color, volumeColor, transmittance);
    vec3 finalColor = B * transmittance;

    FragColor = vec4(finalColor, 1.0);
}
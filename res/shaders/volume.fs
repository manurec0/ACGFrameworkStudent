#version 330 core

in vec3 v_world_position;

//uniform float u_start_position;    // Starting position (ta)
//uniform float u_ending_position;    // Ending position (tb)

uniform mat4 u_model;
uniform vec3 u_camera_position;
uniform vec4 u_color;
uniform vec4 u_ambient_light;
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
    mat4 inverseModel = inverse(u_model);
    vec4 temp = vec4(u_camera_position, 1.0);
    temp *= inverseModel;
    vec3 local_camera_pos = vec3(temp.x / temp.w, temp.y / temp.w, temp.z / temp.w);

    // Compute ray direction
    vec3 rWorld = normalize(v_world_position - u_camera_position);
    vec3 r = normalize((inverseModel * vec4(rWorld, 0.0)).xyz);

    vec3 boxMin = vec3(-1.0, -1.0, -1.0);
    vec3 boxMax = vec3(1.0, 1.0, 1.0);

    vec2 intersection = intersectAABB(local_camera_pos, r, boxMin, boxMax);

    float ta = intersection.x;
    float tb = intersection.y;

    float opticalThickness = tb - ta;

    // Compute transmittance using Beer-Lambert Law
    float transmittance = exp(-u_absorption_coefficient * opticalThickness);

    //FragColor = vec4(finalColor, 1.0);

    FragColor = u_background_color * transmittance + u_color * (1.0 - transmittance);




}
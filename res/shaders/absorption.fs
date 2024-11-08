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
uniform int u_volume_type;
uniform float u_step_length;
uniform int u_noise_detail;
uniform float u_noise_scale;

out vec4 FragColor;

vec3 fade(vec3 t) {
    return t * t * t * (t * (t * 6.0 - 15.0) + 10.0);
}

float permute(float x) {
    return fract(sin(x) * 43758.5453123);
}

float gradientNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    
    f = f * f * (3.0 - 2.0 * f);

    // Hash each corner of the cube
    float n000 = permute(dot(i, vec3(1.0, 57.0, 113.0)));
    float n001 = permute(dot(i + vec3(0.0, 0.0, 1.0), vec3(1.0, 57.0, 113.0)));
    float n010 = permute(dot(i + vec3(0.0, 1.0, 0.0), vec3(1.0, 57.0, 113.0)));
    float n011 = permute(dot(i + vec3(0.0, 1.0, 1.0), vec3(1.0, 57.0, 113.0)));
    float n100 = permute(dot(i + vec3(1.0, 0.0, 0.0), vec3(1.0, 57.0, 113.0)));
    float n101 = permute(dot(i + vec3(1.0, 0.0, 1.0), vec3(1.0, 57.0, 113.0)));
    float n110 = permute(dot(i + vec3(1.0, 1.0, 0.0), vec3(1.0, 57.0, 113.0)));
    float n111 = permute(dot(i + vec3(1.0, 1.0, 1.0), vec3(1.0, 57.0, 113.0)));

    // Interpolate between each corner
    float x00 = mix(n000, n100, f.x);
    float x01 = mix(n001, n101, f.x);
    float x10 = mix(n010, n110, f.x);
    float x11 = mix(n011, n111, f.x);

    float y0 = mix(x00, x10, f.y);
    float y1 = mix(x01, x11, f.y);

    return mix(y0, y1, f.z) * 2.0 - 1.0;
}

float fractalPerlin(vec3 position, float scale, int detail) {
    float noiseValue = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < detail; i ++) {
        noiseValue += amplitude * gradientNoise(position * frequency * scale);
        frequency *= 2.0;
        amplitude *= 0.5;
    }

    return noiseValue;
}

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

    if(u_volume_type == 0){
        // Compute transmittance using Beer-Lambert Law
        float transmittance = exp(-u_absorption_coefficient * opticalThickness);

        //FragColor = vec4(finalColor, 1.0);

        FragColor = u_background_color * transmittance + u_color * (1.0 - transmittance);
    }

    else{
        float tau = 0.0;
        float t = ta + u_step_length / 2.0;

        //for (; t < tb; t += u_step_length) {
        //    vec3 P = local_camera_pos + r * t;
        //    float noiseValue = clamp(fractalPerlin(P, u_noise_scale, u_noise_detail), 0.0, 1.0);
        //    float localAbsorption = u_absorption_coefficient * (1.0 + noiseValue);
        //    tau += localAbsorption * u_step_length;
        //}
        while (t < tb){
            vec3 P = local_camera_pos + r * t;
            float noiseValue = clamp(fractalPerlin(P, u_noise_scale, u_noise_detail), 0.0, 1.0) * u_absorption_coefficient;
            //float localAbsorption = u_absorption_coefficient * noiseValue;
            tau += noiseValue * u_step_length;
            t += u_step_length;
        }

        float transmittance = exp(-tau);
        FragColor = u_background_color * transmittance + u_color * (1.0 - transmittance);

    }



}
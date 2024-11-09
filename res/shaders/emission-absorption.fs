#version 330 core

in vec3 v_world_position;

uniform mat4 u_model;
uniform vec3 u_camera_position;
uniform vec4 u_color;              // Emission color and strength
uniform vec4 u_ambient_light;
uniform vec4 u_background_color;
uniform float u_absorption_coefficient;
uniform float u_emission_coefficient;
uniform vec3 u_boxMin;
uniform vec3 u_boxMax;
uniform int u_volume_type;
uniform float u_step_length;
uniform int u_noise_detail;
uniform float u_noise_scale;

out vec4 FragColor;

// Noise function utilities
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

    float n000 = permute(dot(i, vec3(1.0, 57.0, 113.0)));
    float n001 = permute(dot(i + vec3(0.0, 0.0, 1.0), vec3(1.0, 57.0, 113.0)));
    float n010 = permute(dot(i + vec3(0.0, 1.0, 0.0), vec3(1.0, 57.0, 113.0)));
    float n011 = permute(dot(i + vec3(0.0, 1.0, 1.0), vec3(1.0, 57.0, 113.0)));
    float n100 = permute(dot(i + vec3(1.0, 0.0, 0.0), vec3(1.0, 57.0, 113.0)));
    float n101 = permute(dot(i + vec3(1.0, 0.0, 1.0), vec3(1.0, 57.0, 113.0)));
    float n110 = permute(dot(i + vec3(1.0, 1.0, 0.0), vec3(1.0, 57.0, 113.0)));
    float n111 = permute(dot(i + vec3(1.0, 1.0, 1.0), vec3(1.0, 57.0, 113.0)));

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

    for (int i = 0; i < detail; i++) {
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
}

void main() {
    mat4 inverseModel = inverse(u_model);
    vec3 local_camera_pos = (inverseModel * vec4(u_camera_position, 1.0)).xyz;

    vec3 rWorld = normalize(v_world_position - u_camera_position);
    vec3 r = normalize((inverseModel * vec4(rWorld, 0.0)).xyz);

    vec2 intersection = intersectAABB(local_camera_pos, r, u_boxMin, u_boxMax);
    float ta = intersection.x;
    float tb = intersection.y;

    if (ta > tb || tb < 0.0) {
        FragColor = u_background_color;
        return;
    }
    if(u_volume_type == 0){
        float opticalThickness = u_absorption_coefficient * (tb - ta);
        float transmittance = exp(-opticalThickness);

        // Compute the emission
        vec4 totalEmission = u_emission_coefficient * u_color * (1.0 - transmittance);

        FragColor = mix(u_background_color * transmittance, totalEmission, 1.0 - transmittance);
    }

    else{
        float tau = 0.0;
        vec4 accumulatedEmission = vec4(0.0);
        float t = ta + u_step_length / 2.0;
        while (t < tb) {
            vec3 P = local_camera_pos + r * t;
            float noiseValue = clamp(fractalPerlin(P, u_noise_scale, u_noise_detail), 0.0, 1.0) * u_absorption_coefficient;
            //float localAbsorption = u_absorption_coefficient * noiseValue;
            float transmittance = exp(-tau);

            // Compute the emission contribution at this step using u_color and noise
            vec4 localEmission = u_emission_coefficient * u_color * noiseValue;

            // Accumulate the emission contribution modulated by the transmittance
            accumulatedEmission += localEmission * transmittance * u_step_length;

            tau += noiseValue * u_step_length;
            t += u_step_length;
        }

        float finalTransmittance = exp(-tau);
        FragColor = mix(u_background_color * finalTransmittance, accumulatedEmission, 1.0 - finalTransmittance);
    }


}

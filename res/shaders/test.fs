#version 330 core

in vec3 v_world_position;

// Uniforms for transformations and volume parameters
uniform mat4 u_model;
uniform vec3 u_camera_position;
uniform vec4 u_color;
uniform vec4 u_ambient_light;
uniform vec4 u_background_color;
uniform float u_absorption_coefficient;
uniform vec3 u_boxMin;
uniform vec3 u_boxMax;
uniform int u_density_source;  // 0: constant, 1: noise
uniform float u_constant_density;  // Density for constant mode
uniform float u_noise_scale;       // Scale for procedural noise
uniform int u_noise_detail;        // Detail level for procedural noise
uniform float u_step_length;
uniform sampler3D u_density_texture;  // 3D texture for VDB
uniform float u_density_scale;        // Scale for density values


//light uniforms
uniform float u_light_intensity;
uniform float u_light_shininess;
uniform vec4 u_light_color;
uniform vec3 u_light_direction;
uniform vec3 u_light_position;
uniform vec3 u_local_light_position;

// Output color
out vec4 FragColor;

// Noise function utilities (for 3D noise-based density)
float fractalPerlin(vec3 position, float scale, int detail) {
    float noiseValue = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < detail; i++) {
        noiseValue += amplitude * fract(sin(dot(position * frequency * scale, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
        frequency *= 2.0;
        amplitude *= 0.5;
    }

    return noiseValue;
}

// Function to compute ray-box intersection
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
    // Transform camera position to local space
    mat4 inverseModel = inverse(u_model);
    vec3 local_camera_pos = (inverseModel * vec4(u_camera_position, 1.0)).xyz;

    // Compute ray direction
    vec3 rWorld = normalize(v_world_position - u_camera_position);
    vec3 r = normalize((inverseModel * vec4(rWorld, 0.0)).xyz);

    // Intersect ray with the bounding box
    vec2 intersection = intersectAABB(local_camera_pos, r, u_boxMin, u_boxMax);
    float ta = intersection.x;
    float tb = intersection.y;

    // If no intersection, return background color
    if (ta > tb || tb < 0.0) {
        FragColor = u_background_color;
        return;
    }

    // Initialize variables for ray marching
    float tau = 0.0;
    float t = ta + u_step_length / 2.0;

    // Handle density sources
    if (u_density_source == 0) {
        float opticalThickness = tb - ta;

        float transmittance = exp(-u_absorption_coefficient * opticalThickness);

        FragColor = u_background_color * transmittance;

    } else if (u_density_source == 1) {
        float tau = 0.0;
        float t = ta + u_step_length / 2.0;

        while (t < tb){
            vec3 P = local_camera_pos + r * t;
            float noiseValue = clamp(fractalPerlin(P, u_noise_scale, u_noise_detail), 0.0, 1.0) * u_absorption_coefficient;

            tau += noiseValue * u_step_length;
            t += u_step_length;
        }

        float transmittance = exp(-tau);
        FragColor = u_background_color * transmittance + u_color * (1.0 - transmittance);
    } else if (u_density_source == 2) {
        while (t < tb) {
            vec3 P = local_camera_pos + r * t;

            // Compute normalized texture coordinates
            vec3 texCoords = (P - u_boxMin) / (u_boxMax - u_boxMin);

            // Sample density from the 3D texture
            float density = texture(u_density_texture, texCoords).r * u_density_scale;

            // Accumulate optical thickness
            tau += density * u_absorption_coefficient * u_step_length;

            // Break early if transmittance becomes negligible
            if (exp(-tau) < 0.01) {
                break;
            }

            t += u_step_length;
        }

        // Compute final transmittance
        float transmittance = exp(-tau);

        // Final color combining background and material color
        FragColor = u_background_color * transmittance + u_color * (1.0 - transmittance);
    }
}

#version 330 core

in vec3 v_world_position;

// Uniforms for transformations and volume parameters
uniform mat4 u_model;
uniform vec3 u_camera_position;
uniform mat4 u_viewprojection;
uniform vec3 u_boxMin;
uniform vec3 u_boxMax;

// Uniforms for density sampling
uniform int u_density_source;         // 0: constant, 1: noise, 2: 3D texture
uniform sampler3D u_density_texture;  // 3D texture for VDB
uniform float u_density_scale;        // Scale for density values
uniform float u_constant_density;     // For constant density
uniform float u_noise_scale;          // Scale for procedural noise
uniform int u_noise_detail;           // Detail level for procedural noise

// Rendering parameters
uniform float u_absorption_coefficient;
uniform float u_step_length;

// Output color
out vec4 FragColor;

// Noise function utilities (for noise-based density)
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

// Function to sample density based on the source
float sampleDensity(vec3 position) {
    if (u_density_source == 0) {
        // Constant density
        return u_constant_density;
    } else if (u_density_source == 1) {
        // Procedural noise
        return clamp(fractalPerlin(position, u_noise_scale, u_noise_detail), 0.0, 1.0);
    } else if (u_density_source == 2) {
        // 3D texture sampling
        vec3 texCoords = (position - u_boxMin) / (u_boxMax - u_boxMin); // Normalize to [0,1]
        return texture(u_density_texture, texCoords).r * u_density_scale;
    }
    return 0.0;
}

void main() {
    // Compute ray direction and starting position
    mat4 inverseModel = inverse(u_model);
    vec3 local_camera_pos = (inverseModel * vec4(u_camera_position, 1.0)).xyz;

    vec3 rWorld = normalize(v_world_position - u_camera_position);
    vec3 r = normalize((inverseModel * vec4(rWorld, 0.0)).xyz);

    // Intersect the ray with the volume's bounding box
    vec3 tMin = (u_boxMin - local_camera_pos) / r;
    vec3 tMax = (u_boxMax - local_camera_pos) / r;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float ta = max(max(t1.x, t1.y), t1.z);
    float tb = min(min(t2.x, t2.y), t2.z);

    // If the ray does not intersect the volume
    if (ta > tb || tb < 0.0) {
        FragColor = vec4(0.0); // Transparent background
        return;
    }

    // Ray marching loop
    float t = ta;                // Start at the entry point
    float tau = 0.0;             // Accumulated optical thickness
    const int maxSteps = 256;    // Maximum number of steps
    int stepCount = 0;           // Step counter

    while (t < tb && stepCount < maxSteps) {
        // Compute the current sample position in the volume
        vec3 P = local_camera_pos + r * t;

        // Sample the density at the current position
        float density = sampleDensity(P);

        // Accumulate optical thickness
        tau += u_absorption_coefficient * density * u_step_length;

        // Break if the transmittance is near zero (all light absorbed)
        if (exp(-tau) < 0.01) {
            break;
        }

        // Advance to the next step
        t += u_step_length;
        stepCount++;
    }

    // Compute transmittance
    float transmittance = exp(-tau);

    // Visualize the transmittance
    FragColor = vec4(vec3(1.0 - transmittance), 1.0); // Inverse transmittance as grayscale
}

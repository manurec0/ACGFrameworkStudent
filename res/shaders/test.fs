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
uniform float u_scattering_coefficient;
uniform int u_max_light_steps;


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
    vec4 accumulatedScattering = vec4(0.0);


    if (u_density_source == 0) {
        // Ray marching loop
        while (t < tb) {
            vec3 P = local_camera_pos + r * t; // Current sample position

            float density = u_constant_density;
            float extinction = density * (u_absorption_coefficient + u_scattering_coefficient);
            tau += extinction * u_step_length;

            // Compute transmittance
            float transmittance = exp(-tau);

            // Initialize secondary ray to the light source
            vec3 lightDir = normalize(u_light_position - P);
            vec3 currentLightPos = P + lightDir * u_step_length / 2.0;

            // Compute light contribution
            float lightTau = 0.0;
            vec3 lightAccumulation = vec3(0.0);
            //int maxLightSteps = 100; // Limit light marching steps
            int maxLightSteps = u_max_light_steps;

            //Second Ray Marching Loop
            for (int step = 0; step < maxLightSteps; step++) {
                vec3 texCoords = (currentLightPos - u_boxMin) / (u_boxMax - u_boxMin);
                if (any(lessThan(texCoords, vec3(0.0))) || any(greaterThan(texCoords, vec3(1.0))))
                    break;

                // Accumulate optical thickness along the light ray
                lightTau += u_constant_density * u_scattering_coefficient * u_step_length;
                currentLightPos += lightDir * u_step_length;
        }

            // Compute Ls
            vec4 Ls = u_light_color * exp(-lightTau);

            // Accumulate scattering
            accumulatedScattering += u_scattering_coefficient * Ls * transmittance * density * u_step_length;

            // Advance the ray
            t += u_step_length;
        }

        // Combine transmittance, scattering, and background color
        float finalTransmittance = exp(-tau);
        //vec3 finalColor = u_background_color.rgb * finalTransmittance + accumulatedScattering;

        FragColor = u_background_color * finalTransmittance + accumulatedScattering;

    } else if (u_density_source == 1) {

        while (t < tb){
            vec3 P = local_camera_pos + r * t;
            float noiseValue = clamp(fractalPerlin(P, u_noise_scale, u_noise_detail), 0.0, 1.0) * u_absorption_coefficient;

            float extinction = noiseValue * (u_absorption_coefficient + u_scattering_coefficient); //NOise is density 
            tau += extinction * u_step_length;

            // Compute transmittance
            float transmittance = exp(-tau);

            vec3 lightDir = normalize(u_light_position - P);
            vec3 currentLightPos = P + lightDir * u_step_length / 2.0;

            // Compute light contribution
            float lightTau = 0.0;
            vec3 lightAccumulation = vec3(0.0);
            for (int step = 0; step < u_max_light_steps; step++) {
                vec3 texCoords = (currentLightPos - u_boxMin) / (u_boxMax - u_boxMin);
                if (any(lessThan(texCoords, vec3(0.0))) || any(greaterThan(texCoords, vec3(1.0))))
                    break;

                // Sample density along the light ray
                float lightDensity = clamp(fractalPerlin(currentLightPos, u_noise_scale, u_noise_detail), 0.0, 1.0);
                lightTau += lightDensity * u_scattering_coefficient * u_step_length;

                // Advance the light ray
                currentLightPos += lightDir * u_step_length;
            }

            // Compute scattered light contribution
            vec4 Ls = u_light_color * exp(-lightTau);

            // Accumulate scattering
            accumulatedScattering += u_scattering_coefficient * Ls * transmittance * noiseValue * u_step_length;

            t += u_step_length;
        }

        float transmittance = exp(-tau);
        FragColor = u_background_color * transmittance + accumulatedScattering;
    } else if (u_density_source == 2) {
        while (t < tb) {
            vec3 P = local_camera_pos + r * t;

            // Compute normalized texture coordinates
            vec3 texCoords = (P - u_boxMin) / (u_boxMax - u_boxMin);

            // Sample density from the 3D texture
            float density = texture(u_density_texture, texCoords).r * u_density_scale;

            float extinction = density * (u_absorption_coefficient + u_scattering_coefficient);

            // Accumulate optical thickness
            tau += extinction * u_absorption_coefficient * u_step_length;

            float transmittance = exp(-tau);

            vec3 lightDir = normalize(u_light_position - P);
            vec3 currentLightPos = P + lightDir * u_step_length / 2.0;

            // Compute light contribution
            float lightTau = 0.0;
            vec3 lightAccumulation = vec3(0.0);
            for (int step = 0; step < u_max_light_steps; step++) {
                vec3 lightTexCoords = (currentLightPos - u_boxMin) / (u_boxMax - u_boxMin);
                if (any(lessThan(lightTexCoords, vec3(0.0))) || any(greaterThan(lightTexCoords, vec3(1.0))))
                    break;

                // Sample density along the light ray
                float lightDensity = texture(u_density_texture, lightTexCoords).r * u_density_scale;
                lightTau += lightDensity * u_scattering_coefficient * u_step_length;

                // Advance the light ray
                currentLightPos += lightDir * u_step_length;
            }

            // Compute scattered light contribution
            vec4 Ls = u_light_color * exp(-lightTau);

            // Accumulate scattering
            accumulatedScattering += u_scattering_coefficient * Ls * transmittance * density * u_step_length;

            // Break early if transmittance becomes negligible
            if (exp(-tau) < 0.01) {
                break;
            }

            t += u_step_length;
        }

        // Compute final transmittance
        float transmittance = exp(-tau);

        // Final color combining background and material color
        FragColor = u_background_color * transmittance + accumulatedScattering;
    }
}

#version 120

// Material properties
uniform vec3 ka;
uniform vec3 kd;
uniform vec3 ks;
uniform float s;

// Task 3: Light arrays from main.cpp
uniform vec3 lightPos[2]; // Light positions in eye space
uniform vec3 lightCol[2]; // Light colors (Li)

varying vec3 vPosEye;
varying vec3 vNorEye;

void main() {
    vec3 n = normalize(vNorEye);
    vec3 v = normalize(-vPosEye); // Vector towards the camera
    
    // Start with Ambient: R = Ar
    vec3 finalColor = ka;

    // Summation loop for n lights
    for(int i = 0; i < 2; i++) {
        // Calculate Light (L) and Halfway (H) vectors for this specific light
        vec3 l = normalize(lightPos[i] - vPosEye);
        vec3 h = normalize(l + v);

        // Diffuse response: Di = kd * max(0, n·l)
        float nDotL = max(0.0, dot(n, l));
        vec3 D = kd * nDotL;

        // Specular response: Si = ks * max(0, n·h)^s
        float nDotH = max(0.0, dot(n, h));
        vec3 S = ks * pow(nDotH, s);

        // Apply light color component-wise: Li * (Di + Si)
        // Then add to the total color sum
        finalColor += lightCol[i] * (D + S);
    }

    gl_FragColor = vec4(finalColor, 1.0);
}
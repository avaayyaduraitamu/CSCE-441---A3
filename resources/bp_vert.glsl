#version 120

uniform mat4 P;    // Add this!
uniform mat4 MV;
uniform mat4 MVit; 

attribute vec4 aPos;
attribute vec3 aNor;
attribute vec2 aTex;

varying vec3 vPosEye;
varying vec3 vNorEye;

void main() {
    // 1. Transform position to Eye Space
    vPosEye = vec3(MV * aPos);
    
    // 2. Transform normal to Eye Space using Inverse Transpose
    // This is the "Magic" that makes the sheared teapot look right!
    vNorEye = vec3(MVit * vec4(aNor, 0.0));
    
    // 3. Keep aTex active (Prevents 'Attribute not found' warnings)
    float dummy = (aTex.x + aTex.y) * 0.0;
    
    // 4. Final screen position
    gl_Position = P * MV * aPos + dummy;
}
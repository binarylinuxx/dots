#version 300 es
precision mediump float;
in vec2 v_texcoord;
uniform sampler2D tex;
out vec4 fragColor;

// Color temperature in Kelvin — set via sed before applying
// TEMPERATURE placeholder replaced at runtime
#define TEMPERATURE 3400.0
#define STRENGTH 0.8

// Converts a color temperature (Kelvin) to an RGB multiplier
// Based on Tanner Helland's algorithm
vec3 colorTempToRGB(float temp) {
    temp = clamp(temp, 1000.0, 12000.0) / 100.0;

    float r, g, b;

    // Red
    if (temp <= 66.0) {
        r = 1.0;
    } else {
        r = temp - 60.0;
        r = 329.698727446 * pow(r, -0.1332047592);
        r = clamp(r / 255.0, 0.0, 1.0);
    }

    // Green
    if (temp <= 66.0) {
        g = temp;
        g = 99.4708025861 * log(g) - 161.1195681661;
        g = clamp(g / 255.0, 0.0, 1.0);
    } else {
        g = temp - 60.0;
        g = 288.1221695283 * pow(g, -0.0755148492);
        g = clamp(g / 255.0, 0.0, 1.0);
    }

    // Blue
    if (temp >= 66.0) {
        b = 1.0;
    } else if (temp <= 19.0) {
        b = 0.0;
    } else {
        b = temp - 10.0;
        b = 138.5177312231 * log(b) - 305.0447927307;
        b = clamp(b / 255.0, 0.0, 1.0);
    }

    return vec3(r, g, b);
}

void main() {
    vec4 color = texture(tex, v_texcoord);
    vec3 tempRGB = colorTempToRGB(TEMPERATURE);
    vec3 shifted = color.rgb * mix(vec3(1.0), tempRGB, STRENGTH);
    fragColor = vec4(shifted, color.a);
}

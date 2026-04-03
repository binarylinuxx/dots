// melt.frag
#version 300 es
precision mediump float;

in vec2 v_texcoord;
uniform sampler2D tex;
uniform float time;

out vec4 fragColor;

void main() {
    vec2 uv = v_texcoord;

    // Dripping effect - pixels slide down based on sine pattern
    float drip_speed = 0.15;
    float drip = sin(uv.x * 20.0 + time * 0.5) * sin(uv.x * 7.0 - time * 0.3);
    drip = max(0.0, drip); // Only drip downward

    float melt_amount = drip * drip_speed * (1.0 - uv.y); // More melting at top
    uv.y = uv.y - melt_amount * sin(time * 0.5) * sin(time * 0.5);

    // Slight horizontal wobble
    uv.x += sin(uv.y * 5.0 + time) * 0.005;

    uv = clamp(uv, 0.0, 1.0);

    vec4 color = texture(tex, uv);
    fragColor = color;
}

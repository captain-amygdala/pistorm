#version 100

precision mediump float;

const int colors = 8;

varying vec2 fragTexCoord;
varying vec4 fragColor;

uniform sampler2D texture0;
uniform sampler2D texture1;

uniform vec4 colDiffuse;

uniform ivec3 palette[colors];

void main()
{
    vec4 texelColor = texture2D(texture0, fragTexCoord);

    vec4 color = vec4(1.0, 1.0, 1.0, 1.0);
    vec2 bukCoord = vec2(texelColor.r, 0.5);

    if (texelColor.r == 1.0) {
        bukCoord = vec2(0.9999, 0.5);
    }
    vec4 colorx = texture2D(texture1, bukCoord);
    
    gl_FragColor = vec4(colorx.r, colorx.g, colorx.b, colorx.a);
}

#version 430 core
out vec4 FragColor;

in vec2 vLocalPos;
void main() {
    vec2 N_coords = vLocalPos * 2.f;
    float r2 = dot(N_coords, N_coords);

    if (r2 > 1.f) discard;

    float alpha = 1.f - r2;
    vec3 waterColor = vec3(0.1f, 0.55f, 0.95f);

    float core = pow(1.f - sqrt(r2), 3.f);
    vec3 finalColor = mix(waterColor, vec3(1.f), core * 0.4f);

    FragColor = vec4(finalColor, alpha);
}
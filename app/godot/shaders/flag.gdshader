// A flag's cloth in the wind: a subdivided plane whose free end ripples
// more than the end on the pole (UV.x 0 at the pole, 1 at the fly end).
shader_type spatial;
render_mode cull_disabled;

uniform vec4 colour : source_color = vec4(0.9, 0.2, 0.15, 1.0);
uniform float phase = 0.0;
// Above 0, a black-and-white chequered flag this many squares long.
uniform float chequers = 0.0;

void vertex() {
	float free_share = UV.x;
	float wave = sin(TIME * 5.0 - UV.x * 7.0 + phase) + 0.4 * sin(TIME * 8.3 - UV.x * 11.0);
	VERTEX.z += wave * 0.08 * free_share;
	VERTEX.y -= 0.06 * free_share * free_share;
}

void fragment() {
	// Folds catch the light: darker in the troughs.
	float fold = 0.85 + 0.15 * sin(TIME * 5.0 - UV.x * 7.0 + phase);
	vec3 base = colour.rgb;
	if (chequers > 0.0) {
		// Square checks: the cloth is two thirds as high as it is long.
		float squares = floor(UV.x * chequers) + floor(UV.y * chequers * 2.0 / 3.0);
		base = mod(squares, 2.0) < 0.5 ? vec3(0.95) : vec3(0.05);
	}
	ALBEDO = base * fold;
	ROUGHNESS = 0.9;
}

// One person (People): the vertex colour's alpha says what each part is,
// 0 the shirt, 0.5 skin, 1 a fixed colour (jeans, shoes, hair).
shader_type spatial;

uniform vec4 shirt : source_color = vec4(0.2, 0.4, 0.8, 1.0);
// 0 fair to 1 dark.
uniform float skin = 0.3;

// Skin from fair to dark, and the vertex colours, are written as sRGB
// like every colour in GDScript; to_linear brings them to the light maths'
// space, as `source_color` does for a uniform.
const vec3 FAIR = vec3(0.93, 0.76, 0.64);
const vec3 DARK = vec3(0.42, 0.28, 0.2);

vec3 to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

varying vec4 part;

void vertex() {
	part = COLOR;
}

void fragment() {
	vec3 colour = to_linear(part.rgb);
	if (part.a < 0.25) {
		// Already linear: `source_color` converted it.
		colour = shirt.rgb;
	} else if (part.a < 0.75) {
		colour = to_linear(mix(FAIR, DARK, skin));
	}
	ALBEDO = colour;
	ROUGHNESS = 0.85;
}

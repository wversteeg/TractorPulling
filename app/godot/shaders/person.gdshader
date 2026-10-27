// One person (People): the vertex colour's alpha says what each part is,
// 0 the shirt, 0.5 skin, 1 a fixed colour (jeans, shoes, hair).
shader_type spatial;

uniform vec4 shirt : source_color = vec4(0.2, 0.4, 0.8, 1.0);
// 0 fair to 1 dark.
uniform float skin = 0.3;

const vec3 FAIR = vec3(0.93, 0.76, 0.64);
const vec3 DARK = vec3(0.42, 0.28, 0.2);

varying vec4 part;

void vertex() {
	part = COLOR;
}

void fragment() {
	vec3 colour = part.rgb;
	if (part.a < 0.25) {
		colour = shirt.rgb;
	} else if (part.a < 0.75) {
		colour = mix(FAIR, DARK, skin);
	}
	ALBEDO = colour;
	ROUGHNESS = 0.85;
}

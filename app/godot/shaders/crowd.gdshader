// The crowd: every person a copy of a People mesh in one MultiMesh, with
// their own shirt (INSTANCE_CUSTOM.rgb) and skin (its alpha, 0 fair to 1
// dark), and moving on their own: a slow sway, and little hops that grow
// with `excitement` (0 calm, 1 a full pull coming). Each copy takes its
// phase and pace from its instance id, so neighbours never move as one.
// The vertex colour's alpha says what each part is, as in person.gdshader.
shader_type spatial;

uniform float excitement : hint_range(0.0, 1.0) = 0.2;

// Skin from fair to dark, and the vertex colours, are written as sRGB
// like every colour in GDScript; to_linear brings them to the light maths'
// space, as `source_color` does for a uniform.
const vec3 FAIR = vec3(0.93, 0.76, 0.64);
const vec3 DARK = vec3(0.42, 0.28, 0.2);

vec3 to_linear(vec3 c) {
	return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), c));
}

varying vec4 part;
varying vec4 person;

float random(float seed) {
	return fract(sin(seed * 12.9898) * 43758.5453);
}

void vertex() {
	float id = float(INSTANCE_ID);
	float phase = random(id) * 6.2831;
	float pace = 5.0 + 4.0 * random(id + 7.0);
	// Only a share of the crowd jumps, more as it gets louder.
	float jumps = step(random(id + 3.0), 0.25 + 0.6 * excitement);
	float hop = max(0.0, sin(TIME * pace + phase));
	VERTEX.y += hop * hop * (0.02 + 0.14 * excitement * jumps);
	// The sway is about the feet: more at the head.
	VERTEX.x += sin(TIME * 1.3 + phase) * 0.03 * VERTEX.y;
	part = COLOR;
	person = INSTANCE_CUSTOM;
}

void fragment() {
	vec3 colour = part.rgb;
	if (part.a < 0.25) {
		colour = person.rgb;
	} else if (part.a < 0.75) {
		colour = mix(FAIR, DARK, person.a);
	}
	ALBEDO = to_linear(colour);
	ROUGHNESS = 0.85;
}

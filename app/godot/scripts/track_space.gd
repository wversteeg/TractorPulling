# The simulation's axes in Godot's. The simulation works on the track: x
# down the track from the start line, y to the left, z up (docs/conventions.md).
# Godot's 3D is y up with -z forward, so x becomes -z and y becomes -x;
# headings (positive turning left) are rotations about Godot's y, and a
# nose-up pitch is a positive rotation about the body's x.
class_name TrackSpace


static func point(x_m: float, y_m: float, z_m: float) -> Vector3:
	return Vector3(-y_m, z_m, -x_m)

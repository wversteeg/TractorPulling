# The chain between the tractor's hitch and the sled's hook: steel links
# strung between them, each turned a quarter from the last, darker the
# harder it pulls. One MultiMesh for all the links.
class_name ChainView
extends Node3D

const LINK_LENGTH_M := 0.11
const LINK_THICKNESS_M := 0.018
const SLACK_COLOUR := Color(0.62, 0.62, 0.64)
const TAUT_COLOUR := Color(0.2, 0.2, 0.22)
# The tension at which it is drawn fully taut [N].
const TAUT_N := 40_000.0
# Links drawn at most: a span of up to 2 m in links of their own length
# (the shipped chain is 1.15 m); a longer span stretches them.
const MAX_LINKS := 24

var _links := MultiMeshInstance3D.new()
var _material := StandardMaterial3D.new()


func _ready() -> void:
	var link := TorusMesh.new()
	link.inner_radius = LINK_LENGTH_M / 2.0 - LINK_THICKNESS_M
	link.outer_radius = LINK_LENGTH_M / 2.0
	link.rings = 8
	link.ring_segments = 4
	var multi := MultiMesh.new()
	multi.transform_format = MultiMesh.TRANSFORM_3D
	multi.mesh = link
	multi.instance_count = MAX_LINKS
	multi.visible_instance_count = 0
	_links.multimesh = multi
	_material.metallic = 0.8
	_material.roughness = 0.35
	_material.albedo_color = SLACK_COLOUR
	_links.material_override = _material
	add_child(_links)


func show_between(hitch: Vector3, hook: Vector3, tension_n: float) -> void:
	var span := hitch - hook
	var length_m := span.length()
	if length_m < 1e-3:
		return
	var along := span / length_m
	# A link's torus lies in its own xz plane: stand it along the chain.
	var side := along.cross(Vector3.UP)
	if side.length() < 1e-3:
		side = Vector3.RIGHT
	side = side.normalized()
	var up := side.cross(along).normalized()
	var count := mini(ceili(length_m / (LINK_LENGTH_M * 0.75)), MAX_LINKS)
	var multi := _links.multimesh
	multi.visible_instance_count = count
	for i in count:
		var at := hook + span * ((float(i) + 0.5) / count)
		# Stretched along the chain to fill it; every other link turned a
		# quarter about the chain.
		var stretch := length_m / count / (LINK_LENGTH_M * 0.75)
		var normal := up if i % 2 == 0 else side
		var basis := Basis(along.cross(normal), normal, along)
		multi.set_instance_transform(i, Transform3D(basis.scaled_local(
				Vector3(1.0, 1.0, stretch)), at))
	_material.albedo_color = SLACK_COLOUR.lerp(TAUT_COLOUR, clampf(tension_n / TAUT_N, 0.0, 1.0))

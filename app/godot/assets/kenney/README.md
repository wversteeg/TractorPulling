# Kenney models (CC0)

3D models by Kenney (kenney.nl), released under CC0 1.0 (public domain, no
attribution required; credited here anyway). Taken unchanged from Kenney's
Godot starter kits, whose READMEs list the 3D models as CC0 licensed (the
kits' code is MIT; none of it is used here):

| folder | kit | commit | models |
|---|---|---|---|
| `racing/` | [Starter Kit Racing](https://github.com/KenneyNL/Starter-Kit-Racing) | 2f2e5f2646dda89cb21d4e8539bab60c6e955dc8 | `decoration-tents` |
| `platformer/` | [Starter Kit 3D Platformer](https://github.com/KenneyNL/Starter-Kit-3D-Platformer) | 3fa8a04b1c01ab23db43123d4ce814a34c3fc7f0 | `cloud` |
| `city/` | [Starter Kit City Builder](https://github.com/KenneyNL/Starter-Kit-City-Builder) | 4535092b740b378b700efd9df9e27a631815b84a | `building-garage` |

Each kit's models share one palette texture, `Textures/colormap.png`, kept
beside them as the models reference it. The tractor and the sled are not
models: `scripts/tractor_view.gd` and `scripts/sled_view.gd` build them from
the run's dimensions, so they match the simulation. The people (the
crowd, the driver, the sled's operator), the trees and the flags are built
in code too (`scripts/people.gd`, `scripts/trees.gd`,
`scripts/venue_view.gd`).

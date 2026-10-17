# CLAUDE.md

Guidance for Claude Code when working in this repository.

## Project

A tractor pulling game. Goals, in priority order:

1. **Believable (semi-sim) physics.** Realistic enough that tuning and technique matter, with easy controls and assists at the lowest classes.
2. **Performance.** The simulation must run comfortably on mid-range Android phones.
3. **Code quality.** Clean, idiomatic, well-tested modern C++. The owner is an experienced C++ engineer.
4. **Portability.** Android first; desktop, iOS and others later.

Current focus: **game mechanics only**. Graphics come later. Do not add rendering, engine or UI code until asked.

## Architecture

```
core/        Pure C++20 static library. The whole simulation lives here.
data_io/     tp::data: loads and validates data/ files into core parameter structs.
data/        Tractor, engine, tire, sled and class definitions (TOML).
tools/       Headless CLI: runs a scripted pull, writes CSV.
tests/       GoogleTest unit and integration tests.
bench/       Google Benchmark micro-benchmarks.
app/godot/   (LATER) Godot 4 GDExtension front end. Do not create yet.
```

### Rules for `core/`

- No dependency on any engine, platform API, rendering library or OS-specific header.
- No exceptions or RTTI in hot paths. Use `std::optional`, error enums or result types.
- No heap allocation inside the simulation step. Allocate and size everything at setup.
- Public headers are small and stable. They will later be consumed by a Godot binding layer, so keep them free of exotic language/library features.
- All simulation state is plain data, easy to copy, hash and serialize.
- Input goes in (throttle, clutch, steering, brakes, gear), state comes out. The core never reads devices or the clock.

### Determinism

- Fixed timestep: **1 kHz** (dt = 0.001 s). Never use wall-clock time in the simulation.
- Semi-implicit (symplectic) Euler or better. The clutch and tire slip are stiff, so prefer stability over naive explicit integration.
- `double` for state and parameters. Avoid `-ffast-math`. Be careful with fused multiply-add and compiler flags that could change results across platforms.
- Identical inputs must produce an identical state hash on every supported platform. A replay hash is a regression test.

### Units and conventions

- SI units everywhere (m, kg, s, N, N·m, rad, rad/s). Convert only at the edges (data files may document rpm, but are converted on load).
- Document the unit in every name or comment of a physical quantity (`torque_nm`, `omega_rad_s`, constants `kLoadRate_nm_per_s`).
- Sign conventions are documented once, in `docs/conventions.md`: x forward, z up, pitch positive nose-up.
- No magic numbers. Physical constants and tuning values live in named constants or in `data/`.

### Data-driven design

- Tractor, engine, tires, sled and class rules are TOML files in `data/`. Progression and upgrades only change parameters; the core does not know about "levels".
- Validate data on load with clear error messages. Reject nonsense (negative mass, non-monotonic curves).
- Physical validation rules live in `core/` (e.g. `engine::validate`) so every front end shares them; `data_io/` parses TOML (toml++), converts units and turns validation errors into messages naming the file and field.

## Milestone 1: straight-line pull

Build in this order, one PR each, each piece tested in isolation first:

1. **Engine:** torque curve, rotational inertia, turbo lag, governor and RPM limit.
2. **Clutch:** friction model with slip and stall. An easy-mode assist wraps it later; the model underneath stays the same.
3. **Driveline:** gearbox, final drive, driveline inertia.
4. **Tire:** load-dependent slip curve. Weight transfer feeds rear-axle load.
5. **Chassis:** longitudinal motion plus pitch (wheelies must emerge from the physics).
6. **Sled:** weight box moves forward as the sled advances, so resistance grows with distance.
7. **Run logic:** 100 m track; result is the distance at stall or full pull.

**Done when:**
- A scripted pull gives plausible curves for RPM, slip and sled load.
- Dumping the clutch causes wheelspin or a wheelie.
- Too much load stalls the engine.
- Identical inputs give an identical state hash on x86-64 and ARM.

Reference vehicle: lowest class, light 2WD farm-stock tractor. Exact numbers live in `data/`.

## Later (do not build yet)

- Lane steering and sled sideways drag (leaving the lane ends the run).
- Steering plus two independent brakes. The first steering range turns the wheels; beyond a threshold it applies progressive brake on that side. A "pro" mode splits this into steering and two brake inputs. Same physics, different input mapping.
- Touch-only input (sliders and tilt). Gamepad and keyboard are only for development.
- Godot 4 GDExtension front end: 3D scene, multiple cameras, interpolated rendering of core state.

## Build and test

```bash
cmake --preset debug && cmake --build --preset debug
ctest --preset debug --output-on-failure

cmake --preset asan-ubsan && cmake --build --preset asan-ubsan
ctest --preset asan-ubsan --output-on-failure

cmake --preset release && cmake --build --preset release
./build/release/bench/core_bench

# Engine alone on a virtual dynamometer, CSV to plot
./build/release/tools/tp_pull engine-dyno data/engines/farm_stock_light.toml -o dyno.csv

# The whole tractor pulling a constant drawbar force: clutch slipped in, or dumped
./build/release/tools/tp_pull pull data/tractors/farm_stock_light.toml \
    [--gear 4] [--resistance-kn 8] [--dump] -o pull.csv

# May this tractor compete in this class? Exits non-zero if not
./build/release/tools/tp_pull check data/tractors/farm_stock_light.toml \
    data/classes/ntto_standard_3500.toml
```

Presets: `debug`, `release`, `asan-ubsan`. Keep the commands above working; update this section when presets change.

## Code standards

- **C++20.** Raise the standard only when asked. Avoid library features missing from the Android NDK's libc++.
- Warnings are errors, locally and in CI (`TP_WARNINGS_AS_ERRORS`, on by default): `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` and similar for clang and gcc.
- `core/` builds with `-fno-exceptions -fno-rtti -ffp-contract=off`. Tests, tools and benchmarks may use exceptions.
- Namespace `tp`. Code is grouped by topic: headers in `core/include/tp/core/<topic>/`, sources in `core/src/<topic>/`, tests in `tests/<topic>/`, benchmarks in `bench/<topic>/`.
- `clang-format` and `clang-tidy` must pass. Run them before committing.
- Naming (enforced by clang-tidy): `CamelCase` types, `snake_case` functions and variables, `member_` for private members, `kCamelCase` constants and enumerators, with an optional `_unit` suffix on physical constants.
- Prefer value semantics, `constexpr`, `std::span`, and strong types for physical quantities where it adds safety without cost.
- Small, focused functions and classes. No premature abstraction, no inheritance hierarchies in hot paths.
- Every behavior has a test. Physics tests assert physical properties (energy, monotonicity, limits), not just snapshots.
- Performance-sensitive changes include or update a benchmark.
- Comments explain *why* and the physical model, not *what* the code does.

## Documentation and diagrams

- `docs/architecture.md` shows the class and sequence diagrams. Sources are PlantUML in `docs/diagrams/*.puml`; rendered SVGs live next to them in `docs/diagrams/svg/` and are committed for easy review.
- **Update the diagrams in the same PR as every change** that adds, removes or changes a type, a public function, a dependency between libraries, or the order of calls in a documented flow. Each component gets its own class diagram (`<topic>_classes.puml`; shared types and the engine are in `core_classes.puml`), and each new flow its own sequence diagram (e.g. clutch coupling, a full pull).
- Render with `scripts/render_diagrams.sh` (needs Java and Graphviz; downloads a pinned PlantUML). `scripts/render_diagrams.sh --check` verifies every SVG matches its source; CI runs it.
- On pull requests, CI posts the changed diagrams as one PR comment (`scripts/post_diagram_preview.sh`) and updates it on each push, so they can be reviewed as images.

## Workflow

- Plan first for non-trivial work, and wait for approval before large changes.
- Small, logical commits with clear messages. One feature per branch and PR.
- CI (GitHub Actions) must be green: clang and gcc on Linux, sanitizers, clang-tidy, format check. Keep CI config minimal and portable.
- Ask questions when requirements are ambiguous. State what you chose and why.
- Do not add dependencies without explaining the trade-off. Approved so far: GoogleTest, Google Benchmark, toml++.

## Dependencies

Fetched with CMake `FetchContent` at pinned release tags (`cmake/Dependencies.cmake`). `FIND_PACKAGE_ARGS` lets an installed package be used instead. Chosen over vcpkg because there are only three dependencies, there is no bootstrap step, and the same setup works unchanged with the Android NDK toolchain. Revisit vcpkg if the dependency count grows.

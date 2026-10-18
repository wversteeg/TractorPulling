# TractorPulling

A tractor pulling game with believable, deterministic semi-sim physics. Android first, other platforms later.

The simulation is a pure C++20 library (`core/`) with no engine or platform dependencies. A Godot 4 front end follows later.

## Status

**Milestone 1, a straight-line pull, is complete.** A light farm-stock tractor
(NTTO standard 3.5 t) pulls a transfer sled on a 100 m track. Each "done when"
criterion is covered by a test:

| Criterion | Test |
|---|---|
| A scripted pull gives plausible curves for RPM, slip and sled load | `Run.PullingGearEndsWithAStall`, `Pull.PullingGearStallsAfterThePushdown` (and `tp_pull run` CSV) |
| Dumping the clutch causes wheelspin or a wheelie | `Vehicle.DumpingTheClutchSpinsTheWheels`, `Vehicle.DumpingTheClutchInALowGearLiftsTheFront` |
| Too much load stalls the engine | `Vehicle.LightClassPullsOnTorqueInItsPullingGears`, `Run.PullingGearEndsWithAStall` |
| Identical inputs give an identical state hash on x86-64 and ARM | every `*RegressionHash` test and `Run.ReplayMatchesRegression`, run in CI on x86-64 and arm64 |

Try a run:

```bash
cmake --preset release && cmake --build --preset release
./build/release/tools/tp_pull run data/tractors/farm_stock_light.toml \
    data/sleds/light_class.toml data/tracks/ideal_clay_sand.toml --gear 6 -o run.csv
# run: 71.76 m, engine stalled (29.9 s, top 12.1 km/h)
```

## Requirements

- CMake 3.25+ and Ninja
- GCC 12+ or Clang 16+
- clang-format and clang-tidy (for development)

GoogleTest, Google Benchmark and toml++ are fetched automatically on first configure.

## Build and test

```bash
cmake --preset debug && cmake --build --preset debug
ctest --preset debug --output-on-failure
```

Other presets: `release` (also builds `build/release/bench/core_bench`) and `asan-ubsan`.

See [CLAUDE.md](CLAUDE.md) for architecture, conventions and the roadmap, [docs/architecture.md](docs/architecture.md) for class and sequence diagrams, [docs/conventions.md](docs/conventions.md) for units and sign conventions, and [docs/future_improvements.md](docs/future_improvements.md) for what is deliberately left for later.

## License

MIT, see [LICENSE](LICENSE).

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
# run: 92.60 m, engine stalled (37.4 s, top 11.9 km/h)
```

### Track conditions

Every event's track is different. A track file gives its soil (clay
fraction, moisture, compaction), optionally how that changes along the
track, and the push-down the sled operator sets for the day. The soil sets
how strong the ground is and how well it grips. The tires (Brixius), the
sled's pan and its wheels all work on the ground where they are. Compare
the shipped tracks:

```bash
./build/release/tools/tp_pull compare-tracks data/tractors/farm_stock_light.toml \
    data/sleds/light_class.toml data/tracks/*.toml --gear 6
# track                         cone index   grip  fall-off  push-down  result
# dry_hard_clay                    6000 kPa   0.96      0.19     55.0 m  90.05 m, engine stalled
# ideal_clay_sand                  1485 kPa   1.00      0.22     60.0 m  92.60 m, engine stalled
# loose_sand                        907 kPa   1.00      0.29     60.0 m  89.92 m, engine stalled
# wet_heavy_clay                    539 kPa   1.00      0.28     70.0 m  97.16 m, engine stalled
```

With the class's own push-down the wet clay is the shortest track and the
hard clay the longest; the sled operator evens that out with the push-down
for the day.

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

# TractorPulling

A tractor pulling game with believable, deterministic semi-sim physics. Android first, other platforms later.

The simulation is a pure C++20 library (`core/`) with no engine or platform dependencies. A Godot 4 front end follows later.

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

See [CLAUDE.md](CLAUDE.md) for architecture, conventions and the roadmap, and [docs/conventions.md](docs/conventions.md) for units and sign conventions.

## License

MIT, see [LICENSE](LICENSE).

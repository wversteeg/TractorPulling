#!/usr/bin/env bash
# Prints what a build of tp_pull makes of the scripted runs and of the club
# event, for comparing platforms: identical inputs must give identical
# results on every platform the game ships on (CLAUDE.md). CI runs it on
# Linux and with the Windows build on a Windows runner, and compares the
# two outputs (line endings aside).
#
#   scripts/platform_runs.sh <tp_pull binary>
#
# Run from the repository root: the data paths are relative to it.
set -euo pipefail

if [[ $# -ne 1 ]]; then
    echo "usage: $0 <tp_pull binary>" >&2
    exit 2
fi
readonly tp_pull="$1"
readonly tractor=data/tractors/farm_stock_light.toml
readonly sled=data/sleds/light_class.toml
readonly track=data/tracks/ideal_clay_sand.toml
# The runs' CSVs, which are not compared.
scratch="$(mktemp -d)"
readonly scratch
trap 'rm -rf "${scratch}"' EXIT

# Each run's result and its state hash: the exact bits of its end state.
"${tp_pull}" run "${tractor}" "${sled}" "${track}" --gear 6 \
    --easy data/controls/easy.toml --stick 0.032 --hash -o "${scratch}/run_easy.csv"
"${tp_pull}" run "${tractor}" "${sled}" "${track}" --gear 6 \
    --pro data/controls/pro.toml --stick 0.032 --dump --brake-right 0.05 --hash \
    -o "${scratch}/run_pro.csv"
# A whole event: the field drawn from the data (text parsed into doubles),
# every pull and the standings, and its hash over the exact bits of every
# pull (the printout rounds to 0.01 m).
"${tp_pull}" event data/events/club_pull.toml --hash

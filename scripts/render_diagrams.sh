#!/usr/bin/env bash
# Renders docs/diagrams/*.puml to docs/diagrams/svg/*.svg with a pinned
# PlantUML, and stamps each SVG with the SHA-256 of its source.
#
#   scripts/render_diagrams.sh          render all diagrams
#   scripts/render_diagrams.sh --check  fail if any SVG is missing or stale
#
# The check compares source hashes rather than SVG bytes, because rendered
# text metrics depend on the fonts installed on the machine.
#
# Needs Java 11+ and Graphviz (dot) for class diagram layout.
set -euo pipefail

readonly PLANTUML_VERSION="1.2024.7"
readonly PLANTUML_SHA256="cb42e3272fedecc0ed20ee0c9cef31873d1c42a489043971038631d357f467e6"
readonly PLANTUML_URL="https://repo1.maven.org/maven2/net/sourceforge/plantuml/plantuml/${PLANTUML_VERSION}/plantuml-${PLANTUML_VERSION}.jar"

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
src_dir="${root}/docs/diagrams"
out_dir="${src_dir}/svg"
jar="${root}/.cache/plantuml-${PLANTUML_VERSION}.jar"

source_hash() { sha256sum "$1" | cut -d' ' -f1; }
stamp() { printf '<!-- plantuml-source-sha256: %s -->' "$1"; }

check() {
    local stale=0
    for puml in "${src_dir}"/*.puml; do
        local svg="${out_dir}/$(basename "${puml%.puml}").svg"
        if [[ ! -f "${svg}" ]] || ! grep -qF "$(stamp "$(source_hash "${puml}")")" "${svg}"; then
            echo "stale or missing: ${svg#"${root}"/} (run scripts/render_diagrams.sh)" >&2
            stale=1
        fi
    done
    for svg in "${out_dir}"/*.svg; do
        [[ -e "${svg}" ]] || continue
        if [[ ! -f "${src_dir}/$(basename "${svg%.svg}").puml" ]]; then
            echo "orphaned: ${svg#"${root}"/} has no source" >&2
            stale=1
        fi
    done
    return "${stale}"
}

fetch_plantuml() {
    if [[ -f "${jar}" ]] && echo "${PLANTUML_SHA256}  ${jar}" | sha256sum --check --status; then
        return
    fi
    mkdir -p "$(dirname "${jar}")"
    curl --fail --silent --show-error --location --output "${jar}.tmp" "${PLANTUML_URL}"
    echo "${PLANTUML_SHA256}  ${jar}.tmp" | sha256sum --check --status ||
        { echo "PlantUML checksum mismatch" >&2; rm -f "${jar}.tmp"; exit 1; }
    mv "${jar}.tmp" "${jar}"
}

render() {
    fetch_plantuml
    mkdir -p "${out_dir}"
    rm -f "${out_dir}"/*.svg
    java -Djava.awt.headless=true -jar "${jar}" -tsvg -failfast2 -o "${out_dir}" "${src_dir}"/*.puml
    for puml in "${src_dir}"/*.puml; do
        local svg="${out_dir}/$(basename "${puml%.puml}").svg"
        stamp "$(source_hash "${puml}")" >> "${svg}"
        echo "rendered ${svg#"${root}"/}"
    done
}

case "${1:-}" in
    --check) check ;;
    "") render ;;
    *) echo "usage: $0 [--check]" >&2; exit 2 ;;
esac

#!/usr/bin/env bash
# Posts the diagrams changed by a pull request as one PR comment, and updates
# that same comment on later pushes, so reviewers see rendered images in the
# conversation instead of SVG source in the diff.
#
# Run by CI. Needs: GH_TOKEN, GITHUB_REPOSITORY, PR_NUMBER, BASE_SHA, HEAD_SHA.
# DRY_RUN=1 prints the comment instead of posting it.
set -euo pipefail

readonly MARKER="<!-- diagram-preview -->"
readonly SVG_DIR="docs/diagrams/svg"

mapfile -t changed < <(git diff --name-only --diff-filter=d "${BASE_SHA}...${HEAD_SHA}" -- "${SVG_DIR}/*.svg")
if [[ ${#changed[@]} -eq 0 ]]; then
    echo "No diagrams changed."
    exit 0
fi

blob_url() { echo "https://github.com/${GITHUB_REPOSITORY}/blob/${HEAD_SHA}/$1"; }

body="${MARKER}
## Diagram preview

Diagrams changed in this PR, rendered at ${HEAD_SHA:0:7}. All diagrams with explanations: [docs/architecture.md]($(blob_url docs/architecture.md)).
"
for svg in "${changed[@]}"; do
    name="$(basename "${svg}" .svg)"
    body+="
### ${name}

[![${name}]($(blob_url "${svg}")?raw=true)]($(blob_url "${svg}"))
"
done

if [[ "${DRY_RUN:-0}" == "1" ]]; then
    echo "${body}"
    exit 0
fi

comments="repos/${GITHUB_REPOSITORY}/issues/${PR_NUMBER}/comments"
existing="$(gh api "${comments}" --paginate \
    --jq ".[] | select(.body | startswith(\"${MARKER}\")) | .id" | head -n 1)"
if [[ -n "${existing}" ]]; then
    gh api --method PATCH "repos/${GITHUB_REPOSITORY}/issues/comments/${existing}" \
        --field body="${body}" > /dev/null
    echo "Updated diagram preview comment ${existing}."
else
    gh api --method POST "${comments}" --field body="${body}" > /dev/null
    echo "Posted diagram preview comment."
fi

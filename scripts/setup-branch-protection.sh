#!/usr/bin/env bash
# One-off setup: protect `main` once the repo is pushed and you're
# authenticated (`gh auth login`) with admin rights on the repo. Requires
# the CI workflow to have run at least once so the "build" check exists.
set -euo pipefail

repo="Kauryx-Tech/bcad"

gh api \
    --method PUT \
    -H "Accept: application/vnd.github+json" \
    "repos/${repo}/branches/main/protection" \
    --input - <<'JSON'
{
  "required_status_checks": { "strict": true, "contexts": ["build"] },
  "enforce_admins": true,
  "required_pull_request_reviews": null,
  "restrictions": null
}
JSON

echo "Branch protection enabled on ${repo}@main (CI must pass before merge)."

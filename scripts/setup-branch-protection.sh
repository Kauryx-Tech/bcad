#!/usr/bin/env bash
# Configuration ponctuelle : protège `main` une fois le dépôt poussé et
# une fois authentifié (`gh auth login`) avec des droits admin sur le
# dépôt. Nécessite que le workflow CI ait tourné au moins une fois pour
# que le check "build" existe.
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

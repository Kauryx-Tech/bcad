# Security Policy

bcad is a desktop CAD application that reads/writes local files (DXF,
`.bcad` SQLite projects). The main risk surface is malicious/malformed input
files triggering memory-safety bugs in the parsers (`io::DxfReader`,
`io::Database`) or the CGAL-backed geometry code.

## Reporting a vulnerability

Please report security issues privately rather than opening a public issue:
[GitHub Security Advisories](https://github.com/Kauryx-Tech/bcad/security/advisories/new)
for this repository, or email contact@kauryxgroup.com.

Include, if possible: the file or input that triggers the issue, the
affected module, and the commit/tag you tested against.

This is a personal/small-team project without a fixed SLA, but reports will
be acknowledged and addressed as soon as reasonably possible.

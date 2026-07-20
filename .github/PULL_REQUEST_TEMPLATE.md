## Summary

<!-- What does this change do, and why? -->

## Related issue

<!-- Closes #... -->

## Testing

- [ ] `ctest --test-dir build --output-on-failure` passes locally
- [ ] Added/updated a check in `tests/smoke_test.cpp` if this touches
      `geometry`/`layers`/`core`/`io`
- [ ] Manually exercised the change in the running app, if it touches `app`

## Checklist

- [ ] Module dependency direction (`geometry` → `layers` → `render` →
      `core` → `io` → `app`) is unchanged
- [ ] `CAHIER_DES_CHARGES.md` updated if this closes or changes a roadmap
      item

## Summary

Describe the change and the user-facing impact.

## Testing

- [ ] `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON`
- [ ] `cmake --build build --config Release --parallel`
- [ ] `ctest --test-dir build --build-config Release --output-on-failure`

## Checklist

- [ ] I kept the change focused and avoided unrelated edits.
- [ ] I updated tests when behavior changed.
- [ ] I updated documentation when the public API or behavior changed.
- [ ] I noted any breaking changes or migration steps.

# Benchmarks

`clix` ships with a small benchmark harness that does not require third-party benchmarking libraries.

## Build

```bash
cmake --preset default -DCLIX_BUILD_BENCHMARKS=ON
cmake --build build -j
```

## Run

```bash
./build/benchmarks/clix_benchmarks
```

The benchmark program currently measures:

- schema construction with the direct builder API
- schema construction through routers
- simple command parsing
- nested parsing with options
- router-mounted parsing
- completion generation
- help generation

## Reading Results

The output uses:

- `ns/op`: average nanoseconds per iteration
- `ops/s`: approximate operations per second

## Notes

- These are microbenchmarks, not end-to-end product metrics.
- They are best used to compare revisions of `clix`, not different machines.
- Always run them in a release build.
- Prefer comparing several runs instead of trusting a single number.

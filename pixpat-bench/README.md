# pixbench

Build, measure and compare the pixpat native library — any compiler,
profile and build flags — on three axes:

- **correctness**: a SHA-256 of every output byte of a fixed matrix of
  conversions, pattern draws and error paths (the *fingerprint*). Two
  builds, or two commits, that agree on every byte have the same
  digest; the manifest says per group and per case where they stop
  agreeing.
- **throughput**: Mpx/s per case, timed through raw `ctypes` with every
  library loaded into one process and interleaved, so drift lands on
  all of them alike.
- **size**: the file, the file stripped, and the ELF sections that get
  mapped (`.text` is the number that tracks code).

Every measurement lands in a JSON *run file* under `measurements/`
(gitignored), which `compare`, `report` and `export` read back. Builds
are cached under `.pixbench-cache/` by the content of the subtrees they
came from, so measuring a commit twice, or a commit that did not touch
the library, costs nothing.

Python ≥ 3.10, standard library only; `meson`/`ninja` for the build,
`strip` (binutils) for the stripped size.

## Commands

```
scripts/pixbench build SUBJECT [--tree DIR | --commit REV] [--out DIR]
scripts/pixbench size LIB...
scripts/pixbench fingerprint LIB [--ref LIB] [--tier full|breadth] [--seed N] [filters]
scripts/pixbench fingerprint --diff A.fp.json B.fp.json
scripts/pixbench perf [NAME=]LIB... [filters] [timing] [--save]
scripts/pixbench measure --subject SPEC... [--lib [NAME=]LIB...] [--tree DIR | --commit REV] ...
scripts/pixbench history REVS | --commits A,B | --last N  --subject SPEC... ...
scripts/pixbench compare A B [--threshold 0.05] [--remeasure] [--verbose] [filters]
scripts/pixbench report [--metric mpx_s|text_bytes|...] [--by subject|commit|group] [filters]
scripts/pixbench export [--out FILE] [--csv]
```

`build`, `size`, `fingerprint` and `perf` each do one thing on a tree
or a built library and know nothing about git or run files. `measure`
chains them into one run file; `history` runs `measure` over commits.
`--results DIR` / `--cache DIR` (or `PIXBENCH_RESULTS` /
`PIXBENCH_CACHE`) relocate the two directories.

### Subjects

A subject is one build variant, `[NAME=]BASE[:PROFILE][,key=value...]`:

| part | values |
|---|---|
| `BASE` | `gcc`, `gcc-14`, `clang`, `clang-19`, `cpp` |
| `PROFILE` | a name under `pixpat-native/profiles/` (`no_hotpath`) or a `.toml` path |
| keys | `cc`, `cxx`, `profile`, `arch` (`-march`), `lto`, `opt` (meson `optimization`), `buildtype` |

`gcc`, `clang:no_hotpath`, `gcc-14,arch=x86-64-v3`, `icx=cpp,cc=icx,cxx=icpx`.
A relative profile path is resolved inside the tree being built, so a
commit is measured with its own copy of the profile. The compiler's
`--version` output is recorded with every build.

### Filters — pinpointing

Every case is `<source> -> <destination>`: a format pair, or a pattern
name and its sink. The same flags narrow every command:

```
--only NV12,BGR888     either side is one of these
--src NV12             source format, or pattern name
--dst BGR888           destination format
--cases "NV12 -> BGR888,smpte -> NV12"
--patterns smpte       keep only these pattern draws (converts unaffected)
--kind convert|pattern
```

For `perf` the default case set is the curated list in `cases.py`
(56 conversion pairs, 13 sinks × 10 patterns, ~1–2 min per library);
`--all` times every pair and every pattern × sink. For `fingerprint`
the filter restricts the matrix to the pairs and sinks named, and the
per-format checks to those formats; the run file stores the filter, so
a partial run is a first-class record that never overwrites a full one
(`compare` refuses to compare fingerprints of different filter, tier or
seed).

### Timing

`--size WxH` (1920x1080), `--threads N[,N]` (1), `--rec`/`--range`
(BT709/LIMITED), `--iters` (10) timed calls per round, `--rounds` (3)
passes over the libraries, `--warmup` (2). The record keeps the minimum
and the median over all samples. `--cpu 6` (or `PIXBENCH_CPU=6`) pins
the process; on a hybrid part pick a P-core, and pin to a set (`6-9`)
when timing `--threads 4`. The pinning and the CPU model go into the
run file; an unpinned run is recorded as such.

### Groups

One geomean over 186 cases says whether a build drifted and hides
everything else: a 0.2x collapse on two cases and 1.7x on five others
came out as 0.983x. So `compare` also prints a per-group table (cases,
geomean, min, max) next to the overall number, and `report --by
group` is the same cut as a matrix — one column per build, one row per
group, the geomean of Mpx/s over the cases every column has, so the
ratio of two cells is what `compare` would print for those builds.

A case is summarized under its kind and one group per side. A
conversion is in the family it reads (`src=semiplanar`) and the one
it writes (`dst=bayer-csi2`); a draw is in its pattern
(`pattern=smpte`) and the family it writes (`sink=bayer-csi2`) — a
separate group from `dst=`, because the write loop is a different
share of a draw and of a conversion and one group would average a
0.2x sink regression on the conversions with 0.9x on the draws into
nothing. The families are the library's
own Source/Sink template pairs, transcribed in `formats.py`
(`FAMILIES`): `rgb`, `yuv444`, `yuv422`, `semiplanar`,
`semiplanar-mp`, `planar`, `planar-mp`, `gray`, `gray-mp`,
`gray-csi2`, `mono-rgb`, `bayer`, `bayer-csi2`. Groups overlap on
purpose — one per axis — so a change to one family's read or write
loop shows on that axis whatever is on the other side. The curated
list puts BGR888 on one side of most rows, so `src=rgb` and `dst=rgb`
are large and everything else is small; the `cases` column says which
is which, and groups with a single case are left to the per-case
table. `export` carries `src_family` / `dst_family` on every
throughput record.

## Workflows

Iterating on NV12 in the working tree, gcc against clang:

```sh
scripts/pixbench measure --subject gcc --subject clang --only NV12 --cpu 6
scripts/pixbench compare gcc clang             # latest run of each name
```

Before/after for a commit, all cases, correctness included:

```sh
scripts/pixbench measure --commit master~1 --subject clang --cpu 6
scripts/pixbench measure --commit master   --subject clang --cpu 6
scripts/pixbench compare @master~1:clang @master:clang
scripts/pixbench compare @master~1:clang @master:clang --remeasure   # A/B again, interleaved
```

Across history — one run file per commit; a commit whose library came
out byte-identical to the previous one is recorded but not re-timed,
and `report`/`compare` fill it in from the identical library:

```sh
scripts/pixbench history master~10..master --subject gcc --subject clang --only NV12
scripts/pixbench report --by commit --subjects clang --only NV12
scripts/pixbench report --by commit --metric text_bytes
```

A table with one column per build (rows = cases, latest record of
each), e.g. for a README:

```sh
scripts/pixbench measure --subject gcc --subject clang --subject cold=clang:no_hotpath --cpu 6
scripts/pixbench report --by subject --cases "kmstest -> XRGB8888,smpte -> NV12,YUYV -> BGR888" --markdown
```

The profile matrix (what `build_profiles.sh` used to print):

```sh
scripts/pixbench measure --no-fingerprint \
    $(for p in pixpat no_hotpath pattern_only to_bgr888; do
        echo --subject gcc:$p --subject clang:$p; done) \
    --cases "RGB888 -> BGR888,NV12 -> BGR888,BGR888 -> NV12,smpte -> BGR888"
scripts/pixbench report --by subject --metric text_bytes
```

Bit-exactness of one build against another, live, and of two saved
manifests:

```sh
scripts/pixbench fingerprint build-clang/libpixpat.so --ref build-gcc/libpixpat.so
scripts/pixbench fingerprint --diff measurements/manifests/A.fp.json measurements/manifests/B.fp.json
```

Selectors for `compare`: `RUN.json#SUBJECT`, `@REV[:SUBJECT]` (the
latest run measuring that commit), or a bare `SUBJECT` (its latest
run).

## The fingerprint

`fingerprint.py` holds the matrix; `formats.py` the format table it is
built on — a transcription of the C++ declarations made by hand rather
than generated from them, with plane geometry derived by the library's
own rules, so a build that disagrees with it fails a *coverage* check
(every byte of a 0xFF-prefilled destination must be written) or
returns a non-zero rc rather than silently agreeing on a wrong shape.

| group | what |
|---|---|
| `catalog` | format count, names and order (the enumeration is ABI), unknown name, index past the end, `pixpat_format_supported` per format |
| `coverage` | destination shape, once per sink (self-check) |
| `convert-breadth` | every (src, dst) pair at 96×32, one thread, one spec per kind pair |
| `convert-depth` | one format per I/O shape family × 3 sizes × every rec/range × 1/4/16 threads |
| `patterns` | every pattern into every sink; params-parser cases on the depth subset |
| `pattern-errors`, `convert-errors` | the rc of every rejected call |
| `pads` | fat strides (tight + 16), single and multi thread |
| `spec-fallback` | out-of-range rec/range produce the BT601/limited bytes (self-check) |

`--tier breadth` drops `convert-depth`, `pads` and the params cases
(~11k checks instead of ~39k; the full matrix is ~3 s per library
anyway). The digest is a SHA-256 over the sorted group digests, each a
SHA-256 over that group's sorted per-check hashes; a check hashes the
rc and the destination planes. Source data is seeded from the check's
label, so two libraries see identical input. Manifests are written to
`measurements/manifests/<lib sha256>-<variant>.fp.json`, one per
(library, tier, filter, seed).

`zoneplate` calls libm `cos`, so its bytes can differ in the last bit
between machines with different libm builds; a cross-machine digest
mismatch confined to `patterns/zoneplate:*` is that.

## Run file schema (`schema: 1`)

```
run:      {id, time, label, argv, tool: {commit, dirty},
           machine: {hostname, cpu_model, cpus, arch, kernel, python, libc,
                     cpu_pinned: [6] | null, governor}}
subjects: {NAME: {lang: cpp,
                  source: {commit | null, dirty, describe, subtrees: {path: tree sha}},
                  build:  {cc, cxx, profile, profile_sha256, arch, lto, opt, buildtype,
                           toolchain_versions: {...}, config_hash, error | null},
                  lib:    {path, sha256, bytes, stripped_bytes, alloc_bytes,
                           sections: {".text": n, ...}},
                  unchanged_from: <commit>            # history only: not re-measured
                 }}
fingerprint: {NAME: {tier, filter, seed, digest, groups: {group: digest}, cases,
                     self_check_failures: [...], warnings: [...], manifest: path}}
perf:     [{subject, case, kind, w, h, threads, rec, range, iters, warmup, rounds,
            samples, min_ns, median_ns, mpx_s}]
notes:    [...]                                        # skipped cases and why
```

`export` flattens this to one record per measurement (`kind`: `size`,
`fingerprint` or `perf`) with the run, machine and subject context
inlined — the input for anything that wants to plot across compilers,
versions, flags and commits; `--csv` writes the perf records as a
table. `source.subtrees` are the git tree hashes of the paths the
library is built from (`pixpat-native` plus the meson files); together
with `config_hash` (config + profile content + toolchain versions) they
key the build cache, so anything that leaves them unchanged is the
same library.

## Layout

```
scripts/pixbench      launcher
pixpat-bench/pixbench/
  cli.py              subcommands
  abi.py              ctypes structs, Lib, loading several libraries at once
  formats.py          the format table and buffer geometry
  cases.py            case naming, the curated list, the filter grammar
  fingerprint.py      the matrix, hashing, manifests, the live diff
  size.py             ELF section sizes, stripped size
  perf.py             the timing loop
  build.py            subject specs, meson builds, the cache key
  measure.py          size + fingerprint + perf into a run
  history.py          commit lists, exports, per-commit sources
  store.py            run files, selectors
  report.py           compare / report / export
  machine.py          where it ran
```

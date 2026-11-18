# SotSort vs Virtual-Memory Powersort (their harness)

Comparison on the experimental harness of Moltmann, Nakajima and Wild,
*Virtual-Memory Powersort* (ESA 2026), https://git.sr.ht/~tamionv/vm-powersort,
branch `assignment-fix` (commit `eaa430d`), which also counts copy/move constructions.
Their code is not copied here; `sotsort-harness.patch` adds four SotSort entries to their
`src/main.cpp` (and drops `-v` from the makefile). Everything else is theirs: inputs, types,
timing with `clock()`, correctness check against `std::sort`, `-std=c++20 -O2`.

## Reproduce

```
git clone https://git.sr.ht/~tamionv/vm-powersort && cd vm-powersort
git checkout assignment-fix
cp <sotsort>/include/sotsort.hpp src/
git apply <sotsort>/bench/vmpowersort/sotsort-harness.patch
make main && ./run_sot.sh && python3 parse.py
```

## SotSort entries (all single-threaded, no memory probe, no counting shortcut)

| name      | auxiliary memory given to SotSort                                      |
|-----------|-------------------------------------------------------------------------|
| `sot0`    | none beyond its fixed 16 KB internal buffer (O(1) mode)                 |
| `sotvm`   | exactly the bytes `almost_inplace_powersort` allocates (pages + pointers) |
| `sothalf` | n/2 elements, like CPython-style Powersort (`copy`)                     |
| `sotfull` | n elements                                                              |

`sotvm` still has the 16 KB internal buffer on top, so it uses slightly more memory than
`almost` (Virtual-Memory Powersort).

## Setup of this run

One 2-vCPU cloud VM (Xeon, see `results/*/lscpu`), gcc 13.3, pinned to one core,
maxn = 10^6 (n uniform in [0.9·10^6, 10^6]), 10 iterations per cell, their six
presortedness levels S (average run length) and four types. The paper uses n ≈ 10^7,
100 iterations and an i7-4770, so absolute numbers are not comparable with the paper.
The relative order of their own algorithms agrees with their published `average-time.txt`
in the cells checked (copy ≈ simple < almost < wiki < stable), but this is not a
reproduction of their results.

The wrapper type in their harness has user-defined copy operations, so it is not trivially
copyable: SotSort runs its generic path (no SIMD, no routing probes, no disordered zones).
This is the representative case for non-trivial element types, not for plain integers.

## Results (`results/summary.txt`)

Time, `sotvm / almost` (same memory budget):

| type    | S = 2 | 100  | 1000 | 10^4 | 10^5 | 10^6 |
|---------|-------|------|------|------|------|------|
| int     | 1.07  | 1.03 | 1.05 | 0.98 | 1.03 | 0.88 |
| ptr     | 1.02  | 1.14 | 1.24 | 1.09 | 1.05 | 1.28 |
| arr     | 1.10  | 0.77 | 1.06 | 1.12 | 1.27 | 1.13 |
| fastarr | 1.52  | 1.50 | 1.67 | 1.62 | 1.59 | 0.89 |

Comparisons per element, int:

| S        | 2     | 100   | 1000  | 10^4  | 10^5  | 10^6  |
|----------|-------|-------|-------|-------|-------|-------|
| almost   | 28.93 | 15.55 | 10.39 | 7.39  | 4.30  | 1.69  |
| sotvm    | 19.69 | 15.17 | 11.51 | 8.50  | 5.35  | 2.30  |

Reading:

- With the same memory, SotSort is not faster than Virtual-Memory Powersort on these
  inputs. On `int` the ratio is 0.88–1.07; on `ptr` (slow comparisons) SotSort is 2–28%
  slower; on `arr` 6–27% slower except S = 100 (0.77, possibly noise at 10 iterations);
  on `fastarr` (slow moves) 50–67% slower except at S = 10^6.
- On near-random input (S = 2) SotSort's quicksort makes 32% fewer comparisons than
  Powersort, but this does not turn into lower time here.
- For S ≥ 1000 SotSort makes about 0.6–1.15 more comparisons per element than
  Powersort. The likely cause is the separate turn-counting scan before the run
  detection (one extra comparison per element); this is a concrete thing to fix.
- With more memory (`sothalf`) SotSort gets close to the n/2-buffer Powersorts on `arr`
  and `fastarr`, but does not beat `copy`/`simple` consistently.

## Update: v8-r3, turn density estimated by sampling (`results-ab/`)

The extra ~1 comparison per element on presorted input came from the exact turn count in
`few_turns`, which must scan the whole array when turns are few, before the natural merge sort
scans it again for runs. From n ≥ 2^16 it is now estimated from 64 windows of 256 adjacent
pairs (16384 comparisons); `-DSOT_EXACT_TURNS` restores the exact count. The estimate only
chooses between two correct paths (see `docs/LEMAS_v8r.md`, Observación T).

A/B on the same harness and machine (`ab.sh`; old = `-DSOT_EXACT_TURNS`, new = default),
maxn = 10^6, 10 iterations, ratios to `almost` measured in the same run:

| int comparisons/element | S = 2 | 100   | 1000  | 10^4 | 10^5 | 10^6 |
|-------------------------|-------|-------|-------|------|------|------|
| almost                  | 28.93 | 15.55 | 10.39 | 7.39 | 4.30 | 1.69 |
| sotvm before            | 19.69 | 15.17 | 11.51 | 8.50 | 5.34 | 2.30 |
| sotvm after             | 19.48 | 14.20 | 10.55 | 7.51 | 4.36 | 1.71 |

Time, `sotvm / almost`, after the change: `int` 0.87–1.08; `ptr` 0.90–1.07 (before
0.98–1.40); `arr` 0.71–1.19; `fastarr` still 1.42–1.59 except S = 10^6 (0.83). With the same
memory, SotSort now ties Virtual-Memory Powersort on slow comparisons, and still loses when
moves are expensive. Run-to-run noise between the two runs of identical code paths is up to
about ±8% at 10 iterations, so differences below that are not meaningful.

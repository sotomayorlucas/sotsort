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
maxn = 10^6 (n uniform in [0.9·10^6, 10^6]; per-element counts divided by 950 000), 10 iterations per cell, their six
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
moves are expensive. In cells where the change saves almost nothing (int, S = 2: 1% of the
comparisons) the ratio still moved by 7% between the two runs, so at 10 iterations differences
below about ±8% are not meaningful.

## Update 2: where the extra moves come from, n = 10^7, second machine

**Moves by phase** (`assignments_by_phase.cpp`, int wrapper, n = 950 000, v8-r3). All the
excess over Virtual-Memory Powersort is in rotations inside merges whose runs both exceed the
buffer; the buffer merges themselves are identical for every budget:

| S     | rotations, VM budget | rotations, n/2 budget | buffer merges (both) |
|-------|----------------------|-----------------------|----------------------|
| 1000  | 3.75                 | 0.75                  | 13.84                |
| 10^4  | 3.76                 | 0.74                  | 8.67                 |
| 10^5  | 4.30                 | 0.71                  | 4.39                 |

SymMerge halves the larger run and rotates about half the range per level, for log2(m/B)
levels when the runs are larger than the buffer B. This is structural: with a buffer of
O(sqrt(n log n)) elements, rotation-based merging pays Θ(log(m/B)) extra moves per element of
each large merge, while Virtual-Memory Powersort moves pages by pointer and avoids it.

**Rejected: cycle-leader rotation for large elements** (`results-juggle-rejected/`, fastarr,
120 bytes). It cuts moves (S = 10^5: 14.40 → 11.93 per element) but is slower in 7 of 12
cells, e.g. S = 10^6 `sotvm/almost` 0.79 → 1.11, because of the strided access. Not merged.

**n = 10^7, int** (`results-1e7/`, same cloud VM, 10 iterations for time, 3 for counts):

| S                         | 2    | 100  | 1000 | 10^4 | 10^5 | 10^6 |
|---------------------------|------|------|------|------|------|------|
| time sotvm / almost       | 1.09 | 1.10 | 1.13 | 1.17 | 1.19 | 1.24 |
| time sothalf / copy       | 1.06 | 1.10 | 1.09 | 1.05 | 1.10 | 1.05 |
| comparisons/elem almost   | 26.80| 17.26| 13.58| 10.52| 7.24 | 3.98 |
| comparisons/elem sotvm    | 22.30| 17.35| 13.81| 10.68| 7.27 | 4.06 |
| assignments/elem almost   | 30.71| 19.37| 15.55| 12.50| 9.22 | 5.96 |
| assignments/elem sotvm    | 44.99| 31.96| 27.04| 22.51| 16.89| 10.77|

At n = 10^7 the buffer is a smaller fraction of n, so rotations grow and SotSort loses by
9–24% with the same memory, and by 5–10% against CPython-style Powersort with n/2 memory.
Comparisons are within 2% for S ≥ 100.

Check against the paper's own data (`average-time.txt`, i7-4770): our absolute times for
`almost` are close to theirs (e.g. S = 10^6: 0.129 s here, 0.128 s there; S = 2: 0.93 vs
0.82), but there `copy` is about 10–25% faster than `almost` and here they are within 4%.
So the relative cost of their own variants is not reproduced on this machine.

**Second machine** (`results-pc/`: Intel Core Ultra 7 155H, Linux VM with 2 vCPU on Windows,
gcc 11.4, n = 10^6, 10 iterations). `sotvm / almost`:

| type    | S = 2 | 100  | 1000 | 10^4 | 10^5 | 10^6 |
|---------|-------|------|------|------|------|------|
| int     | 1.08  | 1.09 | 1.07 | 1.12 | 1.14 | 0.99 |
| ptr     | 0.93  | 1.00 | 1.05 | 1.11 | 1.14 | 1.04 |
| arr     | 0.87  | 0.90 | 0.92 | 0.99 | 1.11 | 0.83 |
| fastarr | 1.39  | 1.21 | 1.30 | 1.35 | 1.35 | 0.81 |

Comparison and assignment counts are identical to the cloud run (deterministic). Same picture:
roughly a tie on cheap moves, a clear loss on expensive moves. Both machines are VMs with
2 vCPU; a bare-metal run is still missing.

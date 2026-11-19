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

## Update 3: v8-r4, page merge (`results-r4/`, `results-pc-r4/`)

**Correction first.** The rejected cycle-leader rotation leaked into `main` with commit
`712122d` (it was left in the working tree); v8-r4 removes it. All "before" numbers below are
from a clean v8-r3 (`826125b`). The previous A/B on fastarr was contaminated and is not used.

**What changed in v8-r4.**
1. *Page merge.* When both runs of a merge exceed the buffer, the range is split into pages
   of B/8 elements and the buffer supplies 8 spare pages. Output pages go to any free page
   (spare or already consumed input) and a final cycle-following pass puts them in order.
   No rotations. The idea is Virtual-Memory Powersort's, adapted to a single merge. The page
   table (4 bytes per page) is allocated on the heap and only used when it is at most 1/8 of
   the buffer's bytes; otherwise SymMerge with rotations is used as before.
2. *Large elements take the merge path.* With sizeof(T) ≥ 64 and page merge available, the
   routing always picks the natural merge sort, also on random input (`SOT_MERGE_BYTES`).

Moves by phase (int, n = 950 000, same memory as VM-Powersort): rotations go from 3.75–4.30 to
0 per element. Counts, cloud run (3 iterations, per element):

| assignments | S=2   | 100   | 1000  | 10^4  | 10^5 | 10^6 |
|-------------|-------|-------|-------|-------|------|------|
| int VM-Powersort | 32.36 | 17.74 | 12.17 | 9.10  | 6.07 | 3.70 |
| int SotSort r3   | 34.86 | 22.33 | 17.05 | 12.49 | 8.22 | 2.26 |
| int SotSort r4   | 34.86 | 20.14 | 14.89 | 10.30 | 5.71 | 1.38 |
| fastarr VM-Powersort | 32.51 | 18.01 | 12.75 | 9.25 | 6.06 | 3.71 |
| fastarr SotSort r3   | 48.96 | 30.83 | 26.20 | 21.91 | 15.48 | 4.03 |
| fastarr SotSort r4   | 32.47 | 21.91 | 17.18 | 12.04 | 6.13 | 1.48 |

Time, `sotvm / almost` (same memory), r3 → r4, n = 10^6, 10 iterations:

| machine | type    | S=2  | 100  | 1000 | 10^4 | 10^5 | 10^6 |
|---------|---------|------|------|------|------|------|------|
| cloud   | int     | 1.09→1.08 | 1.18→1.06 | 1.06→1.02 | 1.05→0.98 | 1.15→0.92 | 1.00→0.77 |
| cloud   | ptr     | 1.09→1.05 | 1.06→0.98 | 1.02→0.90 | 1.06→0.93 | 1.01→0.92 | 1.03→0.92 |
| cloud   | fastarr | 1.50→1.07 | 1.30→1.04 | 1.59→1.08 | 1.66→1.16 | 1.48→0.94 | 0.82→0.58 |
| cloud   | arr     | 1.07→0.80 | 0.81→0.82 | 0.85→0.89 | 0.88→1.04 | 0.91→1.03 | 0.80→0.78 |
| PC      | int     | 1.04→1.05 | 1.12→1.07 | 1.11→1.06 | 1.11→1.02 | 1.16→0.97 | 0.91→0.75 |
| PC      | ptr     | 0.98→0.96 | 0.99→0.99 | 1.04→0.86 | 1.09→1.24 | 1.11→0.97 | 0.94→1.00 |
| PC      | fastarr | 1.44→1.03 | 1.29→0.96 | 1.23→0.95 | 1.32→0.97 | 1.21→0.89 | 0.78→0.51 |
| PC      | arr     | 0.79→0.65 | 0.87→0.60 | 1.01→0.72 | 1.07→0.71 | 1.01→0.66 | 0.79→0.63 |

Reading: with the same memory SotSort r4 is now faster than or within noise of
Virtual-Memory Powersort in most cells, and clearly faster at S = 10^6 and on large elements
with slow comparisons (`arr`). Where it still loses beyond noise: random integers (S = 2,
1.05–1.08: the quicksort makes more moves than Powersort's merges there), and the cloud
`fastarr` S = 10^4 cell (1.16; 0.97 on the PC). On random `fastarr` input, routing to merges
costs 15% more comparisons than the quicksort (22.16 vs 19.31 per element) but saves a third
of the moves. Both machines are 2-vCPU VMs, and the `arr` and `ptr` rows disagree between
them by up to 0.3 in some cells, so cells within about ±10% should be read as ties.

## Update 4: v8-r5, paged runs and an honest memory budget (`results-fin/`, `results-pc-fin/`)

**What changed in v8-r5.**
1. *Paged runs.* When the buffer is at most n/8 and the first 17 detected runs have a mean
   length of at least 32, the natural merge path uses Virtual-Memory Powersort's scheme: each
   run is copied to pages on detection, merges go page to page without permuting, and the
   single final run is moved back once (pages already in place are not moved). Run detection
   is SotSort's (descending reversal, insertion, disordered zones). Otherwise the v8-r4 path
   (page merge only for merges larger than the buffer) is used.
2. *Memory budget.* `make_plan` now counts everything allocated against `max_bytes`: the
   buffer rounded as it is allocated (64 bytes, or 2 MB from 4 MB), the internal 16 KB buffer,
   and the page tables (at most 1/8 of the buffer plus 4 KB). `tests/test_mem.cpp` checks the
   peak. The only exception is a budget below the internal buffer itself (O(1) mode).
3. *O(1) mode.* Page merge and paged runs are disabled with the internal buffer (no heap
   tables there; this also fixes an aliasing bug between paging and disordered zones that the
   test suite caught before release).
4. *Code generation.* With gcc -O2 the larger functions had left the comparator or the
   insertion loop out of line (up to 30% with expensive comparisons); the hot loops are now
   kept inline explicitly. Found by comparing operation counts (identical) with times.

So "same memory" below is now literal for SotSort; v8-r4 (`sot4*`, previous `main`) could
exceed the budget by its page tables, the internal buffer and allocation rounding.

Both versions are in the same binary (`sot4*` = v8-r4 at `a0ec5dc`, `sot*` = v8-r5), n = 10^6,
10 runs for time, 2 for counts. Time, SotSort / VM-Powersort with the same memory:

| machine | type    | S=2  | 100  | 1000 | 10^4 | 10^5 | 10^6 |
|---------|---------|------|------|------|------|------|------|
| cloud   | int     | 1.01 | 0.88 | 0.88 | 0.89 | 1.00 | 0.78 |
| cloud   | ptr     | 1.05 | 0.79 | 0.82 | 0.80 | 0.93 | 0.97 |
| cloud   | fastarr | 1.10 | 0.79 | 0.89 | 0.88 | 0.90 | 0.57 |
| cloud   | arr     | 0.80 | 0.84 | 0.85 | 0.96 | 0.94 | 0.77 |
| PC      | int     | 0.99 | 0.91 | 0.89 | 0.83 | 0.98 | 0.72 |
| PC      | ptr     | 1.17 | 0.68 | 0.63 | 0.71 | 0.92 | 0.85 |
| PC      | fastarr | 0.97 | 0.84 | 0.84 | 0.87 | 0.89 | 0.52 |
| PC      | arr     | 0.67 | 0.79 | 0.84 | 0.82 | 0.66 | 0.58 |

Moves per element (cloud, int): VM-Powersort 31.65 / 17.71 / 12.24 / 8.89 / 5.83 / 4.04;
SotSort v8-r5 37.74 / 15.43 / 11.90 / 8.58 / 5.49 / 2.08. Comparisons are within 2% of
VM-Powersort for S ≥ 1000 and 11–32% fewer for S ≤ 100.

Reading: with the same memory, the ratio is below 1.00 in 43 of 48 cells across both
machines, down to 0.52 (about 1.9x faster, `fastarr`, S = 10^6). The five cells at or above
1.00 are cloud `int` S = 10^5 (1.00) and near-random input (S = 2): `int` ties (1.01, 0.99),
and SotSort loses on `ptr` (1.05 and 1.17; the quicksort makes more
moves than the merges, 37.74 vs 31.65 per element on `int`, and with the stricter budget its
blocks are smaller) and on cloud `fastarr` (1.10; 0.97 on the PC). With n/2 memory against
CPython-style Powersort (`summary.txt`, last column) SotSort still loses on slow comparisons
(`ptr` 1.11–1.56) and wins or ties on slow moves; that comparison was not the target here.
Both machines are 2-vCPU VMs; the same build was measured on both; cells within about ±10%
should still be read as ties.

## Update 5 — v8-r5 at n = 10^7 (int, cloud VM)

`cells_1e7.sh`, binary with v8-r5 (b9e0df1), maxn = 10^7, 10 runs for time and 3 for counts; raw output in `results-1e7-r5/`.
Per-element counts divided by 0.95·10^7.

| S | sotvm / almost | sothalf / copy | cmp VM-PS / Sot | moves VM-PS / Sot |
|---|---|---|---|---|
| 2 | 1.09 | 1.11 | 26.80 / 22.30 | 30.71 / 49.58 |
| 100 | 0.97 | 1.10 | 17.26 / 16.93 | 19.37 / 18.69 |
| 1000 | 0.96 | 1.07 | 13.58 / 13.58 | 15.55 / 15.31 |
| 10^4 | 0.97 | 1.05 | 10.52 / 10.52 | 12.50 / 12.27 |
| 10^5 | 1.04 | 1.07 | 7.24 / 7.24 | 9.22 / 8.99 |
| 10^6 | 1.07 | 1.01 | 3.98 / 3.98 | 5.96 / 5.96 |

At 10^6 and 10^5 operation counts are equal or lower for SotSort while time is 4–7% higher; the cause is not identified (single machine, no hardware counters). copy/almost here ranges 0.95–1.05.

**Second machine at n = 10^7** (`results-pc-1e7-r5/`, `cells_1e7_pc.sh`, time only, 10 iterations, same binary as `results-pc-fin`):
`sotvm / almost` = 1.09, 0.95, 0.93, 0.93, 0.95, 0.99 for S = 2 … 10^6; `sothalf / copy` = 1.06–1.09; `copy / almost` = 0.98–1.01.
The 4–7% loss at S = 10^5 and 10^6 on the cloud VM does not appear here.

## Update 6 — stable paged samplesort (`sotps`, experimental)

`paged_samplesort.h` (copy into the harness `src/`; `sotsort-harness.patch` now also adds the `sotps` entry).
Routing as SotSort; on near-random input it distributes into k = 64 groups through pages (stable by flush order,
see `docs/IDEA_particion_paginada.md` §2d–2e) and sorts in-buffer ranges with SotSort's quicksort.
Its buffer, page tables, sample and the base-case `Sorter` all come out of VM-Powersort's own budget (`vm_bytes`).
Counts per element (n uniform in [0.9·10^6, 10^6], divided by 0.95·10^6), results in `results-ps/`:

| type | S | comparisons VM-PS / sotvm / sotps | moves VM-PS / sotvm / sotps |
|---|---|---|---|
| int | 2 | 27.76 / 18.92 / 18.98 | 31.65 / 37.74 / **22.14** |
| fastarr | 2 | 28.09 / 21.92 / 19.08 | 32.01 / 32.13 / **17.88** |
| int, n ≈ 10^7 (1 run) | 2 | 26.07 / 21.86 / 21.92 | 29.90 / 48.18 / **26.49** |

For S ≥ 100 `sotps` takes SotSort's path; counts equal `sotvm` within 0.02 comparisons per element (the routing probe).
Time at S = 2, n ≈ 10^6, 10 runs, cloud VM, ratio to VM-PS: int 1.05, ptr 0.84, fastarr 0.96, arr 0.61
(`sotvm`: 1.08, 1.04, 1.08, 0.71). Single machine, single session: treat as indicative only.

## Update 7 — v8-r6: paged samplesort integrated

`include/sotsort.hpp` now uses the stable paged samplesort for near-random input in block mode (`pss_sort`;
`SOT_NO_PAGED_SAMPLE` turns it off), and the large-element rule only sends to the merge path when it is not available.
The quicksort tolerates at most 4 bad partitions (`SOT_BAD_BUDGET`). Harness entries `sotvm`/`sothalf` with the
current header; counts per element (`results-r6/`, n ≈ 10^6, divided by 0.95·10^6):

| type | S | cmp VM-PS / sotvm | moves VM-PS / sotvm | cmp copy / sothalf | moves copy / sothalf |
|---|---|---|---|---|---|
| int | 2 | 27.76 / 18.96 | 31.65 / 22.10 | 27.76 / 18.96 | 35.52 / 22.11 |
| int | 100 | 15.44 / 13.76 | 17.71 / 15.43 | 15.47 / 14.10 | 20.70 / 19.11 |
| int | 1000 | 10.29 / 10.28 | 12.24 / 11.90 | 10.29 / 10.44 | 13.54 / 13.96 |
| int | 10^4 | 6.94 / 6.96 | 8.89 / 8.58 | 6.94 / 7.04 | 8.48 / 8.94 |
| int | 10^5 | 3.85 / 3.71 | 5.83 / 5.49 | 3.85 / 3.75 | 3.89 / 4.49 |
| int | 10^6 | 2.03 / 2.04 | 4.04 / 2.08 | 2.03 / 2.07 | 1.45 / 1.73 |
| fastarr | 2 | 28.09 / 19.10 | 32.01 / 17.39 | 28.09 / 19.12 | 35.90 / 22.32 |
| fastarr | 100 | 15.87 / 14.04 | 18.17 / 15.89 | 15.88 / 14.39 | 21.23 / 19.50 |
| fastarr | 1000 | 10.53 / 10.51 | 12.51 / 12.29 | 10.53 / 10.67 | 13.86 / 14.29 |
| fastarr | 10^4 | 7.21 / 7.23 | 9.19 / 9.01 | 7.21 / 7.31 | 8.89 / 9.36 |
| fastarr | 10^5 | 4.02 / 4.03 | 6.02 / 6.01 | 4.02 / 4.10 | 4.28 / 4.77 |
| fastarr | 10^6 | 2.09 / 2.11 | 4.07 / 2.21 | 2.09 / 2.16 | 1.31 / 1.31 |

n ≈ 10^7, int, S = 2 (one run): comparisons 26.07 / 21.87 (VM-PS / sotvm), moves 29.90 / 23.50.
Against VM-Powersort with the same memory, sotvm now makes no more comparisons or moves in any cell
(largest excess: +0.02 comparisons). With n/2 memory against copy-Powersort it still makes more moves for S ≥ 1000.
Indicative times (cloud VM, 10 runs, one session), sotvm / VM-PS: int 1.02, 0.98, 0.83 (S = 2, 100, 10^6);
ptr 0.88, 0.72, 0.98; fastarr 0.89, 0.86, 0.56; arr 0.59, 0.88, 0.81.

**Second machine, v8-r6** (`results-pc-r6/`, Core Ultra 7 155H VM, gcc 11.4, n ≈ 10^6, 10 runs, time only):
sotvm / VM-PS for S = 2, 100, 10^6: int 1.12, 0.96, 0.78; ptr 0.78, 0.63, 0.93; fastarr 0.94, 0.82, 0.52; arr 0.54, 0.78, 0.61.
On this machine int at S = 2 is 12% slower in time despite 30% fewer moves and 32% fewer comparisons: with cheap
elements the branchy binary-search classification (one unpredictable branch per level of the search) likely dominates;
IPS4o uses a branchless decision tree for this. Not measured with hardware counters.

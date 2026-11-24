# SotSort

A stable sort for C++17, header-only. One algorithm, two memory regimes:

- `sot::stable_sort(a, n, cmp)` — O(1) extra memory: a ~16 KB buffer on the stack, no heap, no recursion.
- `sot::stable_sort_fast(a, n, cmp, opts)` — uses whatever memory is available (up to an n-element
  buffer), with galloping merges, a counting shortcut for small-range integers, and threads.
- `sot::stable_sort_by_key(a, n, key, opts)` — stable sort by an integer key.

The core is a stable quicksort: blocks are partitioned through the buffer and gathered with
rotations; leaves are sorted with parity merges; inputs with long runs go to a natural merge sort.
Integer keys with `std::less` use an AVX-512 (compress) or AVX2 (table) block partition. With a
bounded buffer (smaller than n/2) and near-random input, version 8-r6 replaces the top of the
quicksort by a stable k-way samplesort that works through pages (see below); 8-r7 gives equal keys
groups of their own in it.

```cpp
#include "sotsort.hpp"
std::vector<Record> v = ...;
sot::stable_sort(v.data(), v.size(), [](const Record& a, const Record& b) { return a.key < b.key; });
```

## Status — read this first

This is version 8-r9. The technical report (`docs/sotsort_report.pdf`, revision 6; arXiv:2611.24786
has earlier revisions) describes it. Commits: `c16d43f` (8-r), `b9e0df1` (8-r5), `ef55874` (8-r6); 8-r7 adds
equality groups to the paged samplesort and galloping to the paged run merge; 8-r8 chooses the
samplesort's splitters and fan-out from the sample and skips subranges that are already sorted;
8-r9 removes a time regression of 8-r8 in the paged run merge (13–53% at S = 100).
Development history and design notes (Spanish): `docs/RECONSTRUCCION.md`; the paged partition and
samplesort, with proofs, tests and prior art: `docs/IDEA_particion_paginada.md`.

## What the measurements say

Numbers are preliminary. Machine-independent counts (comparisons and element moves) are the main
result; times are indicative, from shared 2-vCPU VMs where identical runs moved by up to ±15%.

**Against Virtual-Memory Powersort** (Moltmann, Nakajima, Wild, ESA 2026), on its authors' harness,
with exactly the memory it allocates (`bench/vmpowersort/`). Per element, n ≈ 10^6:

| type, S (mean run length) | comparisons VM-PS / SotSort | moves VM-PS / SotSort |
|---|---|---|
| int, S = 2 (near-random) | 27.76 / 18.96 | 31.65 / 22.10 |
| int, S = 1000 | 10.29 / 10.28 | 12.24 / 11.90 |
| int, S = 10^6 | 2.03 / 2.04 | 4.04 / 2.08 |
| fastarr (120-byte), S = 2 | 28.09 / 19.10 | 32.01 / 17.39 |

On all 12 measured cells (int and fastarr, S from 2 to 10^6) SotSort makes no more comparisons
(up to +0.02 per element) and no more moves. With n/2 memory against copy-Powersort it still makes
more moves for S ≥ 1000. Time at S = 2 relative to VM-PS (two VMs): int 0.72–0.78, ptr 0.82–0.86,
fastarr 0.73, arr 0.55–0.57. An adaptive adversary that knows the seed raises the samplesort to
about 34 moves per element (n = 10^6); VM-Powersort, a mergesort, has no such weakness.

**Against in-place and O(n)-memory sorts** (version 8-r5; the 8-r6 changes do not reach these paths).
One cloud VM (Xeon, AVX-512), gcc 13, n = 10^6, median of 5, pinned. Raw data: `results/`.
Time ratio SotSort (O(1) mode) / blitsort — below 1 means SotSort is faster:

| Input | u32 | u64 | key+index (comparator) |
|---|---|---|---|
| random | 0.82 | 0.71 | 1.04 |
| 16 values | 0.47 | 0.75 | 0.87 |
| √n values | 0.32 | 0.57 | 0.59 |
| Zipf | 0.45 | 0.66 | 0.87 |
| sorted | 0.99 | 1.07 | 2.31 |
| reversed | 0.92 | 1.01 | 1.62 |
| 1% noise | 1.04 | 0.63 | 0.84 |
| 10% random tail | 0.65 | 0.72 | 0.85 |
| organ pipe | 0.55 | 0.65 | 0.78 |
| runs of √n | 0.58 | 0.54 | 1.13 |
| runs of 63 | 0.36 | 0.58 | 0.70 |

- With 32- and 64-bit integers it is ahead of blitsort on 17 of 22 input classes (0.32–0.82) and
  level on the other five (0.92–1.07). With a comparator that rules out the vector partition it
  **loses on 4 of 11 classes, by up to 2.3× on sorted input**.
- The fast mode (full buffer) is ahead of fluxsort, glidesort and driftsort on random input and
  behind glidesort on organ pipe, a random tail and runs of √n (1.5–2.1× for u64).
- The routing heuristics were tuned on this machine and on some of these same inputs, so the
  structured-input rows are optimistic.
- Extra memory for 10^7 u64 (80 MB of data): under 0.5 MB in O(1) mode; about 37 MB in fast mode.
- McIlroy's adversary with random pre-frozen positions: at most 0.70·n·log2(n) comparisons
  (n = 10^5 and 10^6; 1.14 before the 8-r6 cap on bad partitions). Evidence, not a proof.
- A defect of version 6 (no budget charge when separating elements equal to the pivot) is
  reproducible with `-DSOT_V6_BUG`: a lazy adversary forces about n/6 comparisons per element.

## Known limitations

- If the comparator throws, elements can be lost (moved-from objects). No memory errors.
- `T` must be default-constructible and copyable.
- Tested on x86-64 Linux only. Not tested: ARM, Windows, cgroup v1, more than 2 physical cores.
- The proofs are hand-written (Spanish, `docs/`); nothing is machine-checked.
- The paged samplesort only runs with a bounded block buffer; in O(1) mode its page tables would not
  fit, so the quicksort runs. Its worst case is bounded by O(n log n) but measured, not proved tight.
- Against Logsort (operation counts only, `bench/cost/logsort_counts.cpp`, same element type and
  memory, n = 10^7): with a 16 KB buffer Logsort makes fewer moves than SotSort on random input
  (99 vs 112 per element) and with many equal keys (31 vs 46 with 16 distinct keys); with
  VM-Powersort's budget SotSort makes 3–13 times fewer moves on the inputs that reach the paged
  samplesort, and no more comparisons. Timings against Logsort are not measured.
- With a bounded buffer, nearly sorted inputs with sparse disorder (1 in 63 or 1% of elements out of
  place) make 5.6 and 4.5 comparisons per element at n = 10^7 (17.7 and 16.8 before 8-r7), against
  about 2 in O(1) mode, which pays 31–36 moves instead of 20.
- Skewed keys with a bounded buffer (Zipf-like): 5.3 comparisons per element against 4.4 in O(1)
  mode (which pays 40 moves instead of 4.7). The samplesort's choice of splitters and fan-out is
  a cost model on the sample (heuristic); no adversary was built against it.

## Layout

- `include/sotsort.hpp` — the library.
- `include/experimental/sotsort_ooo.hpp` — experimental dependency-driven task scheduler
  (`sot::stable_sort_ooo`). Not part of the stable API; see `docs/OOO.md`.
- `tests/` — differential tests against `std::stable_sort`, injected allocation failures,
  adversaries. `make test`, `make sanitize`, `make adversary`.
- `bench/cost/` — operation-count experiments (partition cost model, paged partition and samplesort
  prototypes, adversaries).
- `bench/` — comparison harness. `bench/build.sh` fetches the third-party sorts.
- `docs/` — technical report (English); proofs, addendum, design notes, OOO study and
  lemmas (Spanish), with LaTeX sources in `docs/tex/`.

## Credits

The fused triple-reversal rotation follows blitsort's trinity rotation and the equal-length parity
merge comes from quadsort (both by Igor van den Hoven, "scandum"). Natural-run merging uses the
power criterion of powersort (Munro and Wild). Galloping comes from Timsort (Tim Peters). The
duplicate handling through a lower bound is inspired by pdqsort (Orson Peters), and the fallback
on unbalanced partitions by introsort (Musser). SymMerge is by Kim and Kutzner. The page merge and
the paged runs follow Virtual-Memory Powersort (Moltmann, Nakajima, Wild). In the paged samplesort,
reusing consumed blocks and recording each block's group comes from Radsort (Clausecker and
Schintke), grouping into one-type blocks restored by cycles from Logsort (aphitorite), and block
distribution with a branchless splitter tree from IPS⁴o (Axtmann, Witt, Ferizovic, Sanders).

## License

Mozilla Public License 2.0 (`LICENSE`). In short: you may use SotSort in any program, open or
closed; if you distribute modified versions of SotSort's own files, those files must stay under the
MPL and their source must be available. This summary is not legal advice; the license text governs.

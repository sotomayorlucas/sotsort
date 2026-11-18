# SotSort

A stable sort for C++17, header-only. One algorithm, two memory regimes:

- `sot::stable_sort(a, n, cmp)` — O(1) extra memory: a ~16 KB buffer on the stack, no heap, no recursion.
- `sot::stable_sort_fast(a, n, cmp, opts)` — uses whatever memory is available (up to an n-element
  buffer), with galloping merges, a counting shortcut for small-range integers, and threads.
- `sot::stable_sort_by_key(a, n, key, opts)` — stable sort by an integer key.

The core is a stable quicksort: blocks are partitioned through the buffer and gathered with
rotations; leaves are sorted with parity merges; inputs with long runs go to a natural merge sort.
Integer keys with `std::less` use an AVX-512 (compress) or AVX2 (table) block partition.

```cpp
#include "sotsort.hpp"
std::vector<Record> v = ...;
sot::stable_sort(v.data(), v.size(), [](const Record& a, const Record& b) { return a.key < b.key; });
```

## Status — read this first

This is version 8-r. The technical report describes and measures this code, at tag `report-r3`.
Development history and design notes (Spanish): `docs/RECONSTRUCCION.md`.

## What the measurements say

One machine: a 2-vCPU cloud VM (Xeon, AVX-512), gcc 13, n = 10^6, median of 5, pinned to one core.
Run-to-run noise on this machine reached ±15%. Raw data: `results/`. Do not cite these numbers
without repeating them on dedicated hardware.

Time ratio SotSort (O(1) mode) / blitsort — below 1 means SotSort is faster:

| Input | u32 | u64 | key+index (comparator) |
|---|---|---|---|
| random | 0.63 | 0.72 | 1.14 |
| 16 values | 0.46 | 0.62 | 0.80 |
| √n values | 0.19 | 0.49 | 0.86 |
| Zipf | 0.44 | 0.50 | 0.64 |
| sorted | 1.09 | 1.00 | 2.57 |
| reversed | 0.94 | 1.01 | 1.81 |
| 1% noise | 1.08 | 1.06 | 1.08 |
| 10% random tail | 0.67 | 0.75 | 1.50 |
| organ pipe | 0.58 | 0.70 | 1.13 |
| runs of √n | 0.53 | 0.75 | 1.23 |
| runs of 63 | 0.42 | 0.48 | 0.78 |

- With 32- and 64-bit integers it is ahead of blitsort on 16 of 22 input classes (0.19–0.75) and
  level on the other six (0.94–1.09). With a comparator that rules out the vector partition it
  **loses on 7 of 11 classes, by up to 2.6× on sorted input**.
- Against O(n)-memory stable sorts (fluxsort, glidesort, driftsort), the fast mode with one thread
  is ahead on random u32/u64 (0.70–0.79 of the best of them) and behind glidesort on organ pipe,
  a random tail and sawtooth input (1.4–1.9× for u64). The "16 values" and "√n values" rows of the
  fast mode use the counting shortcut, so they are not a like-for-like comparison.
- The routing heuristics were tuned on this machine and on some of these same inputs, so the
  structured-input rows are optimistic.
- Extra memory for 10^7 u64 (80 MB of data): under 0.5 MB in O(1) mode; about 40 MB in fast mode.
- McIlroy's adversary with random pre-frozen positions: at most 1.14·n·log2(n) comparisons
  (n = 10^5 and 10^6). This is evidence, not a proof.
- A defect of version 6 (no budget charge when separating elements equal to the pivot) is
  reproducible with `-DSOT_V6_BUG`: a lazy adversary forces about n/6 comparisons per element.

## Known limitations

- If the comparator throws, elements can be lost (moved-from objects). No memory errors.
- `T` must be default-constructible and copyable.
- Tested on x86-64 Linux only. Not tested: ARM, Windows, cgroup v1, more than 2 physical cores.
- The proofs are hand-written (Spanish, `docs/`); nothing is machine-checked.

## Layout

- `include/sotsort.hpp` — the library.
- `include/experimental/sotsort_ooo.hpp` — experimental dependency-driven task scheduler
  (`sot::stable_sort_ooo`). Not part of the stable API; see `docs/OOO.md`.
- `tests/` — differential tests against `std::stable_sort`, injected allocation failures,
  adversaries. `make test`, `make sanitize`, `make adversary`.
- `bench/` — comparison harness. `bench/build.sh` fetches the third-party sorts.
- `docs/` — technical report (English); proofs, addendum, design notes, OOO study and
  lemmas (Spanish), with LaTeX sources in `docs/tex/`.

## Credits

The fused triple-reversal rotation follows blitsort's trinity rotation and the equal-length parity
merge comes from quadsort (both by Igor van den Hoven, "scandum"). Natural-run merging uses the
power criterion of powersort (Munro and Wild). Galloping comes from Timsort (Tim Peters). The
duplicate handling through a lower bound is inspired by pdqsort (Orson Peters), and the fallback
on unbalanced partitions by introsort (Musser). SymMerge is by Kim and Kutzner.

## License

Mozilla Public License 2.0 (`LICENSE`). In short: you may use SotSort in any program, open or
closed; if you distribute modified versions of SotSort's own files, those files must stay under the
MPL and their source must be available. This summary is not legal advice; the license text governs.

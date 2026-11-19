// Compilar con -I include [-DSOT_BAD_BUDGET=k]. Conteos de SotSort en distribuciones de bench/.
// Conteos de SotSort (O(1) y con presupuesto VM) en varias distribuciones.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>
#include <algorithm>
static unsigned long long MV, CMP;
struct W { long k; std::uint32_t id; W() = default; W(long a, std::uint32_t i) : k(a), id(i) {}
  W(const W& o) : k(o.k), id(o.id) { ++MV; } W& operator=(const W& o) { k = o.k; id = o.id; ++MV; return *this; }
  W(W&& o) : k(o.k), id(o.id) { ++MV; } W& operator=(W&& o) { k = o.k; id = o.id; ++MV; return *this; } };
struct Lt { bool operator()(const W& a, const W& b) const { ++CMP; return a.k < b.k; } };
#include "sotsort.hpp"
int main() {
  const std::size_t n = 1000000; std::mt19937_64 g(3);
  const char* names[] = {"rand", "dup16", "sqrtn", "zipf", "organ", "runs63", "tail10", "asc1pct", "median3killer"};
  for (int d = 0; d < 9; ++d) {
    std::vector<W> v(n);
    for (std::size_t i = 0; i < n; ++i) {
      long x;
      switch (d) {
        case 0: x = (long)(g() >> 1); break;
        case 1: x = (long)(g() % 16); break;
        case 2: x = (long)(g() % 1000); break;
        case 3: { double u = (double)(g() >> 11) / 9007199254740992.0; x = (long)std::pow(1.0 / (u + 1e-9), 1.2); } break;
        case 4: x = (long)(i < n / 2 ? i : n - i); break;
        case 5: x = (long)((i % 63) == 0 ? g() % n : i); break;
        case 6: x = (long)(i < n * 9 / 10 ? i : g() % n); break;
        case 7: x = (long)(g() % 100 == 0 ? g() % n : i); break;
        default: x = (long)((i & 1) ? i : n + (i >> 1)); break;   // pares/impares intercalados
      }
      v[i] = W(x, (std::uint32_t)i);
    }
    for (int mode = 0; mode < 2; ++mode) {
      auto w = v; MV = CMP = 0;
      if (mode == 0) sot::stable_sort(w.data(), n, Lt());
      else { sot::Options o; o.threads = 1; o.probe = false; o.shortcuts = false; o.max_bytes = 360000; sot::stable_sort_fast(w.data(), n, Lt(), o); }
      printf("%-14s %s cmp %6.2f mov %6.2f\n", names[d], mode ? "VMbud" : "O(1) ", (double)CMP / n, (double)MV / n);
    }
  }
}

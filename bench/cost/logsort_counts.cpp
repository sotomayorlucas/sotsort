// Conteos de comparaciones y movimientos: Logsort (aphitorite) vs SotSort.
// Compilar: g++ -O2 -std=c++17 -I ../../include logsort_counts.cpp -o /tmp/lc
// Logsort: bLen = B0 (el buffer propio de SotSort en modo O(1), misma memoria) y bLen = presupuesto
// de VM-Powersort. Su propio benchmark usa bLen = 512.
// SotSort: modo O(1) (stable_sort) y con el presupuesto de VM-Powersort.
// Un movimiento = construcción o asignación de un elemento; una comparación = una llamada
// al comparador (CMP de Logsort es de tres vías y cuenta como una).
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>
#include <algorithm>
static unsigned long long MV, NC;
struct W {
  long k; std::uint32_t id;
  W() = default; W(long a, std::uint32_t i) : k(a), id(i) {}
  W(const W& o) : k(o.k), id(o.id) { ++MV; }
  W& operator=(const W& o) { k = o.k; id = o.id; ++MV; return *this; }
  W(W&& o) : k(o.k), id(o.id) { ++MV; }
  W& operator=(W&& o) { k = o.k; id = o.id; ++MV; return *this; }
};
struct Lt { bool operator()(const W& a, const W& b) const { ++NC; return a.k < b.k; } };

#define VAR W
static inline int log_cmp3(const W* a, const W* b) { ++NC; return (a->k > b->k) - (a->k < b->k); }
#define CMP(a, b) log_cmp3((a), (b))   // función: Logsort pasa a++ como argumento
#define LOG_COPY(d, s, k) do { W* d_ = (d); const W* s_ = (s); for (size_t q_ = 0, k_ = (k); q_ < k_; ++q_) d_[q_] = s_[q_]; } while (0)
#define LOG_MOVE(d, s, k) do { W* d_ = (d); W* s_ = (s); size_t k_ = (k); \
    if (d_ > s_) { for (size_t q_ = k_; q_-- > 0;) d_[q_] = s_[q_]; } else { for (size_t q_ = 0; q_ < k_; ++q_) d_[q_] = s_[q_]; } } while (0)
#include "logsort/logsort.h"
#undef CMP
#include "sotsort.hpp"

template <typename T> static size_t vm_bytes(size_t n) {   // igual que en bench/vmpowersort
  const double target_p = sqrt((double)sizeof(T*) * 4 * n / (sizeof(T) * (log2(n) + 7)));
  const size_t log_n = 33 - __builtin_clz(n + 1), log_p = 33 - __builtin_clz((int)target_p + 1),
               co_log_p = log_n - log_p + 1, max_runs = log_n + 4, extra_pages = max_runs + 3,
               page_buf_sz = extra_pages << log_p, free_page_buf_sz = extra_pages + 1, ptr_buf_sz = 4 << co_log_p;
  return sizeof(T) * page_buf_sz + sizeof(T*) * (free_page_buf_sz + ptr_buf_sz);
}

static bool ok(const std::vector<W>& w) {
  for (size_t i = 1; i < w.size(); ++i)
    if (w[i - 1].k > w[i].k || (w[i - 1].k == w[i].k && w[i - 1].id > w[i].id)) return false;
  return true;
}

int main(int argc, char** argv) {
  const size_t n = argc > 1 ? (size_t)atof(argv[1]) : 1000000;
  const size_t vb = vm_bytes<W>(n), vbe = vb / sizeof(W), B0 = sot::Sorter<W>::B0;   // buffer propio de SotSort O(1)
  setvbuf(stdout, nullptr, _IONBF, 0);
  printf("n=%zu  sizeof(W)=%zu  B0=%zu elementos  presupuesto VM=%zu bytes (%zu elementos)\n", n, sizeof(W), B0, vb, vbe);
  printf("%-10s | %-17s | %-17s | %-17s | %-17s\n", "entrada", "Logsort(B0)", "SotSort O(1)", "Logsort(VM)", "SotSort VM");
  std::mt19937_64 g(3);
  const char* names[] = {"rand", "dup16", "sqrtn", "zipf", "organ", "runs63", "tail10", "asc1pct", "interl"};
  const long sq = (long)std::sqrt((double)n);
  for (int d = 0; d < 9; ++d) {
    std::vector<W> v(n);
    for (size_t i = 0; i < n; ++i) {
      long x;
      switch (d) {
        case 0: x = (long)(g() >> 1); break;
        case 1: x = (long)(g() % 16); break;
        case 2: x = (long)(g() % sq); break;
        case 3: { double u = (double)(g() >> 11) / 9007199254740992.0; x = (long)std::pow(1.0 / (u + 1e-9), 1.2); } break;
        case 4: x = (long)(i < n / 2 ? i : n - i); break;
        case 5: x = (long)((i % 63) == 0 ? g() % n : i); break;
        case 6: x = (long)(i < n * 9 / 10 ? i : g() % n); break;
        case 7: x = (long)(g() % 100 == 0 ? g() % n : i); break;
        default: x = (long)((i & 1) ? i : n + (i >> 1)); break;
      }
      v[i] = W(x, (std::uint32_t)i);
    }
    printf("%-10s", names[d]);
    for (int mode = 0; mode < 4; ++mode) {
      std::vector<W> w = v; MV = NC = 0;
      if (mode == 0) logsort(w.data(), n, B0);
      else if (mode == 1) sot::stable_sort(w.data(), n, Lt());
      else if (mode == 2) logsort(w.data(), n, vbe);
      else { sot::Options o; o.threads = 1; o.max_bytes = vb; if (!getenv("PROBE")) { o.probe = false; o.shortcuts = false; }
             sot::stable_sort_fast(w.data(), n, Lt(), o); }
      printf(" | %6.2f / %6.2f%s", (double)NC / n, (double)MV / n, ok(w) ? "  " : " X");
      fflush(stdout);
    }
    printf("\n");
  }
  printf("(cada celda: comparaciones / movimientos por elemento; X = salida no ordenada o inestable)\n");
}

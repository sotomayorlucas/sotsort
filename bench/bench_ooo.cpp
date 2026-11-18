// Comparación: v8-r secuencial (1 hilo), v8-r paralelo (rondas de merge path), SotSort-OOO.
#include "../include/experimental/sotsort_ooo.hpp"
#include <atomic>
#include <chrono>
#include <random>
#include <string>
#include <cstdio>
#include <sys/resource.h>
using u64 = uint64_t;
struct Big { u64 k; u64 id; char pad[112]; };
struct Str { std::string s; unsigned id; };
static std::atomic<u64> g_cmp{0};
struct CntLess { bool operator()(u64 a, u64 b) const { g_cmp.fetch_add(1, std::memory_order_relaxed); return a < b; } };
static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static std::vector<u64> gen(const std::string& d, size_t n) {
  std::mt19937_64 R(99); std::vector<u64> v(n);
  if (d == "rand") for (auto& x : v) x = R();
  else if (d == "asc") for (size_t i = 0; i < n; i++) v[i] = i;
  else if (d == "desc") for (size_t i = 0; i < n; i++) v[i] = n - i;
  else if (d == "asc1pct") { for (size_t i = 0; i < n; i++) v[i] = i; for (size_t k = 0; k < n / 100; k++) v[R() % n] = R() % n; }
  else if (d == "organ") for (size_t i = 0; i < n; i++) v[i] = i < n / 2 ? i : n - i;
  else if (d == "dup16") for (auto& x : v) x = R() % 16;
  else if (d == "sqrtn") { u64 m = (u64)std::sqrt((double)n); for (auto& x : v) x = R() % m; }
  else if (d == "pesado") for (auto& x : v) x = (R() % 2) ? 7 : R();                 // la mitad es un solo valor
  else if (d == "mitades") { for (size_t i = 0; i < n; i++) v[i] = i < n / 2 ? (R() % 8) : R(); }  // desbalance entre mitades
  return v;
}
template <class T, class C, class Mk> static void bench(const char* ty, const std::string& d, size_t n, C c, Mk mk, int reps, unsigned P) {
  auto k = gen(d, n); std::vector<T> base(n); for (size_t i = 0; i < n; i++) base[i] = mk(k[i], i);
  std::vector<T> ref = base; std::stable_sort(ref.begin(), ref.end(), c);
  double t[4][9]; double rootms = 0; sot::OooStats st; bool ok = true;
  for (int r = 0; r < reps; r++) for (int a = 0; a < 4; a++) {
    std::vector<T> w = base; sot::Options o; o.shortcuts = false; o.threads = a == 0 ? 1 : P;
    double t0 = now();
    if (a < 2) sot::stable_sort_fast(w.data(), n, c, o); else sot::stable_sort_ooo(w.data(), n, c, o, &st, size_t(1) << 15, a == 3);
    t[a][r] = now() - t0; if (a == 3) rootms = st.root_s * 1e3;
    if (r == 0) for (size_t i = 0; i < n && ok; i += 1 + (n > 2000000 ? 7 : 0)) ok = !c(w[i], ref[i]) && !c(ref[i], w[i]);
  }
  for (int a = 0; a < 4; a++) std::sort(t[a], t[a] + reps);
  double s = t[0][reps / 2] * 1e3, p = t[1][reps / 2] * 1e3, q = t[2][reps / 2] * 1e3, q2 = t[3][reps / 2] * 1e3;
  printf("%-5s %-8s %9zu sec=%7.1f par=%7.1f ooo=%7.1f ooo+raiz=%7.1f | ooo/par=%.2f ooo+raiz/par=%.2f acel=%.2f [%s tareas=%zu ocio=%.1fms raiz=%.1fms] %s\n",
         ty, d.c_str(), n, s, p, q, q2, q / p, q2 / p, s / q2, st.path, st.tasks, st.idle_s * 1e3, rootms, ok ? "ok" : "FALLA");
  fflush(stdout);
}
int main(int argc, char** argv) {
  size_t n = argc > 1 ? strtoull(argv[1], 0, 10) : 1000000; unsigned P = argc > 2 ? atoi(argv[2]) : 2; int reps = argc > 3 ? atoi(argv[3]) : 5;
  std::string what = argc > 4 ? argv[4] : "all";
  const char* ds[] = {"rand", "dup16", "sqrtn", "pesado", "mitades", "asc1pct", "organ", "asc", "desc"};
  for (auto d : ds) {
    if (what == "all" || what == "u64") bench<u64>("u64", d, n, std::less<u64>(), [](u64 k, size_t) { return k; }, reps, P);
    if (what == "all" || what == "big") bench<Big>("r128", d, n / 4, [](const Big& a, const Big& b) { return a.k < b.k; }, [](u64 k, size_t i) { Big b{}; b.k = k; b.id = i; return b; }, reps, P);
    if (what == "all" || what == "str") bench<Str>("str", d, n / 4, [](const Str& a, const Str& b) { return a.s < b.s; }, [](u64 k, size_t i) { return Str{"clave-larga-para-evitar-sso-" + std::to_string(k % 100000007), (unsigned)i}; }, reps, P);
  }
  if (what == "all" || what == "cmp") for (auto d : {"rand", "dup16", "pesado"}) {   // comparaciones (comparador contado)
    auto k = gen(d, n); u64 c[3];
    for (int a = 0; a < 3; a++) { auto w = k; sot::Options o; o.shortcuts = false; o.threads = a == 0 ? 1 : P; g_cmp = 0;
      if (a < 2) sot::stable_sort_fast(w.data(), n, CntLess(), o); else sot::stable_sort_ooo(w.data(), n, CntLess(), o); c[a] = g_cmp; }
    printf("comparaciones por elemento %-8s sec=%.2f par=%.2f ooo=%.2f\n", d, (double)c[0] / n, (double)c[1] / n, (double)c[2] / n);
  }
  struct rusage ru; getrusage(RUSAGE_SELF, &ru); printf("pico de RSS del proceso: %ld MB\n", ru.ru_maxrss / 1024);
}

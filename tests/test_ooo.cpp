// Prueba diferencial de stable_sort_ooo contra std::stable_sort (etiquetas para estabilidad).
#include "../include/experimental/sotsort_ooo.hpp"
#include <random>
#include <string>
#include <cstdio>
using u64 = uint64_t;
static std::mt19937_64 R(4242);
struct Pair { int k; unsigned id; };
struct Str { std::string s; unsigned id; };
struct Big { u64 k; u64 id; char pad[112]; };
static std::vector<u64> keys(size_t n, int d) {
  std::vector<u64> v(n); u64 m = std::max<u64>(1, n);
  switch (d) {
    case 0: for (auto& x : v) x = R(); break;
    case 1: for (auto& x : v) x = R() % 4; break;
    case 2: for (auto& x : v) x = R() % std::max<u64>(2, (u64)std::sqrt((double)m)); break;
    case 3: for (size_t i = 0; i < n; i++) v[i] = i; for (size_t k = 0; k < n / 50 + 1 && n; k++) v[R() % n] = R() % m; break;
    case 4: for (size_t i = 0; i < n; i++) v[i] = i < n / 2 ? i : n - i; break;
    case 5: for (size_t i = 0; i < n; i++) v[i] = (R() % 3 == 0) ? 7 : R() % 100000; break;   // un valor muy repetido
    case 6: for (size_t i = 0; i < n; i++) v[i] = n - i; break;
    default: for (auto& x : v) x = R() % 1000;
  }
  return v;
}
int main(int argc, char** argv) {
  int rounds = argc > 1 ? atoi(argv[1]) : 150; long fails = 0, cases = 0, ooo = 0;
  for (int it = 0; it < rounds; it++) {
    size_t n = it % 9 == 0 ? 200000 + R() % 400000 : (it % 9 == 4 ? 530000 + R() % 300000 : R() % 60000); int d = it % 8;
    auto k = keys(n, d);
    sot::Options o; o.threads = 2 + it % 4; o.shortcuts = it % 2; o.probe = false;
    if (it % 3 == 1) o.max_bytes = 0; else if (it % 3 == 2) o.max_bytes = 8 * (1 + R() % (n + 1));
    size_t grain = size_t(64) << (it % 7); sot::OooStats st;
#define RUN(V, C, NAME) { auto ref = V; std::stable_sort(ref.begin(), ref.end(), C); auto w = V; \
      sot::stable_sort_ooo(w.data(), n, C, o, &st, n > 500000 ? (size_t(1) << 15) : grain, it % 2 == 0); ++cases; ooo += st.path[0] == 'o' && st.path[1] == 'o'; bool ok = true; \
      for (size_t i = 0; ok && i < n; i++) ok = EQ(w[i], ref[i]); if (!ok) { ++fails; printf("FALLA %s n=%zu d=%d\n", NAME, n, d); } }
    { std::vector<u64> v(k); auto c = std::less<u64>();
#define EQ(a, b) ((a) == (b))
      RUN(v, c, "u64") }
#undef EQ
#define EQ(a, b) ((a).k == (b).k && (a).id == (b).id)
    { std::vector<Pair> v(n); for (size_t i = 0; i < n; i++) v[i] = {(int)(k[i] % 1000003), (unsigned)i};
      auto c = [](const Pair& a, const Pair& b) { return a.k < b.k; }; RUN(v, c, "pair") }
    if (it % 3 == 0) { std::vector<Big> v(n); for (size_t i = 0; i < n; i++) { v[i].k = k[i] % 5000; v[i].id = i; }
      auto c = [](const Big& a, const Big& b) { return a.k < b.k; }; RUN(v, c, "big") }
#undef EQ
#define EQ(a, b) ((a).s == (b).s && (a).id == (b).id)
    if (n < 150000) { std::vector<Str> v(n); for (size_t i = 0; i < n; i++) { v[i].s = "clave-larga-para-evitar-sso-" + std::to_string(k[i] % 5000); v[i].id = (unsigned)i; }
      auto c = [](const Str& a, const Str& b) { return a.s < b.s; }; RUN(v, c, "str") }
  }
  printf("casos=%ld (por tareas OOO: %ld) fallas=%ld\n", cases, ooo, fails);
  return fails != 0;
}

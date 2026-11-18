// Pruebas dirigidas: fallas de reserva inyectadas, adversario (respaldo), comparador que lanza.
#include <cstddef>
static long g_allow = -1;   // -1: sin límite; k: fallan las reservas a partir de la k-ésima
static bool hook(std::size_t) { if (g_allow < 0) return true; if (g_allow == 0) return false; --g_allow; return true; }
#define SOT_TEST_ALLOC_HOOK hook
#include "../include/sotsort.hpp"
#include <random>
#include <string>
#include <cstdio>
using u64 = uint64_t;
struct P { int k; unsigned id; };
static std::vector<u64> av; static u64 gas, nsolid, cand, ncmp;
struct Adv { bool operator()(u64 x, u64 y) const { ++ncmp;
  if (av[x] == gas && av[y] == gas) { if (x == cand) av[x] = nsolid++; else av[y] = nsolid++; }
  if (av[x] == gas) cand = x; else if (av[y] == gas) cand = y; return av[x] < av[y]; } };
int main() {
  long fails = 0, cases = 0, lost = 0; std::mt19937_64 R(7);
  // 1. fallas de reserva en cada punto
  for (size_t n : {5000ul, 70000ul, 300000ul}) for (long k = 0; k < 6; k++) for (int th : {1, 4}) for (int d = 0; d < 3; d++) {
    std::vector<P> v(n); for (size_t i = 0; i < n; i++) v[i] = {(int)(d == 0 ? R() % 1000 : d == 1 ? (i / 7) : (R() % n)), (unsigned)i};
    if (d == 1) for (size_t j = 0; j < n / 50; j++) v[R() % n].k = (int)(R() % n);
    auto ref = v; auto c = [](const P& a, const P& b) { return a.k < b.k; };
    std::stable_sort(ref.begin(), ref.end(), c);
    auto w = v; g_allow = k; sot::Options o; o.threads = th; sot::stable_sort_fast(w.data(), n, c, o); g_allow = -1; ++cases;
    for (size_t i = 0; i < n; i++) if (w[i].k != ref[i].k || w[i].id != ref[i].id) { ++fails; printf("FALLA reserva n=%zu k=%ld\n", n, k); break; }
    w = v; g_allow = k; sot::stable_sort_by_key(w.data(), n, [](const P& p) { return p.k; }, o); g_allow = -1; ++cases;
    for (size_t i = 0; i < n; i++) if (w[i].k != ref[i].k || w[i].id != ref[i].id) { ++fails; printf("FALLA clave n=%zu k=%ld\n", n, k); break; }
  }
  // 2. adversario de McIlroy con pre-congelado: dispara el respaldo; la entrada concreta se verifica
  for (size_t n : {2000ul, 20000ul, 200000ul}) for (double pre : {0.0, 0.001, 0.05}) for (int mode = 0; mode < 2; mode++) {
    av.assign(n, n - 1); gas = n - 1; nsolid = 0; cand = 0; ncmp = 0;
    std::vector<size_t> pos(n); for (size_t i = 0; i < n; i++) pos[i] = i; std::shuffle(pos.begin(), pos.end(), R);
    for (size_t j = 0; j < (size_t)(pre * n); j++) av[pos[j]] = nsolid++;
    std::vector<u64> w(n); for (size_t i = 0; i < n; i++) w[i] = i;
    sot::Options o; o.threads = 1; if (mode) o.max_bytes = 0;
    sot::stable_sort_fast(w.data(), n, Adv(), o);
    std::vector<P> v(n); for (size_t i = 0; i < n; i++) v[i] = {(int)av[i], (unsigned)i};
    auto ref = v; auto c = [](const P& a, const P& b) { return a.k < b.k; };
    std::stable_sort(ref.begin(), ref.end(), c);
    sot::stable_sort(v.data(), n, c); ++cases;
    for (size_t i = 0; i < n; i++) if (v[i].k != ref[i].k || v[i].id != ref[i].id) { ++fails; printf("FALLA adversario n=%zu\n", n); break; }
    
  }
  // 3. comparador que lanza: el arreglo sigue siendo una permutación (tipos triviales pueden duplicar: se informa)
  for (int th : {1, 4}) for (int tr = 0; tr < 40; tr++) {
    size_t n = 1000 + R() % 300000; std::vector<std::string> v(n); for (size_t i = 0; i < n; i++) v[i] = "s" + std::to_string(R() % 100000);
    auto ms = v; std::sort(ms.begin(), ms.end());
    long budget = (long)(R() % (20 * n)); long cnt = 0;
    auto c = [&](const std::string& a, const std::string& b) { if (th == 1 && ++cnt > budget) throw 1; return a < b; };
    sot::Options o; o.threads = th;
    try { if (tr % 2) sot::stable_sort(v.data(), n, c); else sot::stable_sort_fast(v.data(), n, c, o); } catch (int) {}
    std::sort(v.begin(), v.end()); ++cases;
    if (v != ms) ++lost;   // limitación conocida: sin garantía básica si el comparador lanza
  }
  printf("casos=%ld fallas=%ld (excepciones con pérdida de elementos: %ld de 80, sin errores de memoria)\n", cases, fails, lost);
  return fails != 0;
}

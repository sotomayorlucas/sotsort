// Compilar con -I include -I bench/vmpowersort.
// Adversario perezoso de McIlroy contra pss::sort (samplesort estable por páginas) y
// contra SotSort sin páginas, con el presupuesto de VM-Powersort. Después se re-ejecuta la
// entrada obtenida con un tipo que cuenta movimientos y comparaciones.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
#include <algorithm>
static std::vector<long> av; static long gas, nsolid; static unsigned long long ncmp, MV;
struct E { std::uint32_t id; };
struct Lazy { bool operator()(const E& x, const E& y) const { ++ncmp;
  long& a = av[x.id]; long& b = av[y.id];
  if (a == gas && b == gas) a = nsolid++;            // congela x (McIlroy)
  return a < b; } };
struct W { long k; std::uint32_t id;
  W() = default; W(long a, std::uint32_t i) : k(a), id(i) {}
  W(const W& o) : k(o.k), id(o.id) { ++MV; } W& operator=(const W& o) { k = o.k; id = o.id; ++MV; return *this; }
  W(W&& o) : k(o.k), id(o.id) { ++MV; } W& operator=(W&& o) { k = o.k; id = o.id; ++MV; return *this; } };
struct Lt { bool operator()(const W& a, const W& b) const { ++ncmp; return a.k < b.k; } };
#include "paged_samplesort.h"
template <class T> std::size_t vm_bytes(std::size_t n) {
    const double target_p = std::sqrt((double)sizeof(T*) * 4 * n / (sizeof(T) * (std::log2((double)n) + 7)));
    const std::size_t log_n = 33 - __builtin_clz(n + 1), log_p = 33 - __builtin_clz((int)target_p + 1),
        co_log_p = log_n - log_p + 1, extra_pages = log_n + 4 + 3;
    return sizeof(T) * (extra_pages << log_p) + sizeof(T*) * (extra_pages + 1 + (4u << co_log_p)); }
template <class F> void measure(const char* name, std::size_t n, const std::vector<long>& vals, F run) {
    std::vector<W> v(n); for (std::size_t i = 0; i < n; ++i) v[i] = W(vals[i], (std::uint32_t)i);
    auto ref = v; std::stable_sort(ref.begin(), ref.end(), [](const W& a, const W& b) { return a.k < b.k; });
    MV = ncmp = 0; run(v);
    bool ok = true; for (std::size_t i = 0; i < n; ++i) ok &= v[i].id == ref[i].id;
    printf("  %-28s cmp %6.2f  mov %6.2f  %s\n", name, (double)ncmp / n, (double)MV / n, ok ? "ok" : "MAL");
}
int main() {
    for (std::size_t n : {100000u, 1000000u}) {
        const std::size_t bytesE = vm_bytes<W>(n);     // mismo presupuesto en bytes que el tipo contador
        // 1) adversario contra pss::sort
        av.assign(n, 0); gas = (long)n * 4; nsolid = 0; for (auto& x : av) x = gas;
        { std::vector<E> e(n); for (std::size_t i = 0; i < n; ++i) e[i].id = (std::uint32_t)i;
          ncmp = 0; pss::sort(e.data(), n, bytesE, 64, Lazy()); }
        printf("n=%zu  adversario contra paginado: %.2f comparaciones por elemento durante la construcción\n", n, (double)ncmp / n);
        std::vector<long> advp = av;
        // 2) adversario contra SotSort (mismo presupuesto, sin páginas)
        av.assign(n, gas); nsolid = 0;
        { std::vector<E> e(n); for (std::size_t i = 0; i < n; ++i) e[i].id = (std::uint32_t)i;
          ncmp = 0; sot::Options o; o.threads = 1; o.probe = false; o.shortcuts = false; o.max_bytes = bytesE;
          sot::stable_sort_fast(e.data(), n, Lazy(), o); }
        std::vector<long> advs = av;
        std::vector<long> rnd(n); { std::uint64_t x = 88172645463325252ull; for (auto& r : rnd) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; r = (long)(x >> 20); } }
        auto P = [&](std::vector<W>& v) { pss::sort(v.data(), n, vm_bytes<W>(n), 64, Lt()); };
        auto S = [&](std::vector<W>& v) { sot::Options o; o.threads = 1; o.probe = false; o.shortcuts = false; o.max_bytes = vm_bytes<W>(n); sot::stable_sort_fast(v.data(), n, Lt(), o); };
        measure("aleatoria / paginado", n, rnd, P);
        measure("aleatoria / SotSort", n, rnd, S);
        measure("adv. paginado / paginado", n, advp, P);
        measure("adv. paginado / paginado, otra semilla", n, advp, [&](std::vector<W>& v) { pss::sort(v.data(), n, vm_bytes<W>(n), 64, Lt(), 12345); });
        measure("adv. paginado / SotSort", n, advp, S);
        measure("adv. SotSort / paginado", n, advs, P);
        measure("adv. SotSort / SotSort", n, advs, S);
    }
}

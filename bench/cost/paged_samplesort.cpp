// Prototipo: samplesort estable por páginas (k vías). Ver docs/IDEA_particion_paginada.md §2d.
// Una pasada: cada elemento va a la página de salida de su grupo (k páginas de P en el buffer);
// una página llena se vuelca en la primera ranura libre y se anota su grupo. Después:
// permutación estable de las ranuras por grupo, y corrimiento para intercalar las parciales.
// Recursión por grupo; los rangos que entran en el buffer van al quicksort de SotSort.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

static unsigned long long MV = 0, CMP = 0;
struct W {
    int k; int id;
    W() = default;
    W(int x, int i) : k(x), id(i) {}
    W(const W& o) : k(o.k), id(o.id) { ++MV; }
    W& operator=(const W& o) { k = o.k; id = o.id; ++MV; return *this; }
    W(W&& o) : k(o.k), id(o.id) { ++MV; }
    W& operator=(W&& o) { k = o.k; id = o.id; ++MV; return *this; }
};
struct Lt { bool operator()(const W& a, const W& b) const { ++CMP; return a.k < b.k; } };
#include "../../include/sotsort.hpp"

struct Ctx { W* buf; std::size_t B, K, base_moves = 0; Lt lt; };

// Reparte a[0,m) en grupos 0..k-1 (grupo = cantidad de separadores <= x), estable.
// Devuelve los comienzos de cada grupo (k+1 valores).
std::vector<std::size_t> paged_distribute(W* a, std::size_t m, const std::vector<W>& spl, Ctx& c) {
    const std::size_t k = spl.size() + 1, P = c.B / k;
    W* buf = c.buf;
    std::vector<std::size_t> fill(k, 0), full(k, 0);
    std::vector<std::uint16_t> grp;                   // grupo de cada ranura (log2 k bits alcanzarían)
    grp.reserve(m / P + 1);
    std::size_t w = 0;
    for (std::size_t r = 0; r < m; ++r) {
        // grupo = upper_bound sobre los separadores (búsqueda binaria: ~log2 k comparaciones)
        std::size_t lo = 0, hi = spl.size();
        while (lo < hi) { std::size_t md = (lo + hi) / 2; if (c.lt(a[r], spl[md])) hi = md; else lo = md + 1; }
        const std::size_t g = lo;
        buf[g * P + fill[g]++] = std::move(a[r]);
        if (fill[g] == P) {
            std::move(buf + g * P, buf + (g + 1) * P, a + w * P);
            grp.push_back((std::uint16_t)g); ++full[g]; ++w; fill[g] = 0;
        }
    }
    // Fase 1: parciales al espacio libre [wP, m) (mide exactamente la suma de las parciales).
    std::size_t pos = w * P;
    std::vector<std::size_t> ppos(k);
    for (std::size_t g = 0; g < k; ++g) { ppos[g] = pos; std::move(buf + g * P, buf + g * P + fill[g], a + pos); pos += fill[g]; }
    // Fase 2: permutación estable de ranuras por grupo (por ciclos, una página en el buffer).
    std::vector<std::size_t> F(k + 1, 0);
    for (std::size_t g = 0; g < k; ++g) F[g + 1] = F[g] + full[g];
    std::vector<std::size_t> src(w);                 // src[destino] = ranura de origen
    { std::vector<std::size_t> nxt(F.begin(), F.end() - 1); for (std::size_t i = 0; i < w; ++i) src[nxt[grp[i]]++] = i; }
    std::vector<char> done(w, 0);
    for (std::size_t i = 0; i < w; ++i) {
        if (done[i]) continue;
        if (src[i] == i) { done[i] = 1; continue; }
        std::move(a + i * P, a + (i + 1) * P, buf);
        std::size_t hole = i;
        for (;;) {
            done[hole] = 1;
            std::size_t s2 = src[hole];
            if (s2 == i) { std::move(buf, buf + P, a + hole * P); break; }
            std::move(a + s2 * P, a + (s2 + 1) * P, a + hole * P);
            hole = s2;
        }
    }
    // Fase 3: intercalar parciales. Grupo g: páginas en [F_g P, F_{g+1} P), corrimiento D_g =
    // parciales de grupos < g. De derecha a izquierda: copiar parciales al buffer y correr.
    std::size_t tot = 0;
    for (std::size_t g = 0; g < k; ++g) tot += fill[g];
    std::move(a + w * P, a + w * P + tot, buf);       // las parciales, en orden de grupo
    std::vector<std::size_t> D(k + 1, 0);
    for (std::size_t g = 0; g < k; ++g) D[g + 1] = D[g] + fill[g];
    std::vector<std::size_t> start(k + 1);
    for (std::size_t g = k; g-- > 0;) {
        const std::size_t s = F[g] * P, e = F[g + 1] * P, d = D[g];
        std::move(buf + D[g], buf + D[g + 1], a + e + d);              // parcial del grupo g
        if (d && e > s) std::move_backward(a + s, a + e, a + e + d);   // páginas del grupo g
        start[g] = s + d;
    }
    start[k] = m;
    return start;
}

void sot_base(W* a, std::size_t m, Ctx& c) {
    sot::Sorter<W, Lt> s(a, c.lt);
    s.use_buffer(c.buf, c.B);
    s.quick_only(m);
}

void psort(W* a, std::size_t m, Ctx& c, std::mt19937_64& g) {
    if (m <= c.B) { sot_base(a, m, c); return; }
    std::size_t k = c.K;
    while (k > 2 && c.B / k < 64) k /= 2;
    // separadores: muestra de tamaño ~ 8k ordenada (copias: costo O(k log k), despreciable)
    std::vector<W> smp; const std::size_t ns = std::min<std::size_t>(m, 8 * k);
    for (std::size_t i = 0; i < ns; ++i) smp.push_back(a[g() % m]);
    std::sort(smp.begin(), smp.end(), c.lt);
    std::vector<W> spl;
    for (std::size_t j = 1; j < k; ++j) spl.push_back(smp[j * ns / k]);
    auto st = paged_distribute(a, m, spl, c);
    for (std::size_t j = 0; j < k; ++j) {
        std::size_t len = st[j + 1] - st[j];
        if (len == m) { sot_base(a, m, c); return; }  // sin progreso (muchos iguales): base
        if (len > 1) psort(a + st[j], len, c, g);
    }
}

int main(int argc, char** argv) {
    // Presupuesto en elementos: el mismo que reserva VM-Powersort para este tipo (vm_bytes del
    // harness, dividido por sizeof). Con argumentos: B para 10^6 y B para 10^7.
    auto vm_elems = [](std::size_t n) {
        const double target_p = std::sqrt((double)sizeof(W*) * 4 * n / (sizeof(W) * (std::log2((double)n) + 7)));
        const std::size_t log_n = 33 - __builtin_clz(n + 1), log_p = 33 - __builtin_clz((int)target_p + 1),
            co_log_p = log_n - log_p + 1, extra_pages = log_n + 4 + 3;
        std::size_t bytes = sizeof(W) * (extra_pages << log_p) + sizeof(W*) * (extra_pages + 1 + (4u << co_log_p));
        return bytes / sizeof(W);
    };
    for (std::size_t n : {1000000u, 10000000u}) for (int mod : {0, 1000}) {
        std::mt19937_64 g(7);
        std::vector<W> v(n); for (std::size_t i = 0; i < n; ++i) v[i] = W(mod ? (int)(g() % mod) : (int)(g() >> 33), (int)i);
        std::vector<W> ref = v;
        std::stable_sort(ref.begin(), ref.end(), [](const W& a, const W& b) { return a.k < b.k; });
        std::size_t Bn = argc > 2 ? (std::size_t)atol(argv[n == 10000000u ? 2 : 1]) : vm_elems(n);
        std::vector<W> buf(Bn);
        // base: SotSort con el mismo buffer
        auto v0 = v; MV = CMP = 0;
        { sot::Sorter<W, Lt> s(v0.data(), Lt()); s.use_buffer(buf.data(), Bn); s.quick_only(n); }
        double bm = (double)MV / n, bc = (double)CMP / n;
        for (std::size_t K : {8u, 16u, 32u, 64u}) {
            auto v1 = v; Ctx c{buf.data(), Bn, K};
            std::mt19937_64 gg(1); MV = CMP = 0; psort(v1.data(), n, c, gg);
            bool ok = true; for (std::size_t i = 0; i < n; ++i) ok &= v1[i].id == ref[i].id;
            printf("n=%-9zu %s B=%-7zu k=%-3zu | base: mov %.2f cmp %.2f | paginado: mov %.2f cmp %.2f | %s\n",
                   n, mod ? "dup1000" : "random ", Bn, K, bm, bc, (double)MV / n, (double)CMP / n, ok ? "ok" : "MAL");
        }
    }
}

// SotSort + samplesort estable por páginas (experimental). Agregado para la comparación.
// Ruteo: si SotSort detecta estructura (pocos giros) usa su camino normal; si no, reparte
// en k grupos por páginas (ver docs/IDEA_particion_paginada.md §2d del repo sotsort) y
// ordena los rangos que entran en el buffer con el quicksort de SotSort.
// Memoria: todo sale de `bytes` (buffer + tablas + muestra), así que se puede pedir lo mismo
// que reserva VM-Powersort.
#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <random>
#include <vector>
#include "sotsort.hpp"

namespace pss {

template <class T, class Lt>
struct Ctx { T* buf; std::size_t B, K; Lt lt; std::mt19937_64 rng{1}; int max_depth = 64; };

template <class T, class Lt>
std::size_t ways(const Ctx<T, Lt>& c) {
    std::size_t k = c.K;
    while (k > 2 && c.B / k < 64) k /= 2;
    return k;
}

template <class T, class Lt>
void base(T* a, std::size_t m, Ctx<T, Lt>& c) {
    sot::Sorter<T, Lt> s(a, c.lt);
    s.use_buffer(c.buf, c.B);
    s.quick_only(m);
}

template <class T, class Lt>
void distribute(T* a, std::size_t m, const T* spl, std::size_t k, Ctx<T, Lt>& c, std::size_t* start,
                std::uint16_t* grp, std::uint32_t* src) {
    const std::size_t P = c.B / k;
    T* buf = c.buf;
    std::size_t fill[256] = {0}, full[256] = {0};
    std::size_t w = 0;
    for (std::size_t r = 0; r < m; ++r) {
        std::size_t lo = 0, hi = k - 1;
        while (lo < hi) { std::size_t md = (lo + hi) / 2; if (c.lt(a[r], spl[md])) hi = md; else lo = md + 1; }
        const std::size_t g = lo;
        buf[g * P + fill[g]++] = std::move(a[r]);
        if (fill[g] == P) {
            std::move(buf + g * P, buf + (g + 1) * P, a + w * P);
            grp[w] = (std::uint16_t)g; ++full[g]; ++w; fill[g] = 0;
        }
    }
    std::size_t pos = w * P;
    for (std::size_t g = 0; g < k; ++g) { std::move(buf + g * P, buf + g * P + fill[g], a + pos); pos += fill[g]; }
    std::size_t F[257]; F[0] = 0;
    for (std::size_t g = 0; g < k; ++g) F[g + 1] = F[g] + full[g];
    { std::size_t nxt[256]; for (std::size_t g = 0; g < k; ++g) nxt[g] = F[g];
      for (std::size_t i = 0; i < w; ++i) src[nxt[grp[i]]++] = (std::uint32_t)i; }
    // grp ya no hace falta: se reusa como marca de "hecho" (0xFFFF)
    for (std::size_t i = 0; i < w; ++i) {
        if (grp[i] == 0xFFFF) continue;
        if (src[i] == i) { grp[i] = 0xFFFF; continue; }
        std::move(a + i * P, a + (i + 1) * P, buf);
        std::size_t hole = i;
        for (;;) {
            grp[hole] = 0xFFFF;
            std::size_t s2 = src[hole];
            if (s2 == i) { std::move(buf, buf + P, a + hole * P); break; }
            std::move(a + s2 * P, a + (s2 + 1) * P, a + hole * P);
            hole = s2;
        }
    }
    std::size_t tot = 0, D[257]; D[0] = 0;
    for (std::size_t g = 0; g < k; ++g) { tot += fill[g]; D[g + 1] = D[g] + fill[g]; }
    std::move(a + w * P, a + w * P + tot, buf);
    for (std::size_t g = k; g-- > 0;) {
        const std::size_t s = F[g] * P, e = F[g + 1] * P, d = D[g];
        std::move(buf + D[g], buf + D[g + 1], a + e + d);
        if (d && e > s) std::move_backward(a + s, a + e, a + e + d);
        start[g] = s + d;
    }
    start[k] = m;
}

template <class T, class Lt>
void psort(T* a, std::size_t m, Ctx<T, Lt>& c, T* spl, T* smp, std::uint16_t* grp, std::uint32_t* src, int depth = 0, int bad = 0) {
    const std::size_t k = ways(c);
    // Presupuesto de pasadas: con muestras malas (o armadas) se cae al quicksort de SotSort,
    // que tiene su propio respaldo O(n log n). 2·log_k(n) + 2 pasadas como máximo.
    if (m <= c.B || k < 2 || depth > c.max_depth || bad > 1) { base(a, m, c); return; }
    const std::size_t ns = std::min<std::size_t>(m, 8 * k);
    for (std::size_t i = 0; i < ns; ++i) smp[i] = a[c.rng() % m];
    std::sort(smp, smp + ns, c.lt);
    for (std::size_t j = 1; j < k; ++j) spl[j - 1] = smp[j * ns / k];
    std::size_t start[257];
    distribute(a, m, spl, k, c, start, grp, src);
    // Pasada mala: el grupo más grande se queda con más de la mitad. Dos malas seguidas en un
    // camino de la recursión -> caso base (quicksort de SotSort, con su propio respaldo).
    std::size_t big = 0;
    for (std::size_t j = 0; j < k; ++j) big = std::max(big, start[j + 1] - start[j]);
    const int nbad = big > m / 2 ? bad + 1 : 0;
    for (std::size_t j = 0; j < k; ++j) {
        std::size_t len = start[j + 1] - start[j];
        if (len == m) { base(a, m, c); return; }      // sin progreso (muchos iguales)
        if (len > 1) psort(a + start[j], len, c, spl, smp, grp, src, depth + 1, len > m / 2 ? nbad : 0);
    }
}

// bytes: memoria total permitida (buffer + tablas + muestra).
template <class T, class Lt = std::less<T>>
void sort(T* a, std::size_t n, std::size_t bytes, std::size_t K = 64, Lt lt = Lt(), std::uint64_t seed = 1) {
    if (n < 2) return;
    {   // Ruteo de SotSort: con estructura, su camino normal con el mismo presupuesto.
        sot::Sorter<T, Lt> probe(a, lt);
        if (probe.has_few_turns(n)) {
            sot::Options o; o.threads = 1; o.probe = false; o.shortcuts = false; o.max_bytes = bytes;
            sot::stable_sort_fast(a, n, lt, o);
            return;
        }
    }
    // Reparto del presupuesto: tablas (grupo + origen por ranura), muestra y separadores
    // salen de `bytes`; el resto es el buffer de páginas.
    std::size_t B = bytes / sizeof(T);
    for (int it = 0; it < 3; ++it) {
        std::size_t k = K; while (k > 2 && B / k < 64) k /= 2;
        std::size_t P = std::max<std::size_t>(1, B / k);
        std::size_t slots = n / P + 2;
        std::size_t extra = slots * (sizeof(std::uint16_t) + sizeof(std::uint32_t)) + (9 * k) * sizeof(T)
                          + sizeof(sot::Sorter<T, Lt>);   // el Sorter de los casos base (buffer propio incluido)
        B = bytes > extra ? (bytes - extra) / sizeof(T) : 0;
    }
    if (B < 256) {                                    // presupuesto mínimo: SotSort tal cual
        sot::Options o; o.threads = 1; o.probe = false; o.shortcuts = false; o.max_bytes = bytes;
        sot::stable_sort_fast(a, n, lt, o);
        return;
    }
    Ctx<T, Lt> c{nullptr, B, K, lt};
    std::size_t k = ways(c), P = B / k, slots = n / P + 2;
    std::unique_ptr<T[]> buf(new T[B]), smp(new T[8 * k]), spl(new T[k]);
    std::unique_ptr<std::uint16_t[]> grp(new std::uint16_t[slots]);
    std::unique_ptr<std::uint32_t[]> src(new std::uint32_t[slots]);
    c.buf = buf.get();
    c.rng.seed(seed);
    { int lk = 0; for (std::size_t x = k; x > 1; x >>= 1) ++lk; int ln = 0; for (std::size_t x = n; x > 1; x >>= 1) ++ln;
      c.max_depth = 2 * (ln + lk - 1) / lk + 2; }
    psort(a, n, c, spl.get(), smp.get(), grp.get(), src.get());
}

}  // namespace pss

// Prototipo: partición estable paginada (ver docs/IDEA_particion_paginada.md).
// Cuenta movimientos (asignaciones + construcciones) y verifica estabilidad contra
// std::stable_partition. Compara con stable_partition de SotSort con el mismo buffer.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

static unsigned long long MV = 0, CMP = 0;
struct W {
    long k; long id;
    W() = default;
    W(long x, long i) : k(x), id(i) {}
    W(const W& o) : k(o.k), id(o.id) { ++MV; }
    W& operator=(const W& o) { k = o.k; id = o.id; ++MV; return *this; }
    W(W&& o) : k(o.k), id(o.id) { ++MV; }
    W& operator=(W&& o) { k = o.k; id = o.id; ++MV; return *this; }
};
struct Lt { bool operator()(const W& a, const W& b) const { ++CMP; return a.k < b.k; } };

#define private public
#include "../../include/sotsort.hpp"
#undef private

// Particiona a[0, m) en [pred | !pred] estable. buf: 2P elementos.
// Devuelve el número de elementos que cumplen pred.
template <class T, class Pred>
std::size_t paged_partition(T* a, std::size_t m, T* buf, std::size_t P, Pred pred) {
    if (m <= 2 * P) {                                   // cabe en el buffer: partición directa
        std::size_t nl = 0, nr = 0;
        T* R = buf;
        for (std::size_t i = 0; i < m; ++i) { if (pred(a[i])) { if (nl != i) a[nl] = std::move(a[i]); ++nl; } else R[nr++] = std::move(a[i]); }
        std::move(R, R + nr, a + nl);
        return nl;
    }
    T* LB = buf; T* RB = buf + P;
    std::size_t nl = 0, nr = 0;                         // ocupación de las páginas de salida
    std::size_t w = 0;                                  // próxima ranura libre (= páginas volcadas)
    std::vector<std::uint64_t> side((m / P + 64) / 64, 0);   // bit 1 = página del lado derecho
    for (std::size_t r = 0; r < m; ++r) {
        if (pred(a[r])) {
            LB[nl++] = std::move(a[r]);
            if (nl == P) { std::move(LB, LB + P, a + w * P); ++w; nl = 0; }
        } else {
            RB[nr++] = std::move(a[r]);
            if (nr == P) { std::move(RB, RB + P, a + w * P); side[w / 64] |= 1ull << (w % 64); ++w; nr = 0; }
        }
    }
    // Fase 1: las parciales van al espacio libre [wP, m), que mide exactamente nl + nr.
    std::move(LB, LB + nl, a + w * P);
    std::move(RB, RB + nr, a + w * P + nl);
    // Fase 2: permutación estable de las w páginas: izquierdas al frente, derechas detrás.
    auto bit = [&](std::size_t i) { return (side[i / 64] >> (i % 64)) & 1; };
    std::vector<std::size_t> pre(side.size() + 1, 0);   // derechas antes de cada palabra
    for (std::size_t j = 0; j < side.size(); ++j) pre[j + 1] = pre[j] + __builtin_popcountll(side[j]);
    auto rights_before = [&](std::size_t i) {
        std::uint64_t mask = (i % 64) ? (side[i / 64] & ((1ull << (i % 64)) - 1)) : 0;
        return pre[i / 64] + __builtin_popcountll(mask);
    };
    const std::size_t b = rights_before(w), aL = w - b;
    auto dest = [&](std::size_t i) { std::size_t rb = rights_before(i); return bit(i) ? aL + rb : i - rb; };
    // Origen de la ranura j (inversa de dest): la j-ésima izquierda o la (j-aL)-ésima derecha.
    auto select = [&](std::size_t k, bool right) {      // índice de la k-ésima página del lado dado
        std::size_t lo = 0, hi = side.size();           // última palabra con cuenta previa <= k
        auto cnt = [&](std::size_t j) { return right ? pre[j] : j * 64 - pre[j]; };
        while (hi - lo > 1) { std::size_t md = (lo + hi) / 2; if (cnt(md) <= k) lo = md; else hi = md; }
        std::size_t r = k - cnt(lo);
        std::uint64_t word = right ? side[lo] : ~side[lo];
        for (;;) { std::size_t z = __builtin_ctzll(word); if (!r) return lo * 64 + z; word &= word - 1; --r; }
    };
    auto src = [&](std::size_t j) { return j < aL ? select(j, false) : select(j - aL, true); };
    std::vector<std::uint64_t> done((w + 64) / 64, 0);
    for (std::size_t i = 0; i < w; ++i) {
        if ((done[i / 64] >> (i % 64)) & 1) continue;
        if (dest(i) == i) { done[i / 64] |= 1ull << (i % 64); continue; }
        std::move(a + i * P, a + (i + 1) * P, buf);     // hueco en i; seguir el ciclo hacia atrás
        std::size_t hole = i;
        for (;;) {
            done[hole / 64] |= 1ull << (hole % 64);
            std::size_t s2 = src(hole);
            if (s2 == i) { std::move(buf, buf + P, a + hole * P); break; }
            std::move(a + s2 * P, a + (s2 + 1) * P, a + hole * P);
            hole = s2;
        }
    }
    // Fase 3: [L (aL·P) | R (b·P) | nl | nr] -> [L | nl | R | nr]: la parcial derecha ya está
    // en su lugar (son los últimos derechos); la izquierda se rota con las páginas derechas.
    const std::size_t s = aL * P, e = w * P, k = nl;
    std::move(a + e, a + e + k, buf);
    std::move_backward(a + s, a + e, a + e + k);
    std::move(buf, buf + k, a + s);
    return s + nl;
}

int main() {
    std::mt19937_64 g(1);
    printf("%8s %8s %6s | %10s %10s | %s\n", "m", "B", "m/B", "SotSort", "paginada", "estable");
    for (std::size_t m : {1u << 16, 1u << 20, 1u << 22, 3000017u}) for (std::size_t B : {2048u, 8192u, 65536u}) {
        if (B > m) continue;
        std::vector<W> a(m); for (std::size_t i = 0; i < m; ++i) a[i] = W((long)(g() >> 1), (long)i);
        std::vector<long> t; for (auto& x : a) t.push_back(x.k);
        std::nth_element(t.begin(), t.begin() + m / 2, t.end());
        W piv(t[m / 2], -1);
        std::vector<W> ref = a, b1 = a, b2 = a, buf(B);
        std::stable_partition(ref.begin(), ref.end(), [&](const W& x) { return x.k < piv.k; });
        sot::Sorter<W, Lt> s(b1.data(), Lt()); s.use_buffer(buf.data(), B);
        MV = 0; s.stable_partition(0, m, piv, false); double m1 = (double)MV / m;
        MV = 0; paged_partition(b2.data(), m, buf.data(), B / 2, [&](const W& x) { return x.k < piv.k; }); double m2 = (double)MV / m;
        bool ok1 = true, ok2 = true;
        for (std::size_t i = 0; i < m; ++i) { ok1 &= b1[i].id == ref[i].id; ok2 &= b2[i].id == ref[i].id; }
        printf("%8zu %8zu %6.0f | %10.2f %10.2f | %s %s\n", m, B, (double)m / B, m1, m2, ok1 ? "ok" : "MAL", ok2 ? "ok" : "MAL");
    }
}

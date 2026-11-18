// sq3.hpp — v3 del híbrido: quicksort estable in-place (rotaciones)
// + merge con buffer fijo de B elementos en el stack. Sin recursión, sin heap.
//
// Ideas:
//  - Capa quicksort: partición ESTABLE con buffer fijo (bloques de B se parten
//    en O(B) usando el buffer; los bloques ya partidos se juntan bottom-up con
//    rotaciones y búsqueda binaria del punto de corte -> memoria O(1)).
//  - Truco pdqsort para claves repetidas: si el pivote es el mínimo del rango,
//    se parte por "<= pivote" y ese bloque igual queda terminado.
//  - Detección de rangos ya ordenados / estrictamente descendentes (O(n)).
//  - Fallback a merge sort si las particiones salen desbalanceadas (introsort).
//  - Merge branchless con buffer; rotación Gries-Mills (swap de bloques) en vez
//    de triple reversión cuando ningún lado entra en el buffer.
//
// Requisitos: T movible y default-constructible (por el buffer T[B]).
// Complejidad: comparaciones O(n log n) esperadas; movimientos O(n log^2 n)
// en el peor caso (igual que cualquier sort estable basado en rotaciones).
#pragma once
#ifndef SQ_NINTHER
#define SQ_NINTHER 2048
#endif
#ifndef SQ_LEAF
#define SQ_LEAF 256
#endif
#include <cstddef>
#include <utility>
#include <algorithm>
#include <functional>
#include <type_traits>
#include <cmath>

namespace sq5 {

template <class T, class Cmp = std::less<T>, std::size_t B = 512>
class Sorter {
public:
    static constexpr std::size_t LEAF = SQ_LEAF;   // rangos <= LEAF van a la hoja
    static constexpr std::size_t RUN = 24;       // insertion sort
    static constexpr int STACK = 96;

    Sorter(T* a, Cmp c) : a_(a), c_(c) {}

    void sort(std::size_t n) {
        if (n < 2) return;
        if (presorted(0, n)) return;
        if (n > 2 && looks_merge_friendly(n)) {
            adaptive_ = true;            // datos con estructura: merges con branch (predicción acierta)
            reverse_runs(0, n);
            merge_sort(0, n);
            return;
        }
        quick(n);
    }

private:
    T* a_;
    Cmp c_;
    T buf_[B];
    bool adaptive_ = false;

    bool lt(const T& x, const T& y) { return c_(x, y); }

    // ------------------------------------------------------------ primitivas
    std::size_t lower_bound(std::size_t f, std::size_t l, const T& v) {
        while (f < l) { std::size_t m = f + ((l - f) >> 1); if (lt(a_[m], v)) f = m + 1; else l = m; }
        return f;
    }
    std::size_t upper_bound(std::size_t f, std::size_t l, const T& v) {
        while (f < l) { std::size_t m = f + ((l - f) >> 1); if (lt(v, a_[m])) l = m; else f = m + 1; }
        return f;
    }

    // Ya ordenado -> true. Estrictamente descendente -> se invierte (estable) y true.
    bool presorted(std::size_t lo, std::size_t hi) {
        std::size_t i = lo + 1;
        while (i < hi && !lt(a_[i], a_[i - 1])) ++i;
        if (i == hi) return true;
        if (i == lo + 1) {
            while (i < hi && lt(a_[i], a_[i - 1])) ++i;
            if (i == hi) { std::reverse(a_ + lo, a_ + hi); return true; }
        }
        return false;
    }

    // Heurística O(n), 1 comparación/elem: cuenta "giros" (cambios entre tramo
    // ascendente y descendente). Pocos giros = runs largos (ordenado, sierra,
    // organ pipe, casi ordenado) -> el merge adaptativo gana al quicksort.
    bool looks_merge_friendly(std::size_t n) {
        std::size_t turns = 0;
        bool prev = lt(a_[1], a_[0]);
        for (std::size_t i = 2; i < n; ++i) {
            bool d = lt(a_[i], a_[i - 1]);
            turns += d != prev;
            prev = d;
        }
        return turns * 8 < n;
    }

    // Invierte runs ESTRICTAMENTE descendentes (invertirlos no rompe estabilidad).
    void reverse_runs(std::size_t lo, std::size_t hi) {
        std::size_t i = lo;
        while (i + 1 < hi) {
            if (lt(a_[i + 1], a_[i])) {
                std::size_t j = i + 1;
                while (j + 1 < hi && lt(a_[j + 1], a_[j])) ++j;
                if (j - i + 1 >= 8) std::reverse(a_ + i, a_ + j + 1);
                i = j + 1;
            } else {
                ++i;
            }
        }
    }

    void insertion(std::size_t lo, std::size_t hi) {
        for (std::size_t i = lo + 1; i < hi; ++i) {
            if (!lt(a_[i], a_[i - 1])) continue;          // comparar ANTES de mover
            T v = std::move(a_[i]);
            std::size_t j = i;
            do { a_[j] = std::move(a_[j - 1]); --j; } while (j > lo && lt(v, a_[j - 1]));
            a_[j] = std::move(v);
        }
    }

    // ----------------------------------------------------------- rotación
    void rotate(std::size_t l, std::size_t m, std::size_t r) {
        for (;;) {
            std::size_t nl = m - l, nr = r - m;
            if (!nl || !nr) return;
            if (nl <= B) {
                std::move(a_ + l, a_ + m, buf_);
                std::move(a_ + m, a_ + r, a_ + l);
                std::move(buf_, buf_ + nl, a_ + l + nr);
                return;
            }
            if (nr <= B) {
                std::move(a_ + m, a_ + r, buf_);
                std::move_backward(a_ + l, a_ + m, a_ + r);
                std::move(buf_, buf_ + nr, a_ + l);
                return;
            }
            // Puente: si la diferencia de tamaños entra en el buffer, se guarda solo
            // la diferencia y se rota en UNA pasada con dos flujos secuenciales
            // (~1 escritura por elemento en vez de ~1.5 de Gries-Mills).
            if (nl > nr && nl - nr <= B) {
                // [A1 (nr) | A2 (d) | Bb (nr)] -> [Bb | A1 | A2]; A2 = puente
                std::size_t d = nl - nr;
                T* A = a_ + l;
                std::move(A + nr, A + nl, buf_);
                for (std::size_t i = 0; i < nr; ++i) {      // ascendente
                    A[nr + i] = std::move(A[i]);            // A1[i] -> hueco
                    A[i] = std::move(A[nr + d + i]);        // Bb[i] -> su lugar
                }
                std::move(buf_, buf_ + d, A + 2 * nr);
                return;
            }
            if (nr > nl && nr - nl <= B) {
                // [A (nl) | B1 (d) | B2 (nl)] -> [B1 | B2 | A]; B1 = puente
                std::size_t d = nr - nl;
                T* A = a_ + l;
                std::move(A + nl, A + nl + d, buf_);
                for (std::size_t i = nl; i-- > 0; ) {       // descendente
                    A[d + i] = std::move(A[nl + d + i]);    // B2[i] -> su lugar
                    A[nl + d + i] = std::move(A[i]);        // A[i] -> lugar liberado
                }
                std::move(buf_, buf_ + d, A);
                return;
            }
            fused_reversal_rotate(a_ + l, a_ + m, a_ + r);
            return;
        }
    }

    // Triple reversión FUSIONADA en una pasada (idea de la "trinity rotation"
    // de scandum, dominio público): rev(A), rev(B) y rev(AB) se hacen a la vez
    // con 4 punteros y ciclos de 4 elementos -> ~1 escritura por elemento.
    static void fused_reversal_rotate(T* first, T* mid, T* last) {
        T *pa = first, *pb = mid, *pc = mid, *pd = last;
        std::size_t left = mid - first, right = last - mid;
        if (left == right) { std::swap_ranges(first, mid, mid); return; }
        for (std::size_t k = std::min(left, right) / 2; k; --k) {
            T t = std::move(*--pb);
            *pb = std::move(*pa);
            *pa++ = std::move(*pc);
            *pc++ = std::move(*--pd);
            *pd = std::move(t);
        }
        if (left < right) {                       // A agotado: queda reversión de B fusionada
            for (std::size_t k = (pd - pc) / 2; k; --k) {
                T t = std::move(*pc);
                *pc++ = std::move(*--pd);
                *pd = std::move(*pa);
                *pa++ = std::move(t);
            }
        } else {                                  // B agotado
            for (std::size_t k = (pb - pa) / 2; k; --k) {
                T t = std::move(*--pb);
                *pb = std::move(*pa);
                *pa++ = std::move(*--pd);
                *pd = std::move(t);
            }
        }
        for (std::size_t k = (pd - pa) / 2; k; --k) {  // reversión final del centro
            T t = std::move(*pa);
            *pa++ = std::move(*--pd);
            *pd = std::move(t);
        }
    }

    // ------------------------------------------------- merges con buffer
    // Precondición: (m-l) <= B. Fusión hacia adelante, branchless.
    void merge_left_buf(std::size_t l, std::size_t m, std::size_t r) {
        std::size_t nl = m - l;
        std::move(a_ + l, a_ + m, buf_);
        T *p = buf_, *pe = buf_ + nl, *q = a_ + m, *qe = a_ + r, *o = a_ + l;
        if (adaptive_) {
            while (p < pe && q < qe) *o++ = lt(*q, *p) ? std::move(*q++) : std::move(*p++);
        }
        while (p < pe && q < qe) {
            bool t = lt(*q, *p);                // derecha estrictamente menor
            *o++ = std::move(t ? *q : *p);
            q += t; p += !t;
        }
        std::move(p, pe, o);
    }
    // Precondición: (r-m) <= B. Fusión hacia atrás, branchless.
    void merge_right_buf(std::size_t l, std::size_t m, std::size_t r) {
        std::size_t nr = r - m;
        std::move(a_ + m, a_ + r, buf_);
        T *i = a_ + m, *il = a_ + l, *j = buf_ + nr, *o = a_ + r;
        if (adaptive_) {
            while (i > il && j > buf_) *--o = lt(*(j - 1), *(i - 1)) ? std::move(*--i) : std::move(*--j);
        }
        while (i > il && j > buf_) {
            bool t = lt(*(j - 1), *(i - 1));    // derecha < izquierda -> va la izquierda
            *--o = std::move(t ? *(i - 1) : *(j - 1));
            i -= t; j -= !t;
        }
        std::move_backward(buf_, j, o);
    }

    // Merge in-place general (SymMerge iterativo) con hojas en buffer.
    void merge(std::size_t l, std::size_t m, std::size_t r) {
        struct W { std::size_t l, m, r; } st[STACK];
        int sp = 0;
        st[sp++] = {l, m, r};
        while (sp) {
            W w = st[--sp];
            l = w.l; m = w.m; r = w.r;
            if (l >= m || m >= r || !lt(a_[m], a_[m - 1])) continue;
            l = upper_bound(l, m, a_[m]);
            r = lower_bound(m, r, a_[m - 1]);
            std::size_t nl = m - l, nr = r - m;
            if (nl <= nr ? nl <= B : false) { merge_left_buf(l, m, r); continue; }
            if (nr <= B) { merge_right_buf(l, m, r); continue; }
            if (nl <= B) { merge_left_buf(l, m, r); continue; }
            std::size_t m1, m2;
            if (nl >= nr) { m1 = l + (nl >> 1); m2 = lower_bound(m, r, a_[m1]); }
            else          { m2 = m + (nr >> 1); m1 = upper_bound(l, m, a_[m2]); }
            rotate(m1, m, m2);
            std::size_t nm = m1 + (m2 - m);
            // el más grande abajo: la pila crece O(log n)
            if ((nm - l) > (r - nm)) { st[sp++] = {l, m1, nm}; st[sp++] = {nm, m2, r}; }
            else                     { st[sp++] = {nm, m2, r}; st[sp++] = {l, m1, nm}; }
        }
    }

    // ---------------------------------------------------------- hojas
    // Merge src[0..nl) + src[nl..nl+nr) -> dst, branchless con chequeo de bordes.
    void merge_to(T* x, std::size_t nl, T* y, std::size_t nr, T* d) {
        T *xe = x + nl, *ye = y + nr;
        while (x < xe && y < ye) {
            bool t = lt(*y, *x);
            *d++ = std::move(t ? *y : *x);
            y += t; x += !t;
        }
        d = std::move(x, xe, d);
        std::move(y, ye, d);
    }
    // Parity merge (idea de quadsort): runs de igual largo n, se fusiona desde
    // las dos puntas a la vez, n iteraciones exactas, sin chequeo de bordes.
    // Solo para tipos trivialmente copiables (lee posiciones ya copiadas).
    void parity_merge(const T* l, const T* r, std::size_t n, T* d) {
        const T *pl = l, *pr = r, *tl = l + n - 1, *tr = r + n - 1;
        T *dh = d, *dt = d + 2 * n - 1;
        for (std::size_t k = 0; k < n; ++k) {
            bool x = lt(*pr, *pl);
            *dh++ = x ? *pr : *pl; pr += x; pl += !x;
            bool y = lt(*tr, *tl);
            *dt-- = y ? *tl : *tr; tl -= y; tr -= !y;
        }
    }
    // Parity merge con largo fijo W: el compilador lo desenrolla entero.
    template <std::size_t W>
    void parity_fixed(const T* l, const T* r, T* d) {
        const T *pl = l, *pr = r, *tl = l + W - 1, *tr = r + W - 1;
        T *dh = d, *dt = d + 2 * W - 1;
        for (std::size_t k = 0; k < W; ++k) {
            bool x = lt(*pr, *pl);
            *dh++ = x ? *pr : *pl; pr += x; pl += !x;
            bool y = lt(*tr, *tl);
            *dt-- = y ? *tl : *tr; tl -= y; tr -= !y;
        }
    }
    // Un nivel completo de merges de ancho w de src a dst.
    template <std::size_t W>
    void level_fixed(T* src, T* dst, std::size_t n) {
        std::size_t i = 0;
        for (; i + 2 * W <= n; i += 2 * W) parity_fixed<W>(src + i, src + i + W, dst + i);
        if (i + W < n) merge_to(src + i, W, src + i + W, n - i - W, dst + i);
        else std::copy(src + i, src + n, dst + i);
    }
    void level_any(T* src, T* dst, std::size_t n, std::size_t w) {
        if constexpr (std::is_trivially_copyable_v<T>) {
            switch (w) {
                case 2:  level_fixed<2>(src, dst, n);  return;
                case 4:  level_fixed<4>(src, dst, n);  return;
                case 8:  level_fixed<8>(src, dst, n);  return;
                case 16: level_fixed<16>(src, dst, n); return;
                default: break;
            }
            std::size_t i = 0;
            for (; i + 2 * w <= n; i += 2 * w) parity_merge(src + i, src + i + w, w, dst + i);
            if (i + w < n) merge_to(src + i, w, src + i + w, n - i - w, dst + i);
            else std::copy(src + i, src + n, dst + i);
        } else {
            std::size_t i = 0;
            for (; i + 2 * w <= n; i += 2 * w) merge_to(src + i, w, src + i + w, w, dst + i);
            if (i + w < n) merge_to(src + i, w, src + i + w, n - i - w, dst + i);
            else std::move(src + i, src + n, dst + i);
        }
    }
    // Ordena p[0..n), n <= B, alternando array <-> buffer (1 movimiento por nivel).
    // El primer paso (pares) se hace fuera de lugar si la cantidad de niveles es
    // impar, así el resultado termina en p sin copia final.
    void ping_pong(T* p, std::size_t n) {
        if (n < 2) return;
        std::size_t L = 0;
        for (std::size_t w = 2; w < n; w <<= 1) ++L;
        T *src, *dst;
        if constexpr (std::is_trivially_copyable_v<T>) {
            T* out = (L & 1) ? buf_ : p;
            std::size_t i = 0;
            for (; i + 1 < n; i += 2) {
                T x = p[i], y = p[i + 1];
                bool s = lt(y, x);
                out[i] = s ? y : x; out[i + 1] = s ? x : y;
            }
            if (i < n) out[i] = p[i];
            src = out; dst = (out == p) ? buf_ : p;
        } else {
            for (std::size_t i = 0; i + 1 < n; i += 2)
                if (lt(p[i + 1], p[i])) std::swap(p[i], p[i + 1]);
            src = p; dst = buf_;
        }
        for (std::size_t w = 2; w < n; w <<= 1) { level_any(src, dst, n, w); std::swap(src, dst); }
        if (src != p) std::move(src, src + n, p);
    }
    // Hoja: n <= 2B. Dos mitades por ping-pong y un merge final con buffer.
    void leaf_sort(std::size_t lo, std::size_t hi) {
        std::size_t n = hi - lo;
        if (n <= B) { ping_pong(a_ + lo, n); return; }
        std::size_t h = n >> 1;
        ping_pong(a_ + lo, h);
        ping_pong(a_ + lo + h, n - h);
        if (!lt(a_[lo + h], a_[lo + h - 1])) return;
        merge_left_buf(lo, lo + h, hi);
    }

    void merge_sort(std::size_t lo, std::size_t hi) {
        for (std::size_t s = lo; s < hi; s += RUN) insertion(s, std::min(s + RUN, hi));
        for (std::size_t w = RUN; w < hi - lo; w <<= 1)
            for (std::size_t l = lo; l + w < hi; l += 2 * w)
                merge(l, l + w, std::min(l + 2 * w, hi));
    }

    // -------------------------------------------- partición estable O(1)
    // Deja [lo,hi) como [pred=true | pred=false] preservando el orden relativo.
    template <class P>
    std::size_t part_block(std::size_t lo, std::size_t hi, P pred) {   // hi-lo <= B
        if constexpr (std::is_trivially_copyable_v<T>) {
            // Branchless: cada elemento se escribe en ambos destinos y solo
            // avanza el índice que corresponde (sin saltos mal predichos).
            T* p = a_ + lo;
            std::size_t len = hi - lo, m = 0;
            for (std::size_t i = 0; i < len; ++i) {
                T x = p[i];
                bool t = pred(x);
                buf_[i - m] = x;
                p[m] = x;
                m += t;
            }
            std::copy(buf_, buf_ + (len - m), p + m);
            return lo + m;
        }
        std::size_t w = lo, k = 0;
        for (std::size_t i = lo; i < hi; ++i) {
            bool t = pred(a_[i]);
            if (t) { if (w != i) a_[w] = std::move(a_[i]); ++w; } else buf_[k++] = std::move(a_[i]);
        }
        std::move(buf_, buf_ + k, a_ + w);
        return w;
    }
    template <class P>
    std::size_t split_point(std::size_t f, std::size_t l, P pred) {     // primer false
        while (f < l) { std::size_t m = f + ((l - f) >> 1); if (pred(a_[m])) f = m + 1; else l = m; }
        return f;
    }
    // Gather en profundidad (orden "contador binario"): cada bloque partido se
    // fusiona apenas su vecino izquierdo tiene el mismo tamaño. Los merges chicos
    // ocurren sobre datos recién tocados (en caché) y el punto de corte de cada
    // segmento viaja en la pila: cero búsquedas binarias. Pila <= 64 entradas.
    template <class P>
    std::size_t stable_partition(std::size_t lo, std::size_t hi, P pred) {
        struct S { std::size_t s, sp; int lvl; } st[64];
        int top = 0;
        auto fuse = [&]() {                       // fusiona los dos de arriba
            S& L = st[top - 2]; S& R = st[top - 1];
            std::size_t mid = R.s;
            rotate(L.sp, mid, R.sp);
            L.sp = L.sp + (R.sp - mid);
            L.lvl = std::max(L.lvl, R.lvl) + 1;
            --top;
        };
        for (std::size_t s = lo; s < hi; s += B) {
            std::size_t e = std::min(s + B, hi);
            st[top++] = {s, part_block(s, e, pred), 0};
            while (top >= 2 && st[top - 2].lvl == st[top - 1].lvl) fuse();
        }
        while (top >= 2) fuse();
        return st[0].sp;
    }

    // ------------------------------------------------------ pivote
    std::size_t med3(std::size_t i, std::size_t j, std::size_t k) {
        if (lt(a_[j], a_[i])) std::swap(i, j);
        if (lt(a_[k], a_[j])) { j = k; if (lt(a_[j], a_[i])) j = i; }
        return j;
    }
    // Pivote = mediana de una muestra de ~2*cbrt(n) elementos, copiada al buffer
    // (no toca el array, así que no afecta la estabilidad). Mejor pivote =
    // particiones más parejas = menos niveles de rotación.
    T pivot_value(std::size_t lo, std::size_t n) {
        if (n <= SQ_NINTHER) {                         // rangos chicos: ninther sin copiar
            std::size_t s = n / 8;
            std::size_t x = med3(lo, lo + s, lo + 2 * s);
            std::size_t y = med3(lo + 3 * s, lo + 4 * s, lo + 5 * s);
            std::size_t z = med3(lo + 6 * s, lo + 7 * s, lo + n - 1);
            return a_[med3(x, y, z)];
        }
        std::size_t s = 2 * static_cast<std::size_t>(std::cbrt(static_cast<double>(n))) | 1;
        if (s > B - 1) s = (B - 1) | 1;
        if (s < 9) s = 9;
        std::size_t step = n / s;
        for (std::size_t i = 0; i < s; ++i) buf_[i] = a_[lo + i * step + step / 2];
        std::nth_element(buf_, buf_ + s / 2, buf_ + s, [&](const T& x, const T& y) { return lt(x, y); });
        return buf_[s / 2];
    }

    // ------------------------------------------------------ quicksort
    void quick(std::size_t n) {
        struct R { std::size_t lo, hi; int bad; } st[STACK];
        int sp = 0, budget = 0;
        for (std::size_t k = n; k; k >>= 1) ++budget;
        st[sp++] = {0, n, budget};
        while (sp) {
            R w = st[--sp];
            std::size_t lo = w.lo, hi = w.hi; int bad = w.bad;
            for (;;) {
                std::size_t len = hi - lo;
                if (len <= LEAF) { leaf_sort(lo, hi); break; }
                if (bad <= 0)    { if (!presorted(lo, hi)) merge_sort(lo, hi); break; }
                T piv = pivot_value(lo, len);
                std::size_t mid = stable_partition(lo, hi, [&](const T& x) { return lt(x, piv); });
                if (mid == lo) {
                    // pivote = mínimo del rango: separar el bloque de iguales, ya terminado
                    lo = stable_partition(lo, hi, [&](const T& x) { return !lt(piv, x); });
                    continue;
                }
                std::size_t nl = mid - lo, nr = hi - mid;
                if (std::min(nl, nr) < len / 16) --bad;
                // seguir con el chico, apilar el grande -> pila O(log n)
                if (nl < nr) { st[sp++] = {mid, hi, bad}; hi = mid; }
                else         { st[sp++] = {lo, mid, bad}; lo = mid; }
            }
        }
    }
};

template <class T, class Cmp = std::less<T>>
void stable_sort(T* a, std::size_t n, Cmp c = Cmp()) {
    Sorter<T, Cmp> s(a, c);
    s.sort(n);
}

}  // namespace sq

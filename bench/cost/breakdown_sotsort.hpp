// Copia instrumentada de include/sotsort.hpp (fases contadas en PH[]); generada para bench/cost/breakdown.cpp.
// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Lucas Sotomayor
//
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at
// https://mozilla.org/MPL/2.0/.
//
// sotsort.hpp — SotSort v8-r (RECONSTRUCCIÓN, 2026-11-17)
//
// El código fuente original de la versión 8 se perdió. Este archivo se
// reescribió a partir de: el pseudocódigo y las demostraciones de v6.1/v8
// (sotsort_demo.tex, v8_algs.tex, demo_v8.tex), el libro (v7), el motor del
// visor (engine.js) y sq5.hpp (código real de una versión anterior).
// NO es el mismo código que produjo las mediciones publicadas de v8: todo
// número debe volver a medirse con este archivo.
//
// Ordenamiento estable. Dos modos:
//   sot::stable_sort(a, n, cmp)            memoria O(1): buffer propio de ~16 KB
//                                           en el stack, sin heap ni recursión.
//   sot::stable_sort_fast(a, n, cmp, opt)  usa la memoria que haya (plan
//                                           O1 / Bloques / Completo), galope,
//                                           conteo de enteros e hilos.
//   sot::stable_sort_by_key(a, n, key, opt) orden estable por clave entera.
//
// Requisitos: T movible, copiable (el pivote es una copia) y
// default-constructible (buffer y pilas). C++17.
// Para reproducir el defecto de v6 (sin presupuesto en la separación de
// iguales) compilar con -DSOT_V6_BUG.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <new>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#if defined(__AVX2__) || defined(__AVX512F__)
#include <immintrin.h>
#endif
#if defined(__linux__)
#include <sys/mman.h>
#endif
#if defined(_WIN32)
#include <malloc.h>
#include <windows.h>
#endif

namespace sot {

struct Options {
    std::size_t max_bytes = static_cast<std::size_t>(-1);  // tope de memoria auxiliar
    double fraction = 0.5;      // fracción de la memoria disponible que se puede usar
    bool probe = true;          // consultar la memoria disponible (solo si el buffer pasaría de 1 MB)
    unsigned threads = 0;       // 0 = hilos del hardware; 1 = secuencial
    bool shortcuts = true;      // conteo de enteros
    std::uint64_t seed = 0;     // 0 = semilla fija (determinista); otro valor = semilla del muestreo
};

struct Plan {
    enum Mode { O1, Block, Full } mode = O1;
    std::size_t E = 0;          // elementos del buffer auxiliar
    unsigned P = 1;             // hilos
    std::size_t budget = 0;     // presupuesto en bytes
    bool memory_known = false;  // la consulta de memoria dio un valor
};

namespace detail {

constexpr std::uint64_t SIGMA = 0x9E3779B97F4A7C15ull;

template <class T, class Cmp> struct natural_less : std::false_type {};
template <class T> struct natural_less<T, std::less<T>> : std::true_type {};
template <class T> struct natural_less<T, std::less<void>> : std::true_type {};

template <class T>
constexpr bool simd_int = std::is_integral<T>::value && !std::is_same<T, bool>::value &&
                          (sizeof(T) == 4 || sizeof(T) == 8);

inline std::size_t bit_floor(std::size_t x) {
    if (!x) return 0;
    std::size_t r = 1;
    while (r <= x / 2) r <<= 1;
    return r;
}
inline unsigned bits(std::uint64_t x) { unsigned b = 0; while (x) { ++b; x >>= 1; } return b; }

// ---------------------------------------------------------------- partición vectorial
// Parte p[0..len) contra piv con x < piv (le = false) o x <= piv (le = true).
// Los que cumplen quedan en p[0..m) y los que no en buf[0..i-m), en orden.
// Procesa un prefijo múltiplo del ancho; devuelve m y deja en `done` lo procesado.
#if defined(__AVX2__) && !defined(__AVX512F__)
struct PermTables {
    alignas(32) std::int32_t p8[16][8];
    alignas(16) std::int32_t p4[16][4];
    PermTables() {
        for (int k = 0; k < 16; ++k) {
            int c = 0;
            for (int j = 0; j < 4; ++j) if (k >> j & 1) { p4[k][c] = j; p8[k][2 * c] = 2 * j; p8[k][2 * c + 1] = 2 * j + 1; ++c; }
            for (; c < 4; ++c) { p4[k][c] = 0; p8[k][2 * c] = 0; p8[k][2 * c + 1] = 1; }
        }
    }
};
inline const PermTables& perm_tables() { static const PermTables t; return t; }
#endif

template <class T>
inline std::size_t simd_part(T* p, std::size_t len, T* buf, T piv, bool le, std::size_t& done) {
    std::size_t m = 0, i = 0;
    const bool sg = std::is_signed<T>::value;
    (void)sg; (void)p; (void)len; (void)buf; (void)piv; (void)le;
#if defined(__AVX512F__)
    if (sizeof(T) == 8) {
        const __m512i P = _mm512_set1_epi64(static_cast<long long>(piv));
        for (; i + 8 <= len; i += 8) {
            const __m512i X = _mm512_loadu_si512(static_cast<const void*>(p + i));
            __mmask8 k;
            if (sg) k = le ? _mm512_cmple_epi64_mask(X, P) : _mm512_cmplt_epi64_mask(X, P);
            else    k = le ? _mm512_cmple_epu64_mask(X, P) : _mm512_cmplt_epu64_mask(X, P);
            _mm512_storeu_si512(static_cast<void*>(p + m), _mm512_maskz_compress_epi64(k, X));
            _mm512_storeu_si512(static_cast<void*>(buf + (i - m)), _mm512_maskz_compress_epi64(static_cast<__mmask8>(~k), X));
            m += static_cast<std::size_t>(_mm_popcnt_u32(k));
        }
    } else {
        const __m512i P = _mm512_set1_epi32(static_cast<int>(piv));
        for (; i + 16 <= len; i += 16) {
            const __m512i X = _mm512_loadu_si512(static_cast<const void*>(p + i));
            __mmask16 k;
            if (sg) k = le ? _mm512_cmple_epi32_mask(X, P) : _mm512_cmplt_epi32_mask(X, P);
            else    k = le ? _mm512_cmple_epu32_mask(X, P) : _mm512_cmplt_epu32_mask(X, P);
            _mm512_storeu_si512(static_cast<void*>(p + m), _mm512_maskz_compress_epi32(k, X));
            _mm512_storeu_si512(static_cast<void*>(buf + (i - m)), _mm512_maskz_compress_epi32(static_cast<__mmask16>(~k), X));
            m += static_cast<std::size_t>(_mm_popcnt_u32(k));
        }
    }
#elif defined(__AVX2__)
    const PermTables& tb = perm_tables();
    if (sizeof(T) == 8) {
        const __m256i bias = sg ? _mm256_setzero_si256() : _mm256_set1_epi64x(static_cast<long long>(0x8000000000000000ull));
        const __m256i P = _mm256_xor_si256(_mm256_set1_epi64x(static_cast<long long>(piv)), bias);
        for (; i + 4 <= len; i += 4) {
            const __m256i X = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p + i));
            const __m256i Xb = _mm256_xor_si256(X, bias);
            int k;
            if (le) k = ~_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(Xb, P))) & 15;
            else    k = _mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(P, Xb)));
            const __m256i yes = _mm256_permutevar8x32_epi32(X, _mm256_load_si256(reinterpret_cast<const __m256i*>(tb.p8[k])));
            const __m256i no = _mm256_permutevar8x32_epi32(X, _mm256_load_si256(reinterpret_cast<const __m256i*>(tb.p8[~k & 15])));
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(p + m), yes);
            _mm256_storeu_si256(reinterpret_cast<__m256i*>(buf + (i - m)), no);
            m += static_cast<std::size_t>(_mm_popcnt_u32(static_cast<unsigned>(k)));
        }
    } else {
        const __m128i bias = sg ? _mm_setzero_si128() : _mm_set1_epi32(static_cast<int>(0x80000000u));
        const __m128i P = _mm_xor_si128(_mm_set1_epi32(static_cast<int>(piv)), bias);
        for (; i + 4 <= len; i += 4) {
            const __m128i X = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i));
            const __m128i Xb = _mm_xor_si128(X, bias);
            int k;
            if (le) k = ~_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(Xb, P))) & 15;
            else    k = _mm_movemask_ps(_mm_castsi128_ps(_mm_cmpgt_epi32(P, Xb)));
            const __m128 Xf = _mm_castsi128_ps(X);
            const __m128i yes = _mm_castps_si128(_mm_permutevar_ps(Xf, _mm_load_si128(reinterpret_cast<const __m128i*>(tb.p4[k]))));
            const __m128i no = _mm_castps_si128(_mm_permutevar_ps(Xf, _mm_load_si128(reinterpret_cast<const __m128i*>(tb.p4[~k & 15]))));
            _mm_storeu_si128(reinterpret_cast<__m128i*>(p + m), yes);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(buf + (i - m)), no);
            m += static_cast<std::size_t>(_mm_popcnt_u32(static_cast<unsigned>(k)));
        }
    }
#endif
    done = i;
    return m;
}

#if defined(__AVX2__) || defined(__AVX512F__)
constexpr bool have_simd = true;
#else
constexpr bool have_simd = false;
#endif

// Máscara de adyacentes: bit j = (p[j] < p[j-1]), para j en [0, W). Requiere p[-1..W) válido.
template <class T> struct AdjW {
#if defined(__AVX512F__)
    static constexpr std::size_t W = 64 / sizeof(T);
#elif defined(__AVX2__)
    static constexpr std::size_t W = 32 / sizeof(T);
#else
    static constexpr std::size_t W = 1;
#endif
};
template <class T>
inline unsigned adj_lt_mask(const T* p) {
    const bool sg = std::is_signed<T>::value; (void)sg; (void)p;
#if defined(__AVX512F__)
    const __m512i Y = _mm512_loadu_si512(static_cast<const void*>(p));
    const __m512i X = _mm512_loadu_si512(static_cast<const void*>(p - 1));
    if (sizeof(T) == 8) return sg ? _mm512_cmplt_epi64_mask(Y, X) : _mm512_cmplt_epu64_mask(Y, X);
    return sg ? _mm512_cmplt_epi32_mask(Y, X) : _mm512_cmplt_epu32_mask(Y, X);
#elif defined(__AVX2__)
    __m256i Y = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p));
    __m256i X = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(p - 1));
    if (sizeof(T) == 8) {
        if (!sg) { const __m256i b = _mm256_set1_epi64x(static_cast<long long>(0x8000000000000000ull)); X = _mm256_xor_si256(X, b); Y = _mm256_xor_si256(Y, b); }
        return static_cast<unsigned>(_mm256_movemask_pd(_mm256_castsi256_pd(_mm256_cmpgt_epi64(X, Y))));
    }
    if (!sg) { const __m256i b = _mm256_set1_epi32(static_cast<int>(0x80000000u)); X = _mm256_xor_si256(X, b); Y = _mm256_xor_si256(Y, b); }
    return static_cast<unsigned>(_mm256_movemask_ps(_mm256_castsi256_ps(_mm256_cmpgt_epi32(X, Y))));
#else
    return 0;
#endif
}
inline unsigned ctz32(unsigned x) { unsigned n = 0; while (!(x & 1u)) { x >>= 1; ++n; } return n; }
inline unsigned pop32(unsigned x) { unsigned n = 0; while (x) { x &= x - 1; ++n; } return n; }

// Selección sin branch: devuelve t ? a : b con aritmética de punteros. gcc convierte
// `t ? *a : *b` en un salto en varios de estos lazos; clang no. Medido: ver docs.
template <class T>
inline const T* sel(bool t, const T* a, const T* b) {
#if defined(SOT_NO_SEL) || defined(__clang__) || !defined(__GNUC__)
    return t ? a : b;
#else
    if (std::is_arithmetic<T>::value) return t ? a : b;      // con escalares gcc ya emite cmov
    const std::uintptr_t ua = reinterpret_cast<std::uintptr_t>(a), ub = reinterpret_cast<std::uintptr_t>(b);
    return reinterpret_cast<const T*>(ub ^ ((ua ^ ub) & (std::uintptr_t(0) - static_cast<std::uintptr_t>(t))));
#endif
}

// Galope: cantidad de índices iniciales de [0, m) que cumplen q (q monótono:
// verdadero y después falso). Prueba 0, 1, 3, 7, 15, ... y termina con
// búsqueda binaria.
template <class Q>
inline std::size_t gallop(std::size_t m, Q q) {
    if (m == 0 || !q(std::size_t(0))) return 0;
    std::size_t i = 0, d = 1;
    while (i + d < m && q(i + d)) { i += d; d *= 2; }
    std::size_t lo = i + 1, hi = std::min(i + d, m);
    while (lo < hi) { std::size_t mid = lo + ((hi - lo) >> 1); if (q(mid)) lo = mid + 1; else hi = mid; }
    return lo;
}

}  // namespace detail

// =====================================================================
template <class T, class Cmp = std::less<T>>
class Sorter {
public:
#ifndef SOT_B0_BYTES
#define SOT_B0_BYTES 16384
#endif
#ifndef SOT_MERGE_BYTES
#define SOT_MERGE_BYTES 64
#endif
#ifndef SOT_LEAF
#define SOT_LEAF 128
#endif
    static constexpr std::size_t LEAF = SOT_LEAF;   // rangos <= LEAF van a la hoja (LEAF <= B0)
    static constexpr std::size_t B0 = (SOT_B0_BYTES / sizeof(T)) > LEAF ? (SOT_B0_BYTES / sizeof(T)) : LEAF;  // buffer propio
    static constexpr std::size_t RUN = 24;       // inserción / largo mínimo de run
    static constexpr std::size_t N9 = 8192;      // hasta aquí el pivote es un ninther
    static constexpr std::size_t MG = 7;         // umbral de galope
    static constexpr int STACK = 96;
    static constexpr bool TRIV = std::is_trivially_copyable<T>::value;
    static constexpr bool SIMD = detail::have_simd && detail::simd_int<T> && detail::natural_less<T, Cmp>::value;

    Sorter(T* a, Cmp c) : a_(a), c_(c), buf_(own_), B_(B0) {}
    Sorter(const Sorter&) = delete;
    Sorter& operator=(const Sorter&) = delete;

    void use_buffer(T* buf, std::size_t cap) { if (cap > B0) { buf_ = buf; B_ = cap; } else { buf_ = own_; B_ = B0; } }
    // Buffer completo "que sigue al rango": al procesar [lo, hi) se usa base[lo..hi).
    // Rangos disjuntos usan zonas disjuntas del buffer (lo necesita el modo OOO).
    void follow_buffer(T* base) { aux_base_ = base; }
    bool has_few_turns(std::size_t n) { turns_ = (n > 2 && few_turns(n)) ? 1 : 0; return turns_ == 1; }
    // Giros en i de [from, to), from >= 2: permite contar por tramos, en paralelo, con el mismo total.
    std::size_t count_turns(std::size_t from, std::size_t to) {
        std::size_t g = 0;
        if (from >= to) return 0;
        bool prev = lt(a_[from - 1], a_[from - 2]);
        for (std::size_t i = from; i < to; ++i) { bool d = lt(a_[i], a_[i - 1]); g += d != prev; prev = d; }
        return g;
    }
    void set_few_turns(bool few) { turns_ = few ? 1 : 0; }
    bool sample_is_ordered(std::size_t n) { return sample_ordered(n); }
    // Piezas para la partición paralela de la raíz (experimental).
    T pick_pivot(std::size_t lo, std::size_t n) { return pivot_value(lo, n); }
    std::size_t part_chunk(std::size_t lo, std::size_t hi, const T& piv, T* out) {   // x < piv; el resto queda en out
        return part_block_to(lo, hi, piv, false, out, false) - lo;
    }

    // Interfaz para planificadores externos (experimental): recibe la mitad grande de una
    // partición. Si devuelve true, la tarea queda a cargo del planificador.
    struct Spawner {
        virtual bool spawn(std::size_t lo, std::size_t hi, int bad, bool has, const T& lb) = 0;
        virtual ~Spawner() = default;
    };
    void quick_task(std::size_t lo, std::size_t hi, int bad, bool has, const T& lb, Spawner* spw, std::size_t grain) {
        adaptive_ = false;
        quick_from(lo, hi, bad, has, lb, spw, grain);
    }
    void set_gallop(bool g) { gallop_ = g; }
    void seed(std::uint64_t s) { rng_ = s ? s : detail::SIGMA; }

    // Ya ordenado -> true. Estrictamente descendente -> se invierte (estable) y true.
    bool already_sorted(std::size_t lo, std::size_t hi) {
        if (hi - lo < 2) { asc_end_ = hi; return true; }
        std::size_t i = lo + 1;
        if constexpr (SIMD) {
            const std::size_t W = detail::AdjW<T>::W;
            while (i + W <= hi) {
                unsigned m = detail::adj_lt_mask(a_ + i);
                if (m) { i += detail::ctz32(m); break; }
                i += W;
            }
        }
        while (i < hi && !lt(a_[i], a_[i - 1])) ++i;
        asc_end_ = i;
        if (i == hi) return true;
        if (i == lo + 1) {
            if constexpr (SIMD) {
                const std::size_t W = detail::AdjW<T>::W;
                const unsigned full = W >= 32 ? ~0u : ((1u << W) - 1);
                while (i + W <= hi) {
                    unsigned m = ~detail::adj_lt_mask(a_ + i) & full;
                    if (m) { i += detail::ctz32(m); break; }
                    i += W;
                }
            }
            while (i < hi && lt(a_[i], a_[i - 1])) ++i;
            if (i == hi) { std::reverse(a_ + lo, a_ + hi); return true; }
        }
        return false;
    }

    void sort(std::size_t n) {
        if (n < 2) return;
        if (already_sorted(0, n)) return;
        sort_unsorted(n);
    }
    // Precondición: already_sorted(0, n) acaba de devolver false.
    void sort_unsorted(std::size_t n) {
        const bool few = turns_ >= 0 ? turns_ == 1 : (n > 2 && few_turns(n));
        turns_ = -1;
        if (few) {
            adaptive_ = true;            // datos con estructura: merges con branch y, si se pidió, galope
            natural_merge_sort(0, n, asc_end_);
            return;
        }
        adaptive_ = false;
        quick(n);
    }

    void quick_only(std::size_t n) { adaptive_ = false; if (n > 1) quick(n); }   // para experimentos (adversario)

private:
    T* a_;
    Cmp c_;
    T* buf_;
    std::size_t B_;
#ifdef SOT_GALLOP_ALL
    bool adaptive_ = false, gallop_ = true;
#else
    bool adaptive_ = false, gallop_ = !std::is_arithmetic<T>::value;   // tipos no aritméticos: galope siempre
#endif
    std::size_t asc_end_ = 0;
    T* aux_base_ = nullptr;
    int turns_ = -1;                 // caché de has_few_turns: -1 sin calcular
    std::uint64_t rng_ = detail::SIGMA;
    T own_[B0];

    bool lt(const T& x, const T& y) { return c_(x, y); }
    std::uint64_t rnd() {  // xorshift64*
        rng_ ^= rng_ >> 12; rng_ ^= rng_ << 25; rng_ ^= rng_ >> 27;
        return rng_ * 0x2545F4914F6CDD1Dull;
    }

    // ------------------------------------------------------------ primitivas
    std::size_t lower_bound(std::size_t f, std::size_t l, const T& v) {
        while (f < l) { std::size_t m = f + ((l - f) >> 1); if (lt(a_[m], v)) f = m + 1; else l = m; }
        return f;
    }
    std::size_t upper_bound(std::size_t f, std::size_t l, const T& v) {
        while (f < l) { std::size_t m = f + ((l - f) >> 1); if (lt(v, a_[m])) l = m; else f = m + 1; }
        return f;
    }

    // Pocos giros (cambios entre tramo ascendente y descendente): runs largos.
    // g solo crece, así que se puede cortar apenas llega a n/8.
    bool few_turns(std::size_t n) {
        // v8-r4: con elementos grandes (64 bytes o más) mover cuesta más que comparar, y si el
        // buffer alcanza para la mezcla por páginas, las mezclas (~1,5 movimientos por elemento y
        // nivel) le ganan al quicksort (dos escrituras por elemento y nivel, más rotaciones),
        // también con entrada aleatoria. Medido en una sola máquina; SOT_MERGE_BYTES=0 lo apaga.
        if constexpr (SOT_MERGE_BYTES > 0 && sizeof(T) >= SOT_MERGE_BYTES) {
            if (page_ok(n)) return true;
        }
#ifndef SOT_EXACT_TURNS
        // v8-r3: con n grande, la densidad de giros se estima con 64 ventanas de 256 pares
        // (desplazamiento aleatorio). Contarlos todos costaba una comparación por elemento,
        // que la detección de runs volvía a pagar. La decisión solo elige el camino: los dos
        // ordenan bien, así que un error de estimación solo cuesta tiempo.
        if (n >= TURN_SAMPLE_MIN) return route_by_turns(n, sampled_turns(n));
#endif
        std::size_t g = 0;
        bool prev = lt(a_[1], a_[0]);
        std::size_t i = 2;
        while (i < n) {
            std::size_t e = std::min(n, i + 4096);
            if constexpr (SIMD) {
                const std::size_t W = detail::AdjW<T>::W;
                const unsigned full = W >= 32 ? ~0u : ((1u << W) - 1);
                unsigned carry = prev;
                for (; i + W <= e; i += W) {
                    unsigned m = detail::adj_lt_mask(a_ + i);
                    g += detail::pop32((m ^ ((m << 1) | carry)) & full);
                    carry = (m >> (W - 1)) & 1u;
                }
                prev = carry != 0;
            }
            for (; i < e; ++i) { bool d = lt(a_[i], a_[i - 1]); g += d != prev; prev = d; }
            if (g * 8 >= n) return false;
        }
        return route_by_turns(n, g);
    }
    static constexpr std::size_t TURN_SAMPLE_MIN = std::size_t(1) << 16;
    // Giros estimados para todo [0, n): 64 ventanas de 256 pares, cada una en su tramo.
    std::size_t sampled_turns(std::size_t n) {
        const std::size_t K = 64, Wn = 256, seg = n / K;          // seg >= 1024
        std::size_t g = 0;
        for (std::size_t k = 0; k < K; ++k) {
            std::size_t s0 = k * seg + 1 + static_cast<std::size_t>(rnd() % (seg - Wn - 1));
            bool prev = lt(a_[s0], a_[s0 - 1]);
            for (std::size_t i = s0 + 1; i <= s0 + Wn; ++i) { bool d = lt(a_[i], a_[i - 1]); g += d != prev; prev = d; }
        }
        return g * n / (K * Wn);                                    // escala a n (aprox.)
    }
    // Regla de ruta común: g giros (contados o estimados) en n elementos.
    bool route_by_turns(std::size_t n, std::size_t g) {
        if (g * 8 >= n) return false;
        if (!TRIV) return true;                        // tipos no triviales: la regla de ruta midió peor (strings)
        if (sample_ordered(n)) return true;            // orden global: mezclas con rachas largas
        // Runs con valores entremezclados: mezclar cuesta lg(runs) niveles y el quicksort
        // lg(n/L). Un nivel de mezcla es más caro: k veces, según el modo (constantes medidas
        // en una sola máquina). runs ~ giros/2 + 1.
        std::size_t runs = g / 2 + 1;
        unsigned lr = 0, lq = 0;
        while ((std::size_t(1) << lr) < runs) ++lr;
        while ((LEAF << lq) < n) ++lq;
        const bool full = B_ >= n / 2;
        const unsigned k4 = full ? (SIMD ? 8u : 5u) : (SIMD ? 12u : 8u);   // k en cuartos
        return lr * k4 <= lq * 4;
    }
    // Sonda de orden global (v8-r2): 256 muestras equiespaciadas. Si la dirección entre muestras
    // vecinas cambia seguido, los runs tienen valores entremezclados (sierra de claves al azar)
    // y mezclarlos cuesta lg(runs) niveles: conviene el quicksort. Solo afecta el tiempo.
    bool sample_ordered_impl(std::size_t n) {
        if (n < 4096) return true;
        const std::size_t S = 256;
        std::size_t ch = 0;
        const std::size_t st = n / S;                  // posiciones con desplazamiento pseudoaleatorio:
        std::size_t p0 = static_cast<std::size_t>(rnd() % st), p1 = st + static_cast<std::size_t>(rnd() % st);   // sin él, runs de largo fijo hacen alias
        bool prev = lt(a_[p1], a_[p0]);
        for (std::size_t j = 2; j < S; ++j) {
            std::size_t p2 = j * st + static_cast<std::size_t>(rnd() % st);
            bool d = lt(a_[p2], a_[p1]);
            ch += d != prev; prev = d; p1 = p2;
        }
        return ch * 4 < S;
    }
    bool sample_ordered(std::size_t n) {
#ifdef SOT_NO_SAMPLE
        (void)n; return true;
#else
        return sample_ordered_impl(n);
#endif
    }

#if defined(__GNUC__) || defined(__clang__)
    __attribute__((always_inline))
#endif
    inline void insertion(std::size_t lo, std::size_t hi) {
        for (std::size_t i = lo + 1; i < hi; ++i) {
            if (!lt(a_[i], a_[i - 1])) continue;
            T v = std::move(a_[i]);
            std::size_t j = i;
            do { a_[j] = std::move(a_[j - 1]); --j; } while (j > lo && lt(v, a_[j - 1]));
            a_[j] = std::move(v);
        }
    }

    // ----------------------------------------------------------- rotación
    void rotate(std::size_t l, std::size_t m, std::size_t r) {
        std::size_t nl = m - l, nr = r - m;
        if (!nl || !nr) return;
        const std::size_t B = B_;
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
        if (nl > nr && nl - nr <= B) {           // puente: [A1 (nr) | A2 (d) | Bb (nr)] -> [Bb | A1 | A2]
            std::size_t d = nl - nr;
            T* A = a_ + l;
            std::move(A + nr, A + nl, buf_);
            for (std::size_t i = 0; i < nr; ++i) { A[nr + i] = std::move(A[i]); A[i] = std::move(A[nr + d + i]); }
            std::move(buf_, buf_ + d, A + 2 * nr);
            return;
        }
        if (nr > nl && nr - nl <= B) {           // puente simétrico: [A (nl) | B1 (d) | B2 (nl)] -> [B1 | B2 | A]
            std::size_t d = nr - nl;
            T* A = a_ + l;
            std::move(A + nl, A + nl + d, buf_);
            for (std::size_t i = nl; i-- > 0;) { A[d + i] = std::move(A[nl + d + i]); A[nl + d + i] = std::move(A[i]); }
            std::move(buf_, buf_ + d, A);
            return;
        }
        fused_reversal_rotate(a_ + l, a_ + m, a_ + r);
    }

    // ------------------------------------------------- mezcla por páginas (v8-r4)
    // Para mezclas donde los dos runs superan el buffer. Idea de Virtual-Memory Powersort
    // (Moltmann, Nakajima, Wild, ESA 2026), adaptada: la zona [l, r) se divide en páginas de
    // P = B/8 elementos y el buffer aporta 8 páginas libres. La mezcla escribe cada página de
    // salida en una página libre (del buffer o de entrada ya consumida) y anota dónde quedó;
    // al final las páginas se permutan a su lugar siguiendo ciclos. Movimientos: n para la
    // mezcla más a lo sumo n + P·ciclos para la permutación, sin rotaciones.
    // Lugar: con c elementos consumidos hay al menos c/P − 3 páginas de entrada liberadas
    // (dos páginas parciales y la que comparten X e Y), y la salida usa a lo sumo c/P + 1; con
    // 8 del buffer (una menos si la última página es parcial y queda reservada) siempre sobra.
    // La tabla ocupa 4·(n/P + 9) bytes en el heap; se usa solo si es a lo sumo 1/8 del buffer.
    static constexpr std::size_t PG_K = 8;
    std::vector<std::uint32_t> pg_, pgw_, pgf_;
    bool page_ok(std::size_t n) const {
        const std::size_t P = B_ / PG_K;
        if (buf_ == own_) return false;                      // modo O(1): sin memoria en el heap
        if (P < 64 || n / P + PG_K + 2 >= (std::size_t(1) << 31)) return false;
        return (n / P + PG_K + 2) * sizeof(std::uint32_t) * 8 <= B_ * sizeof(T);
    }
    void page_merge(std::size_t l, std::size_t m, std::size_t r) {
        const std::size_t n = r - l, nl = m - l, P = B_ / PG_K;
        const std::size_t R = (n + P - 1) / P, last = n - (R - 1) * P;
        const bool partial = last < P;
        T* A = a_ + l;
        auto base = [&](std::size_t ph) -> T* { return ph < R ? A + ph * P : buf_ + (ph - R) * P; };
        auto plen = [&](std::size_t lg) { return lg == R - 1 ? last : P; };
        pg_.assign(R, 0);
        pgf_.clear();
        for (std::size_t k = PG_K; k-- > 0;) pgf_.push_back(static_cast<std::uint32_t>(R + k));
        bool last_free = false, bnd_rel = false;
        const std::size_t bnd = nl / P;                       // página que comparten X e Y (si nl % P)
        // El cursor de Y empieza después de la página compartida: esa la libera el de X.
        std::size_t x = 0, y = nl, olog = 0, orem = 0, xp = 0, yp = nl % P ? bnd + 1 : bnd;
        T* out = nullptr;
        auto consumed = [&](std::size_t i) {
            std::size_t s0 = i * P, e0 = std::min(s0 + P, n);
            if (e0 <= nl) return x >= e0;
            if (s0 >= nl) return y >= e0;
            return x >= nl && y >= e0;
        };
        auto release = [&](std::size_t i) {
            if (i == bnd && nl % P) { if (bnd_rel) return; bnd_rel = true; }
            if (partial && i == R - 1) last_free = true;
            else pgf_.push_back(static_cast<std::uint32_t>(i));
        };
        while (olog < R || orem) {
            if (!orem) {
                std::size_t ph;
                if (olog == R - 1 && partial && last_free) { ph = R - 1; last_free = false; }
                else { ph = pgf_.back(); pgf_.pop_back(); }
                pg_[olog] = static_cast<std::uint32_t>(ph);
                out = base(ph); orem = plen(olog); ++olog;
            }
            std::size_t xa = x < nl ? std::min(nl, (x / P + 1) * P) - x : 0;
            std::size_t ya = y < n ? std::min(n, (y / P + 1) * P) - y : 0;
            std::size_t k;
            if (!xa) { k = std::min(orem, ya); out = std::move(A + y, A + y + k, out); y += k; }
            else if (!ya) { k = std::min(orem, xa); out = std::move(A + x, A + x + k, out); x += k; }
            else {
                k = std::min(orem, std::min(xa, ya));
                T *px = A + x, *py = A + y;
                for (std::size_t t = 0; t < k; ++t) {
                    if (lt(*py, *px)) *out++ = std::move(*py++);
                    else *out++ = std::move(*px++);
                }
                x = static_cast<std::size_t>(px - A); y = static_cast<std::size_t>(py - A);
            }
            orem -= k;
            while (xp * P < nl && consumed(xp)) release(xp++);
            while (yp < R && consumed(yp)) release(yp++);
        }
        // Permutación: la página lógica j va a la página física j de la zona.
        pgw_.assign(R + PG_K, 0xFFFFFFFFu);                    // física -> lógica
        for (std::size_t j = 0; j < R; ++j) pgw_[pg_[j]] = static_cast<std::uint32_t>(j);
        auto chain = [&](std::size_t cur) {                    // cur: página de la zona libre
            for (;;) {
                std::size_t src = pg_[cur];
                std::move(base(src), base(src) + plen(cur), base(cur));
                pgw_[cur] = static_cast<std::uint32_t>(cur); pg_[cur] = static_cast<std::uint32_t>(cur);
                pgw_[src] = 0xFFFFFFFFu;
                if (src >= R) return;
                cur = src;
            }
        };
        for (std::size_t f = 0; f < R; ++f)
            if (pgw_[f] == 0xFFFFFFFFu) chain(f);
        for (std::size_t j = 0; j < R; ++j) {
            if (pg_[j] == j) continue;
            std::size_t w = pgw_[j], tmp = R;                  // ciclo: la ocupante va a una página del buffer
            while (pgw_[tmp] != 0xFFFFFFFFu) ++tmp;
            std::move(base(j), base(j) + plen(w), base(tmp));
            pg_[w] = static_cast<std::uint32_t>(tmp); pgw_[tmp] = static_cast<std::uint32_t>(w); pgw_[j] = 0xFFFFFFFFu;
            chain(j);
        }
    }

    // Triple reversión fusionada (idea: trinity rotation de scandum).
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
        if (left < right) {
            for (std::size_t k = (pd - pc) / 2; k; --k) {
                T t = std::move(*pc);
                *pc++ = std::move(*--pd);
                *pd = std::move(*pa);
                *pa++ = std::move(t);
            }
        } else {
            for (std::size_t k = (pb - pa) / 2; k; --k) {
                T t = std::move(*--pb);
                *pb = std::move(*pa);
                *pa++ = std::move(*--pd);
                *pd = std::move(t);
            }
        }
        for (std::size_t k = (pd - pa) / 2; k; --k) {
            T t = std::move(*pa);
            *pa++ = std::move(*--pd);
            *pd = std::move(t);
        }
    }

    // ------------------------------------------------- merges con buffer
    // Precondición: (m-l) <= B. Fusión hacia adelante.
    // modo: 0 = sin branch, 1 = con branch, 2 = galope
    void merge_left_buf(std::size_t l, std::size_t m, std::size_t r, int mode) {
        std::size_t nl = m - l;
        std::move(a_ + l, a_ + m, buf_);
        T *p = buf_, *pe = buf_ + nl, *q = a_ + m, *qe = a_ + r, *o = a_ + l;
        if (mode == 2) {
            while (p < pe && q < qe) {
                std::size_t cx = 0, cy = 0;
                if constexpr (TRIV) {                   // paso de cabeza sin branch; los contadores van por cmov
                    while (p < pe && q < qe && std::max(cx, cy) < MG) {
                        bool t = lt(*q, *p);
                        *o++ = t ? *q : *p;
                        q += t; p += !t;
                        cy = t ? cy + 1 : 0; cx = t ? 0 : cx + 1;
                    }
                } else {
                    while (p < pe && q < qe && std::max(cx, cy) < MG) {
                        if (lt(*q, *p)) { *o++ = std::move(*q++); ++cy; cx = 0; }
                        else            { *o++ = std::move(*p++); ++cx; cy = 0; }
                    }
                }
                while (p < pe && q < qe) {
                    std::size_t k = detail::gallop(static_cast<std::size_t>(pe - p), [&](std::size_t i) { return !lt(*q, p[i]); });
                    o = std::move(p, p + k, o); p += k;
                    if (p == pe) break;
                    std::size_t k2 = detail::gallop(static_cast<std::size_t>(qe - q), [&](std::size_t i) { return lt(q[i], *p); });
                    o = std::move(q, q + k2, o); q += k2;      // o < q: hacia adelante
                    if (k < MG && k2 < MG) break;
                }
            }
            std::move(p, pe, o);
            return;
        }
        if (mode == 1) {
            while (p < pe && q < qe) { if (lt(*q, *p)) *o++ = std::move(*q++); else *o++ = std::move(*p++); }
        } else {
            while (p < pe && q < qe) {
                bool t = lt(*q, *p);
                *o++ = std::move(t ? *q : *p);
                q += t; p += !t;
            }
        }
        std::move(p, pe, o);
    }
    // Precondición: (r-m) <= B. Fusión hacia atrás.
    void merge_right_buf(std::size_t l, std::size_t m, std::size_t r, int mode) {
        std::size_t nr = r - m;
        std::move(a_ + m, a_ + r, buf_);
        T *i = a_ + m, *il = a_ + l, *j = buf_ + nr, *o = a_ + r;   // quedan [il, i) y [buf_, j)
        if (mode == 2) {
            while (i > il && j > buf_) {
                std::size_t cx = 0, cy = 0;
                if constexpr (TRIV) {
                    while (i > il && j > buf_ && std::max(cx, cy) < MG) {
                        bool t = lt(*(j - 1), *(i - 1));
                        *--o = t ? *(i - 1) : *(j - 1);
                        i -= t; j -= !t;
                        cx = t ? cx + 1 : 0; cy = t ? 0 : cy + 1;
                    }
                } else {
                    while (i > il && j > buf_ && std::max(cx, cy) < MG) {
                        if (lt(*(j - 1), *(i - 1))) { *--o = std::move(*--i); ++cx; cy = 0; }
                        else                        { *--o = std::move(*--j); ++cy; cx = 0; }
                    }
                }
                while (i > il && j > buf_) {
                    // cola de Y que no es menor que la cola de X: va al final
                    std::size_t k = detail::gallop(static_cast<std::size_t>(j - buf_), [&](std::size_t t) { return !lt(*(j - 1 - t), *(i - 1)); });
                    o = std::move_backward(j - k, j, o); j -= k;
                    if (j == buf_) break;
                    // cola de X estrictamente mayor que la cola de Y
                    std::size_t k2 = detail::gallop(static_cast<std::size_t>(i - il), [&](std::size_t t) { return lt(*(j - 1), *(i - 1 - t)); });
                    o = std::move_backward(i - k2, i, o); i -= k2;   // o > i: hacia atrás
                    if (k < MG && k2 < MG) break;
                }
            }
            std::move_backward(buf_, j, o);
            return;
        }
        if (mode == 1) {
            while (i > il && j > buf_) { if (lt(*(j - 1), *(i - 1))) *--o = std::move(*--i); else *--o = std::move(*--j); }
        } else {
            while (i > il && j > buf_) {
                bool t = lt(*(j - 1), *(i - 1));
                *--o = std::move(t ? *(i - 1) : *(j - 1));
                i -= t; j -= !t;
            }
        }
        std::move_backward(buf_, j, o);
    }

    // Parity merge con largos distintos (solo tipos trivialmente copiables):
    // X[0..na) y Y[0..nb) -> D, sin superposición. Las primeras min(na, nb)
    // iteraciones no chequean bordes: ninguna punta agota un run antes.
    void parity_unequal(const T* X, std::size_t na, const T* Y, std::size_t nb, T* D) {
        const T *pl = X, *pr = Y, *tl = X + na - 1, *tr = Y + nb - 1;
        T *dh = D, *dt = D + na + nb - 1;
        for (std::size_t k = std::min(na, nb); k; --k) {
            bool x = lt(*pr, *pl);
            *dh++ = *detail::sel(x, pr, pl); pr += x; pl += !x;
            bool y = lt(*tr, *tl);
            *dt-- = *detail::sel(y, tl, tr); tl -= y; tr -= !y;
        }
        const T *le = tl + 1, *re = tr + 1;
        while (pl < le && pr < re) {
            bool x = lt(*pr, *pl);
            *dh++ = *detail::sel(x, pr, pl); pr += x; pl += !x;
        }
        while (pl < le) *dh++ = *pl++;
        while (pr < re) *dh++ = *pr++;
    }

    // SymMerge iterativo con hojas en el buffer.
    void sym_merge(std::size_t l, std::size_t m, std::size_t r) {
        struct W { std::size_t l, m, r; } st[STACK];
        int sp = 0;
        st[sp++] = {l, m, r};
        const std::size_t B = B_;
        while (sp) {
            W w = st[--sp];
            l = w.l; m = w.m; r = w.r;
            if (l >= m || m >= r || !lt(a_[m], a_[m - 1])) continue;
            l = upper_bound(l, m, a_[m]);
            r = lower_bound(m, r, a_[m - 1]);
            std::size_t nl = m - l, nr = r - m;
            // Elección de la mezcla. Tipos trivialmente copiables en el camino de runs naturales:
            // una sonda (v8-r) estima las rachas. 8 muestras de X ubicadas en Y; en un hueco con
            // gx elementos de X y gy de Y hay a lo sumo 2*min(gx,gy)+1 cambios de lado. Si la
            // cota da rachas de 8 o más en promedio: galope. Si no: parity merge cuando todo
            // entra en el buffer, o mezcla con branch. Todas las mezclas son estables y
            // correctas: la sonda solo afecta el tiempo.
            int mode = adaptive_ ? (gallop_ ? 2 : 1) : 0;
            if constexpr (TRIV) {
                bool par = true;
                if (adaptive_ && nl + nr >= 256) {
                    std::size_t prevy = m, prevx = l, sw = 0;
                    for (std::size_t i = 1; i <= 9; ++i) {
                        std::size_t px = i < 9 ? l + i * nl / 9 : m;
                        std::size_t py = i < 9 ? lower_bound(prevy, r, a_[px]) : r;
                        sw += 2 * std::min(px - prevx, py - prevy) + 1;
                        prevx = px; prevy = py;
                    }
                    par = sw * 8 > nl + nr;
                }
                if (par && nl + nr <= B) {
                    std::copy(a_ + l, a_ + r, buf_);
                    parity_unequal(buf_, nl, buf_ + nl, nr, a_ + l);
                    continue;
                }
                mode = par ? (adaptive_ ? 1 : 0) : 2;   // intercalado que no entra en el buffer: con branch (patrones regulares)
            }
            if (nl <= nr && nl <= B) { merge_left_buf(l, m, r, mode); continue; }
            if (nr <= B) { merge_right_buf(l, m, r, mode); continue; }
            if (nl <= B) { merge_left_buf(l, m, r, mode); continue; }
#ifndef SOT_NO_PAGES
            if (page_ok(nl + nr)) { page_merge(l, m, r); continue; }
#endif
            std::size_t m1, m2;
            if (nl >= nr) { m1 = l + (nl >> 1); m2 = lower_bound(m, r, a_[m1]); }
            else          { m2 = m + (nr >> 1); m1 = upper_bound(l, m, a_[m2]); }
            rotate(m1, m, m2);
            std::size_t nm = m1 + (m2 - m);
            if ((nm - l) > (r - nm)) { st[sp++] = {l, m1, nm}; st[sp++] = {nm, m2, r}; }   // el más grande abajo
            else                     { st[sp++] = {nm, m2, r}; st[sp++] = {l, m1, nm}; }
        }
    }

    // ---------------------------------------------------------- hojas
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
    template <std::size_t W>
    void parity_fixed(const T* l, const T* r, T* d) {
        const T *pl = l, *pr = r, *tl = l + W - 1, *tr = r + W - 1;
        T *dh = d, *dt = d + 2 * W - 1;
        for (std::size_t k = 0; k < W; ++k) {
            bool x = lt(*pr, *pl);
            *dh++ = *detail::sel(x, pr, pl); pr += x; pl += !x;
            bool y = lt(*tr, *tl);
            *dt-- = *detail::sel(y, tl, tr); tl -= y; tr -= !y;
        }
    }
    void parity_merge(const T* l, const T* r, std::size_t n, T* d) {
        const T *pl = l, *pr = r, *tl = l + n - 1, *tr = r + n - 1;
        T *dh = d, *dt = d + 2 * n - 1;
        for (std::size_t k = 0; k < n; ++k) {
            bool x = lt(*pr, *pl);
            *dh++ = *detail::sel(x, pr, pl); pr += x; pl += !x;
            bool y = lt(*tr, *tl);
            *dt-- = *detail::sel(y, tl, tr); tl -= y; tr -= !y;
        }
    }
    // Trozo potencia de 2 (c <= LEAF): pares sin branch y niveles de parity
    // merge alternando arreglo <-> buffer; el último nivel cae en p.
    void ping_pong_pow2(T* p, std::size_t c) {
        std::size_t L = 0;
        for (std::size_t w = 2; w < c; w <<= 1) ++L;
        T* out = (L & 1) ? buf_ : p;
        for (std::size_t i = 0; i < c; i += 2) {
            T x = p[i], y = p[i + 1];
            bool s = lt(y, x);
            out[i] = *detail::sel(s, &y, &x); out[i + 1] = *detail::sel(s, &x, &y);
        }
        T *src = out, *dst = (out == p) ? buf_ : p;
        for (std::size_t w = 2; w < c; w <<= 1) {
            switch (w) {
                case 2:  for (std::size_t i = 0; i < c; i += 4)  parity_fixed<2>(src + i, src + i + 2, dst + i); break;
                case 4:  for (std::size_t i = 0; i < c; i += 8)  parity_fixed<4>(src + i, src + i + 4, dst + i); break;
                case 8:  for (std::size_t i = 0; i < c; i += 16) parity_fixed<8>(src + i, src + i + 8, dst + i); break;
                case 16: for (std::size_t i = 0; i < c; i += 32) parity_fixed<16>(src + i, src + i + 16, dst + i); break;
                default: for (std::size_t i = 0; i < c; i += 2 * w) parity_merge(src + i, src + i + w, w, dst + i);
            }
            std::swap(src, dst);
        }
    }
    // Hoja genérica (tipos no triviales): ping-pong con movimientos.
    void ping_pong_moves(T* p, std::size_t n) {
        for (std::size_t i = 0; i + 1 < n; i += 2)
            if (lt(p[i + 1], p[i])) std::swap(p[i], p[i + 1]);
        T *src = p, *dst = buf_;
        for (std::size_t w = 2; w < n; w <<= 1) {
            std::size_t i = 0;
            for (; i + w < n; i += 2 * w) merge_to(src + i, w, src + i + w, std::min(w, n - i - w), dst + i);
            if (i < n) std::move(src + i, src + n, dst + i);
            std::swap(src, dst);
        }
        if (src != p) std::move(src, src + n, p);
    }
    // Hoja: hi - lo <= LEAF. Trozos potencia de 2, de derecha (chico) a
    // izquierda (grande), unidos con parity merges por el buffer.
    void leaf_sort(std::size_t lo, std::size_t hi) {
        std::size_t n = hi - lo;
        if (n < 2) return;
        if constexpr (TRIV) {
            std::size_t end = hi, rest = n;
            bool first = true;
            while (rest) {
                std::size_t c = rest & (~rest + 1);
                std::size_t s = end - c;
                if (c > 1) ping_pong_pow2(a_ + s, c);
                if (!first && lt(a_[s + c], a_[s + c - 1])) {
                    std::size_t tot = hi - s;
                    parity_unequal(a_ + s, c, a_ + s + c, tot - c, buf_);
                    std::copy(buf_, buf_ + tot, a_ + s);
                }
                first = false; end = s; rest -= c;
            }
        } else {
            ping_pong_moves(a_ + lo, n);
        }
    }

    // Respaldo (presupuesto agotado): inserción + SymMerge de abajo hacia arriba.
    void merge_sort(std::size_t lo, std::size_t hi) {
        for (std::size_t s = lo; s < hi; s += RUN) insertion(s, std::min(s + RUN, hi));
        for (std::size_t w = RUN; w < hi - lo; w <<= 1)
            for (std::size_t l = lo; l + w < hi; l += 2 * w)
                sym_merge(l, l + w, std::min(l + 2 * w, hi));
    }

    // ------------------------------------- runs naturales (criterio de potencias)
    static int powerloop(std::size_t s1, std::size_t n1, std::size_t n2, std::size_t nn) {
        int r = 0;
        std::size_t x = 2 * s1 + n1, y = x + n1 + n2;
        for (;;) {
            ++r;
            if (x >= nn) { x -= nn; y -= nn; }
            else if (y >= nn) break;
            x *= 2; y *= 2;
        }
        return r;
    }
    // ------------------------------------------------- runs paginados (v8-r5)
    // Esquema de Virtual-Memory Powersort (Moltmann, Nakajima, Wild, ESA 2026) dentro del camino
    // de runs naturales de SotSort, cuando el buffer es chico (B < n/2). Cada run, apenas se
    // detecta, se copia a páginas de P elementos; un run es una lista de páginas (solo la última
    // parcial). Las mezclas van de página a página, liberando páginas de entrada y tomando
    // páginas libres para la salida, sin permutar. Al final la lista única se vuelve al arreglo.
    // Páginas físicas: las páginas completas del arreglo (que se liberan a medida que los runs
    // se copian) y K = ⌈lg n⌉ + 8 páginas del buffer. Movimientos: 1 (copia inicial) + 1 por
    // nivel de mezcla + a lo sumo 1 (vuelta al arreglo; las páginas que ya están en su lugar
    // no se mueven).
    // Lugar: en uso hay a lo sumo Σ⌈len/P⌉ ≤ n/P + runs páginas más 3 transitorias de la mezcla;
    // hay ⌊n/P⌋ del arreglo más K del buffer, y la pila de powersort tiene a lo sumo ⌈lg n⌉ + 2 runs.
    bool paging_ = false;
    std::vector<std::uint32_t> pr_stk_, pr_tmp_, pr_pos_, pr_fr_, pr_cur_;
    std::size_t paged_K(std::size_t n) const { std::size_t k = 0; while ((std::size_t(1) << k) < n) ++k; return k + 8; }
    std::size_t paged_P(std::size_t n) const {
        std::size_t q = B_ / paged_K(n), p = 1;
        while (p * 2 <= q) p *= 2;
        return p;
    }
    // ¿Se puede paginar? Solo con un buffer reservado aparte: en modo O(1) (buffer propio) la
    // tabla de páginas rompería la cota de memoria y las zonas usan el buffer propio.
    bool paged_ok(std::size_t n) const {
        // Con un buffer grande (más de n/8) casi todas las mezclas entran en él y la copia en
        // bloque al buffer es barata: paginar (dos pasadas más y runs repartidos) midió peor.
        if (buf_ == own_ || n < 4096 || B_ * 8 > n) return false;
        const std::size_t P = paged_P(n);
        if (P < 64) return false;
        const std::size_t pages = n / P + paged_K(n) + 2;
        if (pages >= (std::size_t(1) << 31)) return false;
        return pages * sizeof(std::uint32_t) * 5 <= B_ * sizeof(T) / 8 + 4096;
    }
    // Decisión de paginar (natural_merge_sort detecta primero hasta PG_PRE runs): con pocos runs
    // copiar a páginas y volver cuesta más que mezclar directo, y con runs muy cortos (menos de
    // 32 en promedio) paginar midió peor con comparaciones caras.
    static constexpr int PG_PRE = 17;
    // Bucle de mezcla del camino paginado: k pasos sin cruzar bordes de página. Va aparte y con
    // flatten porque, dentro del lambda grande, gcc dejaba el comparador sin expandir (con
    // comparaciones caras costaba hasta un 30%).
#if defined(__GNUC__) && !defined(__clang__)
    __attribute__((flatten))
#endif
    T* merge_run_k(T*& x1, T*& y1, T* o1, std::size_t k) {
        T *x = x1, *y = y1;
        for (std::size_t t = 0; t < k; ++t) {
            if (lt(*y, *x)) *o1++ = std::move(*y++);
            else *o1++ = std::move(*x++);
        }
        x1 = x; y1 = y;
        return o1;
    }
    // Mezcla hacia atrás de X (aparte) e Y, dejando el resultado en Y[0, lx+ly): Y tiene lugar.
#if defined(__GNUC__) && !defined(__clang__)
    __attribute__((flatten))
#endif
    void merge_back_into(T* xa, std::size_t lx, T* ya, std::size_t ly) {
        T *xb = xa + lx, *yb = ya + ly, *o = ya + lx + ly;
        while (xb != xa && yb != ya) { if (lt(*(yb - 1), *(xb - 1))) *--o = std::move(*--xb); else *--o = std::move(*--yb); }
        while (xb != xa) *--o = std::move(*--xb);
    }
    void paged_natural_merge_sort(std::size_t lo, std::size_t hi, std::size_t known, const std::size_t (*pre)[2], int npre) {
        const std::size_t n = hi - lo, K = paged_K(n), P = paged_P(n), NA = n / P;
        T* const A = a_ + lo;
        auto base = [&](std::uint32_t ph) -> T* { return ph < NA ? A + std::size_t(ph) * P : buf_ + std::size_t(ph - NA) * P; };
        auto npg = [&](std::size_t len) { return (len + P - 1) / P; };
        pr_fr_.clear(); pr_stk_.clear();
        for (std::size_t k = K; k-- > 0;) pr_fr_.push_back(static_cast<std::uint32_t>(NA + k));
        auto pop = [&]() { std::uint32_t ph = pr_fr_.back(); pr_fr_.pop_back(); return ph; };
        struct Run { std::size_t s, len, off; int power; } st[STACK];
        int top = 0;
        std::size_t af = 0;                                     // próxima página del arreglo a liberar
        paging_ = true;
        auto merge_top = [&]() {
            Run& X = st[top - 2]; Run& Y = st[top - 1];
            const std::size_t lx = X.len, ly = Y.len, px = npg(lx), py = npg(ly);
            std::uint32_t* xs = pr_stk_.data() + X.off;
            std::uint32_t* ys = pr_stk_.data() + Y.off;
            if (lx + ly <= P && px == 1 && py == 1) {           // cabe en una página: mezcla hacia atrás en la de Y
                merge_back_into(base(xs[0]), lx, base(ys[0]), ly);
                pr_fr_.push_back(xs[0]);
                xs[0] = ys[0];
                X.len += ly; --top; pr_stk_.resize(X.off + 1);
                return;
            }
            pr_tmp_.clear();
            std::size_t ix = 0, iy = 0, rx = lx, ry = ly;
            T *x1 = base(xs[0]), *x2 = x1 + std::min(rx, P), *y1 = base(ys[0]), *y2 = y1 + std::min(ry, P);
            T *o1 = nullptr, *o2 = nullptr;
            while (rx || ry) {
                if (o1 == o2) { std::uint32_t ph = pop(); pr_tmp_.push_back(ph); o1 = base(ph); o2 = o1 + P; }
                std::size_t k = static_cast<std::size_t>(o2 - o1);
                if (rx && ry) {
                    k = std::min(k, std::min(static_cast<std::size_t>(x2 - x1), static_cast<std::size_t>(y2 - y1)));
                    T *xb = x1, *yb = y1;
                    o1 = merge_run_k(x1, y1, o1, k);
                    rx -= static_cast<std::size_t>(x1 - xb); ry -= static_cast<std::size_t>(y1 - yb);
                } else if (rx) {
                    k = std::min(k, static_cast<std::size_t>(x2 - x1));
                    o1 = std::move(x1, x1 + k, o1); x1 += k; rx -= k;
                } else {
                    k = std::min(k, static_cast<std::size_t>(y2 - y1));
                    o1 = std::move(y1, y1 + k, o1); y1 += k; ry -= k;
                }
                if (x1 == x2 && ix < px) {
                    pr_fr_.push_back(xs[ix++]);
                    if (rx) { x1 = base(xs[ix]); x2 = x1 + std::min(rx, P); }
                }
                if (y1 == y2 && iy < py) {
                    pr_fr_.push_back(ys[iy++]);
                    if (ry) { y1 = base(ys[iy]); y2 = y1 + std::min(ry, P); }
                }
            }
            X.len += ly; --top;
            pr_stk_.resize(X.off);
            pr_stk_.insert(pr_stk_.end(), pr_tmp_.begin(), pr_tmp_.end());
        };
        std::size_t i = lo;
        int ip = 0;
        while (i < hi) {
            std::size_t j;
            if (ip < npre) j = pre[ip++][1];
            else {
            // Misma detección que next_run, escrita en línea: como llamada, gcc generaba un bucle
            // de inserción más lento (~10% con comparaciones caras).
            if (i == lo && known > lo + 1) j = known;
            else if (i + 1 < hi && lt(a_[i + 1], a_[i])) {
                j = i + 2;
                while (j < hi && lt(a_[j], a_[j - 1])) ++j;
                std::reverse(a_ + i, a_ + j);              // estrictamente descendente: invertir es estable
            } else {
                j = i + 1;
                while (j < hi && !lt(a_[j], a_[j - 1])) ++j;
            }
            if (j - i < RUN) {
    #ifndef SOT_NO_ZONES
                // Zona desordenada (v8-r2): 8 o más runs naturales seguidos de menos de 8
                // elementos. Se extiende hasta el primer run de 16 o más y se ordena con el
                // quicksort estable: queda como un solo run.
                std::size_t e = j, tiny = 1;
                while (TRIV && e < hi) {            // solo tipos triviales: con strings midió peor
                    std::size_t f = e + 1;
                    const std::size_t cap = std::min(hi, e + 16);   // alcanza con saber si llega a 16
                    if (f < hi && lt(a_[f], a_[e])) { ++f; while (f < cap && lt(a_[f], a_[f - 1])) ++f; }
                    else { while (f < cap && !lt(a_[f], a_[f - 1])) ++f; }
                    if (f - e >= (tiny >= 8 ? 16u : 8u)) break;
                    e = f; ++tiny;
                }
                if (tiny >= 8) {
                    int budget = 0;
                    for (std::size_t k = e - i; k; k >>= 1) ++budget;
                    // En el camino paginado el buffer guarda páginas: la zona usa el buffer propio.
                    T* sb = buf_; std::size_t sB = B_;
                    if (paging_) { buf_ = own_; B_ = B0; }
                    adaptive_ = false;
                    quick_from(i, e, budget, false, T{}, nullptr, 0);
                    adaptive_ = true;
                    buf_ = sb; B_ = sB;
                    j = e;
                } else
    #endif
                { std::size_t e2 = std::min(hi, i + RUN); insertion(i, e2); j = e2; }
            }
            }
            // copiar [i, j) a páginas y liberar las páginas completas del arreglo ya copiadas
            pr_cur_.clear();
            for (std::size_t t = i; t < j; t += P) {
                std::size_t e = std::min(t + P, j);
                std::uint32_t ph = pop();
                std::move(a_ + t, a_ + e, base(ph));
                pr_cur_.push_back(ph);
                while (af < NA && lo + (af + 1) * P <= e) pr_fr_.push_back(static_cast<std::uint32_t>(af++));
            }
            int p = 0;
            if (top > 0) {
                p = powerloop(st[top - 1].s - lo, st[top - 1].len, j - i, n);
                while (top > 1 && st[top - 2].power > p) merge_top();
                st[top - 1].power = p;
            }
            st[top++] = {i, j - i, pr_stk_.size(), 0};
            pr_stk_.insert(pr_stk_.end(), pr_cur_.begin(), pr_cur_.end());
            i = j;
        }
        while (top > 1) merge_top();
        // Vuelta al arreglo: la página lógica q va a A + q·P (la última, parcial, a la cola).
        const std::size_t L = npg(n);
        std::uint32_t* pg = pr_stk_.data() + st[0].off;
        pr_pos_.assign(NA + K, 0xFFFFFFFFu);                    // física -> lógica
        for (std::size_t q = 0; q < L; ++q) pr_pos_[pg[q]] = static_cast<std::uint32_t>(q);
        for (std::size_t q = 0; q < L; ++q) {
            const std::size_t len = std::min(P, n - q * P);
            if (q < NA) {
                if (pg[q] == q) continue;                       // ya está en su lugar
                std::uint32_t occ = pr_pos_[q];
                if (occ != 0xFFFFFFFFu) {                       // la ocupa otra lógica (posterior): sacarla
                    // destino: una página libre del buffer o del arreglo todavía sin destino final (> q)
                    std::uint32_t f = static_cast<std::uint32_t>(NA + K);
                    while (f-- > q + 1) if (pr_pos_[f] == 0xFFFFFFFFu) break;
                    std::move(base(static_cast<std::uint32_t>(q)), base(static_cast<std::uint32_t>(q)) + std::min(P, n - std::size_t(occ) * P), base(f));
                    pg[occ] = f; pr_pos_[f] = occ;
                }
            }
            std::uint32_t src = pg[q];
            std::move(base(src), base(src) + len, A + q * P);
            pr_pos_[src] = 0xFFFFFFFFu;
            if (q < NA) { pr_pos_[q] = static_cast<std::uint32_t>(q); pg[q] = static_cast<std::uint32_t>(q); }
        }
        paging_ = false;
    }

    // Detecta el run que empieza en i (o usa known), lo deja ordenado en [i, j) y devuelve j.
    // Descendentes estrictos se invierten; runs cortos se extienden (inserción o zona).
    std::size_t next_run(std::size_t i, std::size_t lo, std::size_t hi, std::size_t known) {
        std::size_t j;
        if (i == lo && known > lo + 1) j = known;
        else if (i + 1 < hi && lt(a_[i + 1], a_[i])) {
            j = i + 2;
            while (j < hi && lt(a_[j], a_[j - 1])) ++j;
            std::reverse(a_ + i, a_ + j);              // estrictamente descendente: invertir es estable
        } else {
            j = i + 1;
            while (j < hi && !lt(a_[j], a_[j - 1])) ++j;
        }
        if (j - i < RUN) {
#ifndef SOT_NO_ZONES
            // Zona desordenada (v8-r2): 8 o más runs naturales seguidos de menos de 8
            // elementos. Se extiende hasta el primer run de 16 o más y se ordena con el
            // quicksort estable: queda como un solo run.
            std::size_t e = j, tiny = 1;
            while (TRIV && e < hi) {            // solo tipos triviales: con strings midió peor
                std::size_t f = e + 1;
                const std::size_t cap = std::min(hi, e + 16);   // alcanza con saber si llega a 16
                if (f < hi && lt(a_[f], a_[e])) { ++f; while (f < cap && lt(a_[f], a_[f - 1])) ++f; }
                else { while (f < cap && !lt(a_[f], a_[f - 1])) ++f; }
                if (f - e >= (tiny >= 8 ? 16u : 8u)) break;
                e = f; ++tiny;
            }
            if (tiny >= 8) {
                int budget = 0;
                for (std::size_t k = e - i; k; k >>= 1) ++budget;
                // En el camino paginado el buffer guarda páginas: la zona usa el buffer propio.
                T* sb = buf_; std::size_t sB = B_;
                if (paging_) { buf_ = own_; B_ = B0; }
                adaptive_ = false;
                quick_from(i, e, budget, false, T{}, nullptr, 0);
                adaptive_ = true;
                buf_ = sb; B_ = sB;
                j = e;
            } else
#endif
            { std::size_t e2 = std::min(hi, i + RUN); insertion(i, e2); j = e2; }
        }
        return j;
    }

    void natural_merge_sort(std::size_t lo, std::size_t hi, std::size_t known) {
        std::size_t pre[PG_PRE][2];
        int npre = 0;
#ifndef SOT_NO_PAGED_RUNS
        if (paged_ok(hi - lo)) {
            std::size_t i = lo;
            while (i < hi && npre < PG_PRE) { std::size_t j = next_run(i, lo, hi, known); pre[npre][0] = i; pre[npre][1] = j; ++npre; i = j; }
            if (npre == PG_PRE && i < hi && (i - lo) / PG_PRE >= 32) { paged_natural_merge_sort(lo, hi, known, pre, npre); return; }
        }
#endif
        struct Run { std::size_t s, len; int power; } st[STACK];
        int top = 0;
        const std::size_t nn = hi - lo;
        std::size_t i = lo;
        auto merge_top = [&]() {
            Run& X = st[top - 2]; Run& Y = st[top - 1];
            sym_merge(X.s, X.s + X.len, Y.s + Y.len);
            X.len += Y.len; --top;
        };
        int ip = 0;
        while (i < hi) {
            std::size_t j;
            if (ip < npre) j = pre[ip++][1];
            else {
            // Misma detección que next_run, escrita en línea: como llamada, gcc generaba un bucle
            // de inserción más lento (~10% con comparaciones caras).
            if (i == lo && known > lo + 1) j = known;
            else if (i + 1 < hi && lt(a_[i + 1], a_[i])) {
                j = i + 2;
                while (j < hi && lt(a_[j], a_[j - 1])) ++j;
                std::reverse(a_ + i, a_ + j);              // estrictamente descendente: invertir es estable
            } else {
                j = i + 1;
                while (j < hi && !lt(a_[j], a_[j - 1])) ++j;
            }
            if (j - i < RUN) {
    #ifndef SOT_NO_ZONES
                // Zona desordenada (v8-r2): 8 o más runs naturales seguidos de menos de 8
                // elementos. Se extiende hasta el primer run de 16 o más y se ordena con el
                // quicksort estable: queda como un solo run.
                std::size_t e = j, tiny = 1;
                while (TRIV && e < hi) {            // solo tipos triviales: con strings midió peor
                    std::size_t f = e + 1;
                    const std::size_t cap = std::min(hi, e + 16);   // alcanza con saber si llega a 16
                    if (f < hi && lt(a_[f], a_[e])) { ++f; while (f < cap && lt(a_[f], a_[f - 1])) ++f; }
                    else { while (f < cap && !lt(a_[f], a_[f - 1])) ++f; }
                    if (f - e >= (tiny >= 8 ? 16u : 8u)) break;
                    e = f; ++tiny;
                }
                if (tiny >= 8) {
                    int budget = 0;
                    for (std::size_t k = e - i; k; k >>= 1) ++budget;
                    // En el camino paginado el buffer guarda páginas: la zona usa el buffer propio.
                    T* sb = buf_; std::size_t sB = B_;
                    if (paging_) { buf_ = own_; B_ = B0; }
                    adaptive_ = false;
                    quick_from(i, e, budget, false, T{}, nullptr, 0);
                    adaptive_ = true;
                    buf_ = sb; B_ = sB;
                    j = e;
                } else
    #endif
                { std::size_t e2 = std::min(hi, i + RUN); insertion(i, e2); j = e2; }
            }
            }
            if (top > 0) {
                int p = powerloop(st[top - 1].s - lo, st[top - 1].len, j - i, nn);
                while (top > 1 && st[top - 2].power > p) merge_top();
                st[top - 1].power = p;
            }
            st[top++] = {i, j - i, 0};
            i = j;
        }
        while (top > 1) merge_top();
    }

    // -------------------------------------------- partición estable
    // Bloque (hi - lo <= B): [x cumple | x no cumple], estable. Predicado:
    // x < piv (le = false) o x <= piv (le = true).
    template <class P>
    std::size_t part_block_scalar(T* p, std::size_t len, std::size_t i, std::size_t m, P pred, T* buf, bool back) {
        if constexpr (TRIV) {
            for (; i < len; ++i) {                 // sin branch: escribe en ambos destinos
                T x = p[i];
                bool t = pred(x);
                buf[i - m] = x;
                p[m] = x;
                m += t;
            }
            if (back) std::copy(buf, buf + (len - m), p + m);
            return m;
        } else {
            std::size_t k = i - m;                 // (en este camino i = m = 0)
            for (; i < len; ++i) {
                if (pred(p[i])) { if (m != i) p[m] = std::move(p[i]); ++m; }
                else buf[k++] = std::move(p[i]);
            }
            if (back) std::move(buf, buf + k, p + m);
            return m;
        }
    }
    // Con back = false, los que no cumplen quedan en buf[0 .. len - m) y no se copian de vuelta.
    std::size_t part_block_to(std::size_t lo, std::size_t hi, const T& piv, bool le, T* buf, bool back) {
        T* p = a_ + lo;
        std::size_t len = hi - lo, i = 0, m = 0;
        if constexpr (SIMD) m = detail::simd_part(p, len, buf, piv, le, i);
        if (le) m = part_block_scalar(p, len, i, m, [&](const T& x) { return !lt(piv, x); }, buf, back);
        else    m = part_block_scalar(p, len, i, m, [&](const T& x) { return lt(x, piv); }, buf, back);
        return lo + m;
    }
    std::size_t part_block(std::size_t lo, std::size_t hi, const T& piv, bool le) {
        return part_block_to(lo, hi, piv, le, buf_, true);
    }
#ifdef SOT_PAGED_PART
    // Partición estable por páginas (experimental, docs/IDEA_particion_paginada.md).
    // Páginas de P = B/2; ≤ 4.5 movimientos por elemento para cualquier B. Las tablas de bits
    // (O(m/P)) se piden al heap: todavía no entran en el presupuesto de memoria.
    std::size_t paged_partition(std::size_t lo, std::size_t hi, const T& piv, bool le) {
        T* a = a_ + lo;
        const std::size_t m = hi - lo, P = B_ / 2;
        T* LB = buf_; T* RB = buf_ + P;
        std::size_t nl = 0, nr = 0, w = 0;
        const std::size_t words = m / P / 64 + 2;
        std::unique_ptr<std::uint64_t[]> side(new std::uint64_t[words]()), done(new std::uint64_t[words]());
        std::unique_ptr<std::size_t[]> pre(new std::size_t[words + 1]);
        for (std::size_t r = 0; r < m; ++r) {
            bool left = le ? !lt(piv, a[r]) : lt(a[r], piv);
            if (left) {
                LB[nl++] = std::move(a[r]);
                if (nl == P) { std::move(LB, LB + P, a + w * P); ++w; nl = 0; }
            } else {
                RB[nr++] = std::move(a[r]);
                if (nr == P) { std::move(RB, RB + P, a + w * P); side[w / 64] |= 1ull << (w % 64); ++w; nr = 0; }
            }
        }
        std::move(LB, LB + nl, a + w * P);
        std::move(RB, RB + nr, a + w * P + nl);
        pre[0] = 0;
        for (std::size_t j = 0; j < words; ++j) pre[j + 1] = pre[j] + static_cast<std::size_t>(__builtin_popcountll(side[j]));
        auto rights_before = [&](std::size_t i) {
            std::uint64_t mask = (i % 64) ? (side[i / 64] & ((1ull << (i % 64)) - 1)) : 0;
            return pre[i / 64] + static_cast<std::size_t>(__builtin_popcountll(mask));
        };
        const std::size_t b = rights_before(w), aL = w - b;
        auto select = [&](std::size_t k, bool right) {
            std::size_t l0 = 0, h0 = words;
            auto cnt = [&](std::size_t j) { return right ? pre[j] : j * 64 - pre[j]; };
            while (h0 - l0 > 1) { std::size_t md = (l0 + h0) / 2; if (cnt(md) <= k) l0 = md; else h0 = md; }
            std::size_t r = k - cnt(l0);
            std::uint64_t word = right ? side[l0] : ~side[l0];
            for (;;) { std::size_t z = static_cast<std::size_t>(__builtin_ctzll(word)); if (!r) return l0 * 64 + z; word &= word - 1; --r; }
        };
        auto src = [&](std::size_t j) { return j < aL ? select(j, false) : select(j - aL, true); };
        for (std::size_t i = 0; i < w; ++i) {
            if ((done[i / 64] >> (i % 64)) & 1) continue;
            if (src(i) == i) { done[i / 64] |= 1ull << (i % 64); continue; }
            std::move(a + i * P, a + (i + 1) * P, buf_);
            std::size_t hole = i;
            for (;;) {
                done[hole / 64] |= 1ull << (hole % 64);
                std::size_t s2 = src(hole);
                if (s2 == i) { std::move(buf_, buf_ + P, a + hole * P); break; }
                std::move(a + s2 * P, a + (s2 + 1) * P, a + hole * P);
                hole = s2;
            }
        }
        const std::size_t s0 = aL * P, e = w * P;
        if (nl && e > s0) {                   // sin parcial izquierda no hay nada que correr (y no autoasignar)
            std::move(a + e, a + e + nl, buf_);
            std::move_backward(a + s0, a + e, a + e + nl);
            std::move(buf_, buf_ + nl, a + s0);
        }
        return lo + s0 + nl;
    }
#endif
    // Gather en orden de contador binario; el corte de cada segmento viaja en la pila.
    std::size_t stable_partition(std::size_t lo, std::size_t hi, const T& piv, bool le) {
#ifdef SOT_PAGED_PART
        if (hi - lo > 8 * B_ && B_ / 2 >= 64) return paged_partition(lo, hi, piv, le);
#endif
        struct S { std::size_t s, sp; int lvl; } st[70];
        int top = 0;
        const std::size_t B = B_;
        auto fuse = [&]() {
            S& L = st[top - 2]; S& R = st[top - 1];
            std::size_t mid = R.s;
            rotate(L.sp, mid, R.sp);
            L.sp = L.sp + (R.sp - mid);
            L.lvl = std::max(L.lvl, R.lvl) + 1;
            --top;
        };
        for (std::size_t s = lo; s < hi; s += B) {
            std::size_t e = (hi - s > B) ? s + B : hi;
            std::size_t sp = part_block(s, e, piv, le);
            st[top++] = {s, sp, 0};
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
    // Una copia, no un elemento: no toca el arreglo. Muestreo estratificado con
    // offsets pseudoaleatorios (v7): una entrada armada contra posiciones fijas
    // deja de servir mientras no se conozca la semilla.
    T pivot_value(std::size_t lo, std::size_t n) {
        if (n <= N9) {
            std::size_t s = n / 9;                 // n > LEAF: s >= 14
            std::size_t q[9];
            for (std::size_t j = 0; j < 9; ++j) q[j] = lo + j * s + static_cast<std::size_t>(rnd() % s);
            std::size_t x = med3(q[0], q[1], q[2]);
            std::size_t y = med3(q[3], q[4], q[5]);
            std::size_t z = med3(q[6], q[7], q[8]);
            return a_[med3(x, y, z)];
        }
        std::size_t s = (2 * static_cast<std::size_t>(std::cbrt(static_cast<double>(n)))) | 1;
        if (s > B_ - 1) s = (B_ - 1) | 1;
        if (s > B_) s = B_ - 2;
        std::size_t step = n / s;
        for (std::size_t i = 0; i < s; ++i) buf_[i] = a_[lo + i * step + static_cast<std::size_t>(rnd() % step)];
        std::nth_element(buf_, buf_ + s / 2, buf_ + s, [&](const T& x, const T& y) { return lt(x, y); });
        return buf_[s / 2];
    }

    // ------------------------------------------------------ quicksort estable
    void quick(std::size_t n) {
        int budget = 0;
        for (std::size_t k = n; k; k >>= 1) ++budget;
        quick_from(0, n, budget, false, T{}, nullptr, 0);
    }
    void quick_from(std::size_t lo0, std::size_t hi0, int bad0, bool has0, const T& lb0, Spawner* spw, std::size_t grain) {
        struct R { std::size_t lo, hi; int bad; bool has; T lb; };
        std::unique_ptr<R[]> heap;                 // T grande: la pila de rangos no entra en el stack
        R local[sizeof(T) <= 64 ? STACK : 1];
        R* st = local;
        if (sizeof(T) > 64) {
            heap.reset(new R[STACK]);
            st = heap.get();
        }
        int sp = 0;
        st[sp].lo = lo0; st[sp].hi = hi0; st[sp].bad = bad0; st[sp].has = has0;
        if (has0) st[sp].lb = lb0;
        ++sp;
        T lb{};
        while (sp) {
            --sp;
            std::size_t lo = st[sp].lo, hi = st[sp].hi;
            int bad = st[sp].bad;
            bool has = st[sp].has;
            if (has) lb = std::move(st[sp].lb);
            for (;;) {
                std::size_t len = hi - lo;
                if (aux_base_) { buf_ = aux_base_ + lo; B_ = len; }
                if (len <= LEAF) { auto m0=MV; leaf_sort(lo, hi); PH[0]+=MV-m0; break; }
                if (bad <= 0) { if (!already_sorted(lo, hi)) merge_sort(lo, hi); break; }
                auto m2=MV; T piv = pivot_value(lo, len); PH[3]+=MV-m2;
                bool none_less = has && !lt(lb, piv);          // el pivote no supera la cota inferior
                std::size_t mid = lo;
                if (!none_less) {
                    { auto m0=MV; mid = stable_partition(lo, hi, piv, false); PH[1]+=MV-m0; }
                    none_less = (mid == lo);
                }
                if (none_less) {                               // separar los iguales al pivote: quedan terminados
                    auto m1=MV; std::size_t nlo = stable_partition(lo, hi, piv, true); PH[2]+=MV-m1;
#ifndef SOT_V6_BUG
                    if (nlo - lo < len / 16) --bad;            // v6.1: esta separación también consume presupuesto
#endif
                    lo = nlo;
                    continue;
                }
                std::size_t nl = mid - lo, nr = hi - mid;
                if (std::min(nl, nr) < len / 16) --bad;
                if (nl < nr) {                                 // seguir con el chico, apilar (o ceder) el grande
                    if (!(spw && nr >= grain && spw->spawn(mid, hi, bad, true, piv))) {
                        st[sp].lo = mid; st[sp].hi = hi; st[sp].bad = bad; st[sp].has = true; st[sp].lb = piv; ++sp;
                    }
                    hi = mid;
                } else {
                    if (!(spw && nl >= grain && spw->spawn(lo, mid, bad, has, lb))) {
                        st[sp].lo = lo; st[sp].hi = mid; st[sp].bad = bad; st[sp].has = has;
                        if (has) st[sp].lb = lb;
                        ++sp;
                    }
                    lo = mid; lb = std::move(piv); has = true;
                }
            }
        }
    }
};

// ===================================================================== modo O(1)
template <class T, class Cmp = std::less<T>>
void stable_sort(T* a, std::size_t n, Cmp c = Cmp()) {
    if (n < 2) return;
    Sorter<T, Cmp> s(a, c);
    s.seed(detail::SIGMA ^ n);
    s.sort(n);
}

// ===================================================================== memoria
// Memoria disponible: min(MemAvailable, margen del cgroup). Devuelve false si
// no se pudo saber. Limitación: solo mira el cgroup raíz del espacio de nombres.
inline bool available_memory_bytes(std::size_t& out) {
    bool known = false;
    unsigned long long best = ~0ull;
#if defined(__linux__)
    auto read_ull = [](const char* path, unsigned long long& v, bool& is_max) {
        is_max = false;
        std::FILE* f = std::fopen(path, "r");
        if (!f) return false;
        char s[64] = {0};
        bool ok = std::fgets(s, sizeof s, f) != nullptr;
        std::fclose(f);
        if (!ok) return false;
        if (!std::strncmp(s, "max", 3)) { is_max = true; return true; }
        char* e = nullptr;
        v = std::strtoull(s, &e, 10);
        return e != s;
    };
    if (std::FILE* f = std::fopen("/proc/meminfo", "r")) {
        char line[256];
        while (std::fgets(line, sizeof line, f)) {
            unsigned long long kb;
            if (std::sscanf(line, "MemAvailable: %llu kB", &kb) == 1) { best = std::min(best, kb * 1024ull); known = true; break; }
        }
        std::fclose(f);
    }
    unsigned long long lim = 0, cur = 0;
    bool mx = false, mx2 = false;
    if (read_ull("/sys/fs/cgroup/memory.max", lim, mx) && !mx && read_ull("/sys/fs/cgroup/memory.current", cur, mx2)) {
        best = std::min(best, lim > cur ? lim - cur : 0ull); known = true;
    } else if (read_ull("/sys/fs/cgroup/memory/memory.limit_in_bytes", lim, mx) && lim < (1ull << 60) &&
               read_ull("/sys/fs/cgroup/memory/memory.usage_in_bytes", cur, mx2)) {
        best = std::min(best, lim > cur ? lim - cur : 0ull); known = true;
    }
#elif defined(_WIN32)
    MEMORYSTATUSEX ms; ms.dwLength = sizeof ms;
    if (GlobalMemoryStatusEx(&ms)) { best = ms.ullAvailPhys; known = true; }
#endif
    if (!known) return false;
    out = best > static_cast<unsigned long long>(static_cast<std::size_t>(-1)) ? static_cast<std::size_t>(-1) : static_cast<std::size_t>(best);
    return true;
}

template <class T>
Plan make_plan(std::size_t n, const Options& o) {
    const std::size_t s = sizeof(T), B0 = Sorter<T>::B0;
    Plan pl;
    pl.budget = o.max_bytes;
    const bool big = n > (std::size_t(1) << 20) / s;                 // n*s > 1 MB
    if (big && o.probe) {
        std::size_t D = 0;
        pl.memory_known = available_memory_bytes(D);
        std::size_t cap = pl.memory_known ? static_cast<std::size_t>(o.fraction * static_cast<double>(D))
                                          : (std::size_t(256) << 20);
        pl.budget = std::min(pl.budget, cap);
    }
    if (n <= B0) return pl;
    // v8-r5: el presupuesto cubre todo lo que se reserva: el buffer redondeado como lo reserva
    // Aux (64 bytes, o 2 MB desde 4 MB), el buffer propio del Sorter (B0 elementos) y, en modo
    // bloques, las tablas de páginas (a lo sumo 1/8 del buffer más 4 KB; ver page_ok y paged_ok).
    auto fits = [&](std::size_t E, bool tables) {
        if (E > (static_cast<std::size_t>(-1) >> 2) / s) return false;
        std::size_t bytes = E * s, al = bytes >= (std::size_t(4) << 20) ? (std::size_t(2) << 20) : 64;
        bytes = (bytes + al - 1) / al * al;
        std::size_t need = bytes + B0 * s + (tables ? E * s / 8 + 4096 : 0);
        return need <= pl.budget;
    };
    if (fits(n, false)) { pl.mode = Plan::Full; pl.E = n; }
    else {
        std::size_t b = pl.budget / s, E = detail::bit_floor(b);
        while (E > B0 && !fits(E, true)) E >>= 1;
        if (E > B0 && b >= 2 * B0) { pl.mode = Plan::Block; pl.E = E; }
    }
    if (pl.mode == Plan::Full && o.threads != 1) {
        unsigned h = o.threads ? o.threads : std::thread::hardware_concurrency();
        if (h < 1) h = 1;
        std::size_t lim = std::max<std::size_t>(1, n >> 17);
        unsigned P = 1;
        while (2 * P <= h && 2 * static_cast<std::size_t>(P) <= lim) P *= 2;
        pl.P = P;
    }
    return pl;
}

namespace detail {

// Buffer auxiliar: no lanza. >= 4 MB: alineado a 2 MB, redondeado a múltiplos
// de 2 MB y con MADV_HUGEPAGE.
template <class T>
struct Aux {
    T* p = nullptr;
    std::size_t n = 0;
    bool alloc(std::size_t count) {
        release();
        if (!count || count > static_cast<std::size_t>(-1) / sizeof(T) - (std::size_t(4) << 20)) return false;
        std::size_t bytes = count * sizeof(T), al = alignof(T) < 64 ? 64 : alignof(T);
        const bool huge = bytes >= (std::size_t(4) << 20);
        if (huge) al = std::size_t(2) << 20;
        bytes = (bytes + al - 1) / al * al;
        void* m = nullptr;
#ifdef SOT_TEST_ALLOC_HOOK
        if (!SOT_TEST_ALLOC_HOOK(bytes)) return false;
#endif
#if defined(_WIN32)
        m = _aligned_malloc(bytes, al);
#else
        if (posix_memalign(&m, al, bytes) != 0) m = nullptr;
#endif
        if (!m) return false;
#if defined(__linux__) && defined(MADV_HUGEPAGE)
        if (huge) (void)madvise(m, bytes, MADV_HUGEPAGE);
#endif
        p = static_cast<T*>(m);
        if (!std::is_trivially_default_constructible<T>::value) {
            try { std::uninitialized_default_construct_n(p, count); }
            catch (...) { free_raw(); return false; }
        }
        n = count;
        return true;
    }
    void release() {
        if (!p) return;
        if (!std::is_trivially_destructible<T>::value) std::destroy_n(p, n);
        free_raw();
    }
    ~Aux() { release(); }
    Aux() = default;
    Aux(const Aux&) = delete;
    Aux& operator=(const Aux&) = delete;
private:
    void free_raw() {
#if defined(_WIN32)
        _aligned_free(p);
#else
        std::free(p);
#endif
        p = nullptr; n = 0;
    }
};

// Conteo: T entero con su orden natural. El arreglo de cuentas (8(R+1) bytes)
// sale del presupuesto y se pide antes que el buffer: nunca coexisten.
template <class T>
bool int_shortcut(T* a, std::size_t n, std::size_t budget_bytes) {
    if (n < 4096) return false;
    T mn = a[0], mx = a[0];
    const std::size_t st = n / 1024;
    for (std::size_t i = 0; i < n; i += st) { if (a[i] < mn) mn = a[i]; if (mx < a[i]) mx = a[i]; }
    using U = std::make_unsigned_t<T>;
    if (static_cast<U>(static_cast<U>(mx) - static_cast<U>(mn)) >= 65536) return false;
    for (std::size_t i = 0; i < n; ++i) { if (a[i] < mn) mn = a[i]; if (mx < a[i]) mx = a[i]; }
    const std::uint64_t R = static_cast<U>(static_cast<U>(mx) - static_cast<U>(mn));
    if (R >= 65536 || (R + 1) * 8 > budget_bytes) return false;
    std::size_t* c = new (std::nothrow) std::size_t[R + 1]();
    if (!c) return false;
    for (std::size_t i = 0; i < n; ++i) ++c[static_cast<U>(static_cast<U>(a[i]) - static_cast<U>(mn))];
    std::size_t o = 0;
    for (std::uint64_t v = 0; v <= R; ++v) {
        const T x = static_cast<T>(static_cast<U>(static_cast<U>(mn) + static_cast<U>(v)));
        std::fill(a + o, a + o + c[v], x);
        o += c[v];
    }
    delete[] c;
    return true;
}

// Ejecuta f(0..P-1) en P hilos (el actual incluido). Si no se puede crear un
// hilo, esa tarea corre en el hilo actual. Relanza la primera excepción.
template <class F>
void run_parallel(unsigned P, F f) {
    std::vector<std::exception_ptr> err(P);
    std::vector<std::thread> th;
    th.reserve(P);
    auto task = [&](unsigned i) { try { f(i); } catch (...) { err[i] = std::current_exception(); } };
    for (unsigned i = 1; i < P; ++i) {
        try { th.emplace_back(task, i); }
        catch (...) { task(i); }
    }
    task(0);
    for (auto& t : th) t.join();
    for (auto& e : err) if (e) std::rethrow_exception(e);
}

// Cuántos de X hay entre los primeros k de la mezcla estable de X e Y.
template <class T, class Cmp>
std::size_t co_rank(std::size_t k, const T* X, std::size_t a, const T* Y, std::size_t b, Cmp& c) {
    std::size_t lo = k > b ? k - b : 0, hi = std::min(k, a);
    while (lo < hi) {
        std::size_t i = lo + ((hi - lo) >> 1), j = k - i;
        if (!c(Y[j - 1], X[i])) lo = i + 1; else hi = i;      // X[i] va antes que Y[j-1]
    }
    return lo;
}

template <class T, class Cmp>
void merge_move(T* x, T* xe, T* y, T* ye, T* d, Cmp& c) {
    while (x < xe && y < ye) {
        bool t = c(*y, *x);
        *d++ = std::move(t ? *y : *x);
        y += t; x += !t;
    }
    d = std::move(x, xe, d);
    std::move(y, ye, d);
}

// Modo paralelo: P potencia de 2, aux de n elementos.
template <class T, class Cmp>
void parallel_full(T* a, std::size_t n, T* aux, unsigned P, Cmp c, std::uint64_t seed) {
    std::vector<std::size_t> d(P + 1);
    for (unsigned i = 0; i <= P; ++i) d[i] = static_cast<std::size_t>(static_cast<unsigned __int128>(n) * i / P);
    run_parallel(P, [&](unsigned i) {
        std::size_t len = d[i + 1] - d[i];
        auto s = std::make_unique<Sorter<T, Cmp>>(a + d[i], c);
        s->use_buffer(aux + d[i], len);
        s->set_gallop(true);
        s->seed(seed ^ len);
        s->sort(len);
    });
    T *S = a, *D = aux;
    struct Cut { std::size_t lo, md, hi, k0, k1, i0, i1; };
    std::vector<Cut> cuts(P);
    std::size_t w = 1;
    for (unsigned rho = P; rho >= 2; rho /= 2) {
        const unsigned g = 2 * P / rho;                 // tareas por cada par de runs
        // todos los cortes se calculan antes de mover nada
        run_parallel(P, [&](unsigned t) {
            unsigned u = t / g, v = t % g;
            Cut q;
            q.lo = d[2 * u * w]; q.md = d[(2 * u + 1) * w]; q.hi = d[(2 * u + 2) * w];
            std::size_t N = q.hi - q.lo;
            q.k0 = static_cast<std::size_t>(static_cast<unsigned __int128>(N) * v / g);
            q.k1 = static_cast<std::size_t>(static_cast<unsigned __int128>(N) * (v + 1) / g);
            q.i0 = co_rank(q.k0, S + q.lo, q.md - q.lo, S + q.md, q.hi - q.md, c);
            q.i1 = co_rank(q.k1, S + q.lo, q.md - q.lo, S + q.md, q.hi - q.md, c);
            cuts[t] = q;
        });
        run_parallel(P, [&](unsigned t) {
            const Cut& q = cuts[t];
            merge_move(S + q.lo + q.i0, S + q.lo + q.i1, S + q.md + (q.k0 - q.i0), S + q.md + (q.k1 - q.i1), D + q.lo + q.k0, c);
        });
        std::swap(S, D);
        w *= 2;
    }
    if (S != a) run_parallel(P, [&](unsigned i) { std::move(S + d[i], S + d[i + 1], a + d[i]); });
}

}  // namespace detail

namespace detail {
// Segunda mitad del modo rápido: reserva con caída escalonada y orden. Precondición:
// s.already_sorted(0, n) devolvió false (y, si corresponde, el conteo no se aplicó).
template <class T, class Cmp>
void fast_tail(T* a, std::size_t n, Cmp c, Plan pl, Sorter<T, Cmp>& s, std::uint64_t seed) {
    Aux<T> aux;
    while (pl.mode != Plan::O1 && !aux.alloc(pl.E)) {    // caída escalonada
        std::size_t e = pl.mode == Plan::Full ? n / 2 : pl.E / 2;
        std::size_t E = bit_floor(e);
        pl.P = 1;
        if (E > Sorter<T, Cmp>::B0) { pl.mode = Plan::Block; pl.E = E; }
        else { pl.mode = Plan::O1; pl.E = 0; }
    }
    if (pl.mode == Plan::Full && pl.P > 1) { parallel_full(a, n, aux.p, pl.P, c, seed); return; }
    if (pl.mode != Plan::O1) { s.use_buffer(aux.p, pl.E); s.set_gallop(true); }
    s.sort_unsorted(n);
}
}  // namespace detail

// ===================================================================== modo rápido
template <class T, class Cmp = std::less<T>>
void stable_sort_fast(T* a, std::size_t n, Cmp c = Cmp(), const Options& o = Options()) {
    Plan pl = make_plan<T>(n, o);
    if (n < 2) return;
    const std::uint64_t seed = (o.seed ? o.seed : detail::SIGMA) ^ n;
    auto sp = std::make_unique<Sorter<T, Cmp>>(a, c);    // en el heap: el hilo llamador puede tener poco stack
    Sorter<T, Cmp>& s = *sp;
    s.seed(seed);
    if (s.already_sorted(0, n)) return;
    if constexpr (std::is_integral<T>::value && !std::is_same<T, bool>::value && detail::natural_less<T, Cmp>::value) {
        if (o.shortcuts && detail::int_shortcut(a, n, pl.budget)) return;
    }
    detail::fast_tail(a, n, c, pl, s, seed);
}
template <class T>
void stable_sort_fast(T* a, std::size_t n, const Options& o) { stable_sort_fast(a, n, std::less<T>(), o); }

// ===================================================================== orden por clave
// Orden estable por key(x), con key entera. x < y  <=>  key(x) < key(y).
template <class T, class Key>
void stable_sort_by_key(T* a, std::size_t n, Key key, const Options& o = Options()) {
    using K = std::decay_t<decltype(key(a[0]))>;
    static_assert(std::is_integral<K>::value, "la clave debe ser entera");
    auto less = [key](const T& x, const T& y) { return key(x) < key(y); };
    auto generic = [&]() { stable_sort_fast(a, n, less, o); };
    Plan pl = make_plan<T>(n, o);
    if (!std::is_trivially_copyable<T>::value || n < 4096 || pl.mode != Plan::Full) { generic(); return; }
    // ya ordenado por clave / pocos giros
    {
        std::size_t i = 1;
        while (i < n && !(key(a[i]) < key(a[i - 1]))) ++i;
        if (i == n) return;
        std::size_t g = 0;
        bool prev = key(a[1]) < key(a[0]);
        for (i = 2; i < n && g * 8 < n; ++i) { bool dd = key(a[i]) < key(a[i - 1]); g += dd != prev; prev = dd; }
        if (g * 8 < n) { generic(); return; }
    }
    using U = std::make_unsigned_t<K>;
    K mn = key(a[0]), mx = mn;
    for (std::size_t i = 1; i < n; ++i) { K k = key(a[i]); if (k < mn) mn = k; if (mx < k) mx = k; }
    const std::uint64_t R = static_cast<U>(static_cast<U>(mx) - static_cast<U>(mn));
    auto rel = [&](const T& x) -> std::uint64_t { return static_cast<U>(static_cast<U>(key(x)) - static_cast<U>(mn)); };
    const unsigned rb = detail::bits(R), ib = detail::bits(n - 1);
    // indirecto empaquetado: se ordenan enteros (clave, índice) y se permuta una vez
    if (sizeof(T) >= 32 && rb + ib <= 64 && n <= pl.budget / (sizeof(T) + 16)) {
        detail::Aux<std::uint64_t> ks;
        detail::Aux<T> aux;
        if (ks.alloc(n) && aux.alloc(n)) {
            for (std::size_t i = 0; i < n; ++i) ks.p[i] = (ib == 64 ? 0 : rel(a[i]) << ib) | i;
            Options ko = o;
            ko.probe = false; ko.max_bytes = 8 * n;
            stable_sort_fast(ks.p, n, std::less<std::uint64_t>(), ko);
            const std::uint64_t mask = ib >= 64 ? ~0ull : ((1ull << ib) - 1);
            for (std::size_t j = 0; j < n; ++j) aux.p[j] = a[ks.p[j] & mask];
            std::memcpy(static_cast<void*>(a), static_cast<const void*>(aux.p), n * sizeof(T));
            return;
        }
    }
    // radix LSD sobre los registros
    if (R < (1u << 24)) {
        detail::Aux<T> aux;
        if (aux.alloc(n)) {
            const int q = R < 256 ? 1 : (R < 65536 ? 2 : 3);
            T *S = a, *D = aux.p;
            for (int dg = 0; dg < q; ++dg) {
                std::size_t cnt[257] = {0};
                const int sh = 8 * dg;
                for (std::size_t i = 0; i < n; ++i) ++cnt[((rel(S[i]) >> sh) & 255) + 1];
                for (int v = 0; v < 256; ++v) cnt[v + 1] += cnt[v];
                for (std::size_t i = 0; i < n; ++i) D[cnt[(rel(S[i]) >> sh) & 255]++] = S[i];
                std::swap(S, D);
            }
            if (S != a) std::memcpy(static_cast<void*>(a), static_cast<const void*>(S), n * sizeof(T));
            return;
        }
    }
    generic();
}

}  // namespace sot

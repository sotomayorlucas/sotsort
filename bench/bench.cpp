// Banco de pruebas SotSort vs. competidores. Modos:
//   bench time  <n> <tipos> <dists> <reps> [algos]   -> CSV de tiempos
//   bench cmps  <n> <dists> [algos]                  -> CSV de comparaciones (callback contado)
//   bench adv   <n> [algos]                          -> adversario de McIlroy (en hijo con timeout)
//   bench mem   <n> <algo>                           -> pico de RSS (ru_maxrss) para u64 aleatorio
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include "legacy/sq5.hpp"
#include "../include/sotsort.hpp"
#include "wiki.hpp"

using u32 = uint32_t; using u64 = uint64_t;
struct R32 { u64 k, p[3]; };
typedef int CMPFUNC(const void*, const void*);

extern "C" {
#define DECL3(L) void w_##L##_u32(void*,size_t); void w_##L##_u64(void*,size_t); void w_##L##_c8(void*,size_t,CMPFUNC*); void w_##L##_p8(void*,size_t);
DECL3(blit) DECL3(quad) DECL3(flux)
#define DECLG(T) void w_grail_##T(void*,size_t); void w_grailb_##T(void*,size_t);
DECLG(u32) DECLG(u64) DECLG(c8) DECLG(p8) DECLG(r32)
typedef int (*RCmp)(const uint8_t*, const uint8_t*, void*);
#define DECLR(A) void rs_##A##_u32(u32*,size_t); void rs_##A##_u64(u64*,size_t); void rs_##A##_p8(u64*,size_t); \
  void rs_##A##_r32(R32*,size_t); void rs_##A##_c8(uint8_t*,size_t,RCmp,void*);
DECLR(drift) DECLR(glide) DECLR(ipn)
int g_cmp8(const void* a, const void* b);
}

// ---------------------------------------------------------------- comparador por callback (contado)
static CMPFUNC* g_cb = nullptr;
static uint64_t g_ncmp = 0;
extern "C" int g_cmp8(const void* a, const void* b) { return g_cb(a, b); }
static int rs_tramp(const uint8_t* a, const uint8_t* b, void*) { return g_cb(a, b); }
struct CbLess { bool operator()(const u64& a, const u64& b) const { return g_cb(&a, &b) < 0; } };
static int cmp_u64_counted(const void* a, const void* b) {
  ++g_ncmp; u64 x = *(const u64*)a, y = *(const u64*)b; return (x > y) - (x < y);
}

struct KeyP8 { bool operator()(u64 a, u64 b) const { return (a >> 32) < (b >> 32); } };
struct KeyR32 { bool operator()(const R32& a, const R32& b) const { return a.k < b.k; } };

struct Algo {
  const char* name; const char* mem; bool stable;
  void (*u32f)(u32*, size_t); void (*u64f)(u64*, size_t); void (*p8f)(u64*, size_t);
  void (*r32f)(R32*, size_t); void (*c8f)(u64*, size_t);
};
#define SC(L) [](u32* a, size_t n) { w_##L##_u32(a, n); }, [](u64* a, size_t n) { w_##L##_u64(a, n); }, \
  [](u64* a, size_t n) { w_##L##_p8(a, n); }, nullptr, [](u64* a, size_t n) { w_##L##_c8(a, n, g_cb); }
#define GR(P) [](u32* a, size_t n) { P##_u32(a, n); }, [](u64* a, size_t n) { P##_u64(a, n); }, \
  [](u64* a, size_t n) { P##_p8(a, n); }, [](R32* a, size_t n) { P##_r32(a, n); }, [](u64* a, size_t n) { P##_c8(a, n); }
#define RS(A) [](u32* a, size_t n) { rs_##A##_u32(a, n); }, [](u64* a, size_t n) { rs_##A##_u64(a, n); }, \
  [](u64* a, size_t n) { rs_##A##_p8(a, n); }, [](R32* a, size_t n) { rs_##A##_r32(a, n); }, \
  [](u64* a, size_t n) { rs_##A##_c8((uint8_t*)a, n, rs_tramp, nullptr); }
#define TPL(F) [](u32* a, size_t n) { F(a, a + n, std::less<u32>()); }, [](u64* a, size_t n) { F(a, a + n, std::less<u64>()); }, \
  [](u64* a, size_t n) { F(a, a + n, KeyP8()); }, [](R32* a, size_t n) { F(a, a + n, KeyR32()); }, \
  [](u64* a, size_t n) { F(a, a + n, CbLess()); }

template <class It, class C> static void sot_f(It f, It l, C c) { sot::stable_sort(f, (size_t)(l - f), c); }
template <class It, class C> static void sq5_f(It f, It l, C c) { sq5::stable_sort(f, (size_t)(l - f), c); }
static sot::Options fastopt(unsigned th) { sot::Options o; o.threads = th; return o; }
template <class It, class C> static void sotfast_f(It f, It l, C c) { sot::stable_sort_fast(f, (size_t)(l - f), c, fastopt(1)); }
template <class It, class C> static void sotfast2_f(It f, It l, C c) { sot::stable_sort_fast(f, (size_t)(l - f), c, fastopt(2)); }
template <class It, class C> static void wiki_f(It f, It l, C c) { Wiki::Sort(f, l, c); }
template <class It, class C> static void stds_f(It f, It l, C c) { std::stable_sort(f, l, c); }
template <class It, class C> static void stdu_f(It f, It l, C c) { std::sort(f, l, c); }

static std::vector<Algo> ALGOS = {
  {"sot8r", "O(1)", true, TPL(sot_f)},
  {"sq5", "O(1)", true, TPL(sq5_f)},
  {"sot8r_fast", "O(n)", true, TPL(sotfast_f)},
  {"sot8r_fast2", "O(n) 2 hilos", true, TPL(sotfast2_f)},
  {"blitsort", "O(1)", true, SC(blit)},
  {"grail", "O(1)", true, GR(w_grail)},
  {"grail512", "O(1)", true, GR(w_grailb)},
  {"wikisort", "O(1)", true, TPL(wiki_f)},
  {"quadsort", "O(n)", true, SC(quad)},
  {"fluxsort", "O(n)", true, SC(flux)},
  {"glidesort", "O(n)", true, RS(glide)},
  {"driftsort", "O(n)", true, RS(drift)},
  {"std_stable", "O(n)", true, TPL(stds_f)},
  {"ipnsort", "O(log n)", false, RS(ipn)},
  {"std_sort", "O(log n)", false, TPL(stdu_f)},
};

// ---------------------------------------------------------------- distribuciones (claves de 64 bits)
static u64 mix(u64 x) { x += 0x9e3779b97f4a7c15ull; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ull; x = (x ^ (x >> 27)) * 0x94d049bb133111ebull; return x ^ (x >> 31); }
// small = true si las claves son chicas (< 2^32) y no hay que truncarlas
static std::vector<u64> gen(const std::string& d, size_t n, u64 seed, bool& small) {
  std::mt19937_64 R(seed); std::vector<u64> v(n); small = true;
  auto U = [&](u64 m) { return std::uniform_int_distribution<u64>(0, m - 1)(R); };
  if (d == "rand") { for (auto& x : v) x = R(); small = false; }
  else if (d == "dup16") { for (auto& x : v) x = U(16); }
  else if (d == "sqrtn") { u64 m = std::max<u64>(2, (u64)std::sqrt((double)n)); for (auto& x : v) x = U(m); }
  else if (d == "zipf") {  // s = 1.1 sobre 2^20 valores, rango mapeado por hash
    const size_t M = 1 << 20; std::vector<double> c(M); double s = 0;
    for (size_t i = 0; i < M; i++) { s += 1.0 / std::pow((double)(i + 1), 1.1); c[i] = s; }
    std::uniform_real_distribution<double> ur(0, s);
    for (auto& x : v) { size_t r = std::lower_bound(c.begin(), c.end(), ur(R)) - c.begin(); x = mix(r) >> 32; }
  }
  else if (d == "asc") { for (size_t i = 0; i < n; i++) v[i] = i; }
  else if (d == "desc") { for (size_t i = 0; i < n; i++) v[i] = n - i; }
  else if (d == "asc1pct") { for (size_t i = 0; i < n; i++) v[i] = i; for (size_t k = 0; k < n / 100; k++) v[U(n)] = U(n); }
  else if (d == "tail10") { size_t m = n - n / 10; for (size_t i = 0; i < m; i++) v[i] = i; for (size_t i = m; i < n; i++) v[i] = U(n); }
  else if (d == "organ") { for (size_t i = 0; i < n / 2; i++) v[i] = i; for (size_t i = n / 2; i < n; i++) v[i] = n - i; }
  else if (d == "sawsqrt") {  // corridas ascendentes de largo sqrt(n) con claves aleatorias
    size_t L = std::max<size_t>(2, (size_t)std::sqrt((double)n));
    for (size_t i = 0; i < n; i++) v[i] = U(n);
    for (size_t i = 0; i < n; i += L) std::sort(v.begin() + i, v.begin() + std::min(n, i + L));
  }
  else if (d == "runs63") {  // corridas de 63 (justo debajo de 64): castiga detección de corridas
    for (size_t i = 0; i < n; i++) v[i] = U(n);
    for (size_t i = 0; i < n; i += 63) std::sort(v.begin() + i, v.begin() + std::min(n, i + 63));
  }
  else if (d.rfind("file:", 0) == 0) {  // entrada concreta guardada (p.ej. la del adversario)
    FILE* f = fopen(d.c_str() + 5, "rb"); u64 m = 0; if (!f || fread(&m, 8, 1, f) != 1) { fprintf(stderr, "no file\n"); exit(2); }
    std::vector<u64> w(m); if (fread(w.data(), 8, m, f) != m) exit(2); fclose(f);
    for (size_t i = 0; i < n; i++) v[i] = w[i % m] + (i / m) * m;  // repite si n > m
  }
  else { fprintf(stderr, "dist desconocida %s\n", d.c_str()); exit(2); }
  return v;
}

// ---------------------------------------------------------------- verificación
template <class T, class K> static bool check(const T* a, size_t n, K key, bool stable_payload, u64 (*pay)(const T&)) {
  for (size_t i = 1; i < n; i++) {
    if (key(a[i]) < key(a[i - 1])) return false;
    if (stable_payload && key(a[i]) == key(a[i - 1]) && pay(a[i]) < pay(a[i - 1])) return false;
  }
  return true;
}
template <class T> static u64 fp(const T* a, size_t n) {  // huella independiente del orden (multiconjunto)
  u64 s = 0, x = 0;
  for (size_t i = 0; i < n; i++) {
    u64 h = 0; const unsigned char* p = (const unsigned char*)&a[i];
    for (size_t o = 0; o < sizeof(T); o += 8) { u64 w = 0; std::memcpy(&w, p + o, std::min<size_t>(8, sizeof(T) - o)); h = mix(h ^ w); }
    s += h; x ^= mix(h + 1);
  }
  return s ^ (x * 3);
}

static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

template <class T> static void run_time(const Algo& A, void (*f)(T*, size_t), const std::vector<T>& base, size_t n, int reps,
                                        const char* ty, const char* dist, bool (*ok)(const T*, size_t, bool), u64 fp0) {
  if (!f) return;
  size_t batch = std::max<size_t>(1, 2000000 / n);
  std::vector<T> work(n * batch); std::vector<double> ts;
  bool good = true;
  for (int r = 0; r < reps + 1; r++) {  // la primera repetición es de calentamiento
    for (size_t b = 0; b < batch; b++) std::memcpy(work.data() + b * n, base.data(), n * sizeof(T));
    double t0 = now();
    for (size_t b = 0; b < batch; b++) f(work.data() + b * n, n);
    double t1 = now();
    if (r) ts.push_back((t1 - t0) / batch);
    if (r == 0) for (size_t b = 0; b < batch; b++) good = good && ok(work.data() + b * n, n, A.stable) && fp(work.data() + b * n, n) == fp0;
  }
  std::sort(ts.begin(), ts.end());
  printf("%zu,%s,%s,%s,%s,%.4f,%.4f,%s\n", n, ty, dist, A.name, A.mem, ts[ts.size() / 2] * 1e3, ts[0] * 1e3, good ? "ok" : "FALLA");
  fflush(stdout);
}

static std::vector<std::string> split(const std::string& s) {
  std::vector<std::string> r; size_t i = 0;
  while (i <= s.size()) { size_t j = s.find(',', i); if (j == std::string::npos) j = s.size(); if (j > i) r.push_back(s.substr(i, j - i)); i = j + 1; }
  return r;
}
static bool want(const std::vector<std::string>& sel, const char* name) {
  return sel.empty() || std::find(sel.begin(), sel.end(), std::string(name)) != sel.end();
}

// ---------------------------------------------------------------- adversario de McIlroy (1999)
static std::vector<u64> av; static u64 gas, nsolid, candidate;
static bool g_lazy = false;
static int cmp_adv(const void* px, const void* py) {
  ++g_ncmp; u64 x = *(const u64*)px, y = *(const u64*)py;
  if (g_lazy) {  // adversario perezoso del libro: dos indefinidos reciben el mismo valor; indefinido > fijado
    if (av[x] == gas && av[y] == gas) { av[x] = av[y] = nsolid++; return 0; }
    return (av[x] > av[y]) - (av[x] < av[y]);
  }
  if (av[x] == gas && av[y] == gas) { if (x == candidate) av[x] = nsolid++; else av[y] = nsolid++; }
  if (av[x] == gas) candidate = x; else if (av[y] == gas) candidate = y;
  return (av[x] > av[y]) - (av[x] < av[y]);
}

int main(int argc, char** argv) {
  if (argc < 3) { fprintf(stderr, "uso: ver encabezado\n"); return 2; }
  std::string mode = argv[1]; size_t n = strtoull(argv[2], 0, 10);
  if (mode == "time") {
    auto tys = split(argv[3]); auto ds = split(argv[4]); int reps = atoi(argv[5]);
    std::vector<std::string> sel = argc > 6 ? split(argv[6]) : std::vector<std::string>{};
    for (auto& d : ds) {
      bool small; auto k = gen(d, n, 12345, small);
      for (auto& ty : tys) {
        if (ty == "u32") {
          std::vector<u32> b(n); for (size_t i = 0; i < n; i++) b[i] = (u32)k[i]; u64 f0 = fp(b.data(), n);
          for (auto& A : ALGOS) if (want(sel, A.name)) run_time<u32>(A, A.u32f, b, n, reps, "u32", d.c_str(),
            [](const u32* a, size_t n, bool) { return check(a, n, [](u32 x) { return x; }, false, (u64(*)(const u32&))nullptr); }, f0);
        } else if (ty == "u64") {
          u64 f0 = fp(k.data(), n);
          for (auto& A : ALGOS) if (want(sel, A.name)) run_time<u64>(A, A.u64f, k, n, reps, "u64", d.c_str(),
            [](const u64* a, size_t n, bool) { return check(a, n, [](u64 x) { return x; }, false, (u64(*)(const u64&))nullptr); }, f0);
        } else if (ty == "p8") {  // clave 32 bits arriba, índice original abajo: mide estabilidad
          std::vector<u64> b(n); for (size_t i = 0; i < n; i++) b[i] = ((u64)(u32)k[i] << 32) | i; u64 f0 = fp(b.data(), n);
          for (auto& A : ALGOS) if (want(sel, A.name)) run_time<u64>(A, A.p8f, b, n, reps, "p8", d.c_str(),
            [](const u64* a, size_t n, bool st) { return check(a, n, [](u64 x) { return x >> 32; }, st, +[](const u64& x) { return x & 0xffffffffu; }); }, f0);
        } else if (ty == "r32") {
          std::vector<R32> b(n); for (size_t i = 0; i < n; i++) b[i] = R32{k[i], {i, ~i, i * 7}}; u64 f0 = fp(b.data(), n);
          for (auto& A : ALGOS) if (want(sel, A.name)) run_time<R32>(A, A.r32f, b, n, reps, "r32", d.c_str(),
            [](const R32* a, size_t n, bool st) { return check(a, n, [](const R32& x) { return x.k; }, st, +[](const R32& x) { return x.p[0]; }); }, f0);
        }
      }
    }
  } else if (mode == "cmps") {
    auto ds = split(argv[3]); std::vector<std::string> sel = argc > 4 ? split(argv[4]) : std::vector<std::string>{};
    double lb = std::lgamma((double)n + 1) / std::log(2.0);
    for (auto& d : ds) {
      bool small; auto k = gen(d, n, 777, small);
      for (auto& A : ALGOS) if (want(sel, A.name)) {
        std::vector<u64> w = k; g_cb = cmp_u64_counted; g_ncmp = 0; A.c8f(w.data(), n);
        bool ok = std::is_sorted(w.begin(), w.end());
        printf("%zu,%s,%s,%s,%llu,%.4f,%s\n", n, d.c_str(), A.name, A.mem, (unsigned long long)g_ncmp, g_ncmp / lb, ok ? "ok" : "FALLA");
        fflush(stdout);
      }
    }
  } else if (mode == "adv") {
    std::vector<std::string> sel = argc > 3 ? split(argv[3]) : std::vector<std::string>{};
    const char* outdir = getenv("ADV_OUT");
    double nlog = n * std::log2((double)n);
    for (auto& A : ALGOS) if (want(sel, A.name)) {
      pid_t pid = fork();
      if (pid == 0) {
        alarm(120);
        av.assign(n, n - 1); gas = n - 1; nsolid = 0; candidate = 0; g_lazy = getenv("ADV_LAZY") != nullptr;
        // Pre-congelado: una fracción de posiciones recibe rangos sólidos al azar antes de empezar.
        // Sin esto, la detección inicial de corridas congela todo en orden ascendente y el adversario
        // "pierde" con n-1 comparaciones sin haber probado nada sobre el quicksort.
        if (const char* pf = getenv("ADV_PRE")) {
          double fr = atof(pf); std::mt19937_64 R(getenv("ADV_SEED") ? atoll(getenv("ADV_SEED")) : 1);
          size_t m = std::max<size_t>(2, (size_t)(fr * n));
          std::vector<size_t> pos(n); for (size_t i = 0; i < n; i++) pos[i] = i; std::shuffle(pos.begin(), pos.end(), R);
          for (size_t j = 0; j < m; j++) av[pos[j]] = nsolid++;
        }
        std::vector<u64> w(n); for (size_t i = 0; i < n; i++) w[i] = i;
        g_cb = cmp_adv; g_ncmp = 0; A.c8f(w.data(), n);
        u64 c_adv = g_ncmp;
        // entrada concreta: el valor congelado de cada posición original
        std::vector<u64> conc(av);
        std::vector<u64> w2 = conc; g_cb = cmp_u64_counted; g_ncmp = 0; A.c8f(w2.data(), n);
        bool ok = std::is_sorted(w2.begin(), w2.end());
        printf("%zu,%s,%s,%llu,%.3f,%llu,%.3f,%llu,%s\n", n, getenv("ADV_PRE") ? getenv("ADV_PRE") : "0", A.name, (unsigned long long)c_adv, c_adv / nlog,
               (unsigned long long)g_ncmp, g_ncmp / nlog, (unsigned long long)nsolid, ok ? "ok" : "FALLA");
        fflush(stdout);
        if (outdir) { std::string p = std::string(outdir) + "/adv_" + A.name + "_" + std::to_string(n) + "_" + (getenv("ADV_PRE") ? getenv("ADV_PRE") : "0") + ".bin";
          FILE* f = fopen(p.c_str(), "wb"); u64 m = n; fwrite(&m, 8, 1, f); fwrite(conc.data(), 8, n, f); fclose(f); }
        _exit(0);
      }
      int st; waitpid(pid, &st, 0);
      if (WIFSIGNALED(st)) { printf("%zu,%s,TIMEOUT>120s(sig %d),,,,,\n", n, A.name, WTERMSIG(st)); fflush(stdout); }
    }
  } else if (mode == "mem") {
    std::string an = argv[3]; bool small; auto k = gen("rand", n, 1, small);
    struct rusage r0; getrusage(RUSAGE_SELF, &r0);
    for (auto& A : ALGOS) if (an == A.name) A.u64f(k.data(), n);
    struct rusage r1; getrusage(RUSAGE_SELF, &r1);
    printf("%s,%zu,%ld,%ld\n", an.c_str(), n, r0.ru_maxrss, r1.ru_maxrss);
  }
  return 0;
}

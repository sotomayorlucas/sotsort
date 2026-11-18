// Prueba diferencial de sotsort.hpp contra std::stable_sort.
#include "../include/sotsort.hpp"
#include <cstdio>
#include <random>
#include <string>
#include <vector>
using u64 = uint64_t;
static std::mt19937_64 R(12345);
static u64 U(u64 m) { return R() % m; }
struct Pair { int k; unsigned id; };
struct Rec40 { int64_t k; uint64_t id; char pad[24]; };
struct Rec16 { int32_t k; uint32_t id; uint64_t x; };
struct Big { u64 k; u64 id; char pad[112]; };
struct Str { std::string s; unsigned id; };
static long fails = 0, cases = 0;

static std::vector<u64> keys(size_t n, int d) {
  std::vector<u64> v(n);
  u64 m = std::max<u64>(1, n);
  switch (d) {
    case 0: for (auto& x : v) x = R(); break;
    case 1: for (auto& x : v) x = U(4); break;
    case 2: for (auto& x : v) x = U(std::max<u64>(2, (u64)std::sqrt((double)m))); break;
    case 3: for (size_t i = 0; i < n; i++) v[i] = i; break;
    case 4: for (size_t i = 0; i < n; i++) v[i] = n - i; break;
    case 5: for (size_t i = 0; i < n; i++) v[i] = i; for (size_t k = 0; k < n / 50 + 1 && n; k++) v[U(n)] = U(m); break;
    case 6: for (size_t i = 0; i < n; i++) v[i] = i < n / 2 ? i : n - i; break;
    case 7: { size_t L = 1 + U(200); for (size_t i = 0; i < n; i++) v[i] = U(m); for (size_t i = 0; i < n; i += L) std::sort(v.begin() + i, v.begin() + std::min(n, i + L)); } break;
    case 8: for (size_t i = 0; i < n; i++) v[i] = (n - i) / 3; break;                 // descendente con empates
    case 9: for (size_t i = 0; i < n; i++) v[i] = (i * 2654435761u) % 7 + (i / 300) * 3; break;
    case 10: for (size_t i = 0; i < n; i++) v[i] = 100000 + U(60000); break;          // rango < 2^16 (conteo)
    case 11: { size_t L = 1 + U(40); for (size_t i = 0; i < n; i++) v[i] = U(m); for (size_t i = 0; i < n; i += L) std::sort(v.begin() + i, v.begin() + std::min(n, i + L), std::greater<u64>()); } break;
    default: for (auto& x : v) x = U(1000);
  }
  return v;
}
static bool eq(u64 a, u64 b) { return a == b; }
static bool eq(uint32_t a, uint32_t b) { return a == b; }
static bool eq(int32_t a, int32_t b) { return a == b; }
static bool eq(int64_t a, int64_t b) { return a == b; }
static bool eq(const Pair& a, const Pair& b) { return a.k == b.k && a.id == b.id; }
static bool eq(const Rec40& a, const Rec40& b) { return a.k == b.k && a.id == b.id; }
static bool eq(const Rec16& a, const Rec16& b) { return a.k == b.k && a.id == b.id; }
static bool eq(const Big& a, const Big& b) { return a.k == b.k && a.id == b.id; }
static bool eq(const Str& a, const Str& b) { return a.s == b.s && a.id == b.id; }

template <class T, class Cmp, class F> static void check(std::vector<T> v, Cmp c, F f, const char* what, size_t n, int d) {
  std::vector<T> ref = v;
  std::stable_sort(ref.begin(), ref.end(), c);
  f(v);
  ++cases;
  bool ok = v.size() == ref.size();
  for (size_t i = 0; ok && i < v.size(); i++) ok = eq(v[i], ref[i]);
  if (!ok) { ++fails; if (fails < 20) printf("FALLA %s n=%zu dist=%d\n", what, n, d); }
}
int main(int argc, char** argv) {
  size_t maxn = argc > 1 ? strtoull(argv[1], 0, 10) : 3000;
  int rounds = argc > 2 ? atoi(argv[2]) : 400;
  std::vector<sot::Options> opts;
  { sot::Options o; o.threads = 1; opts.push_back(o); }                                   // completo, 1 hilo
  { sot::Options o; o.threads = 4; opts.push_back(o); }                                   // completo, hilos
  { sot::Options o; o.threads = 1; o.max_bytes = 0; opts.push_back(o); }                  // O(1)
  { sot::Options o; o.threads = 2; o.shortcuts = false; opts.push_back(o); }
  { sot::Options o; o.threads = 1; o.seed = 99; o.probe = false; opts.push_back(o); }
  for (int it = 0; it < rounds; it++) {
    size_t n = it < 40 ? it : (it % 7 == 0 ? U(maxn * 40) : U(maxn));
    if (it % 53 == 52) n = 300000 + U(300000);
    int d = it % 13;
    auto k = keys(n, d);
    sot::Options o = opts[it % opts.size()];
    if (it % 3 == 0 && n) { o.max_bytes = 8 * (1 + U(2 * n)); o.probe = false; }          // bloques de tamaños varios
    // enteros (SIMD)
    { std::vector<u64> v(k); check(v, std::less<u64>(), [](auto& v) { sot::stable_sort(v.data(), v.size()); }, "u64 O1", n, d);
      check(v, std::less<u64>(), [&](auto& v) { sot::stable_sort_fast(v.data(), v.size(), std::less<u64>(), o); }, "u64 fast", n, d); }
    { std::vector<int64_t> v(n); for (size_t i = 0; i < n; i++) v[i] = (int64_t)(k[i] * (d == 0 ? 1 : 3)) - (int64_t)(n); 
      check(v, std::less<int64_t>(), [](auto& v) { sot::stable_sort(v.data(), v.size()); }, "i64 O1", n, d);
      check(v, std::less<int64_t>(), [&](auto& v) { sot::stable_sort_fast(v.data(), v.size(), std::less<int64_t>(), o); }, "i64 fast", n, d); }
    { std::vector<uint32_t> v(n); for (size_t i = 0; i < n; i++) v[i] = (uint32_t)(d == 0 ? k[i] : k[i] + 0x7ffffff0u);
      check(v, std::less<uint32_t>(), [](auto& v) { sot::stable_sort(v.data(), v.size()); }, "u32 O1", n, d);
      check(v, std::less<uint32_t>(), [&](auto& v) { sot::stable_sort_fast(v.data(), v.size(), std::less<uint32_t>(), o); }, "u32 fast", n, d); }
    { std::vector<int32_t> v(n); for (size_t i = 0; i < n; i++) v[i] = (int32_t)(uint32_t)k[i] / (d == 0 ? 1 : 1) - (d ? (int32_t)(n / 2) : 0);
      check(v, std::less<int32_t>(), [](auto& v) { sot::stable_sort(v.data(), v.size()); }, "i32 O1", n, d);
      check(v, std::less<int32_t>(), [&](auto& v) { sot::stable_sort_fast(v.data(), v.size(), std::less<int32_t>(), o); }, "i32 fast", n, d); }
    // pares: estabilidad
    { std::vector<Pair> v(n); for (size_t i = 0; i < n; i++) v[i] = {(int)(k[i] % 1000003), (unsigned)i};
      auto c = [](const Pair& a, const Pair& b) { return a.k < b.k; };
      check(v, c, [&](auto& v) { sot::stable_sort(v.data(), v.size(), c); }, "pair O1", n, d);
      check(v, c, [&](auto& v) { sot::stable_sort_fast(v.data(), v.size(), c, o); }, "pair fast", n, d);
      auto g = [](const Pair& a, const Pair& b) { return a.k > b.k; };
      check(v, g, [&](auto& v) { sot::stable_sort_fast(v.data(), v.size(), g, o); }, "pair fast desc", n, d); }
    // registros y por clave
    { std::vector<Rec40> v(n); for (size_t i = 0; i < n; i++) { v[i].k = (int64_t)(k[i] >> (d == 0 ? 20 : 0)) - 500; v[i].id = i; }
      auto c = [](const Rec40& a, const Rec40& b) { return a.k < b.k; };
      check(v, c, [&](auto& v) { sot::stable_sort_by_key(v.data(), v.size(), [](const Rec40& r) { return r.k; }, o); }, "rec40 clave", n, d);
      check(v, c, [&](auto& v) { sot::stable_sort_fast(v.data(), v.size(), c, o); }, "rec40 fast", n, d); }
    { std::vector<Rec16> v(n); for (size_t i = 0; i < n; i++) { v[i].k = (int32_t)(k[i] % (it % 2 ? 200 : 70000)) - 100; v[i].id = (uint32_t)i; }
      auto c = [](const Rec16& a, const Rec16& b) { return a.k < b.k; };
      check(v, c, [&](auto& v) { sot::stable_sort_by_key(v.data(), v.size(), [](const Rec16& r) { return r.k; }, o); }, "rec16 clave", n, d); }
    if (it % 4 == 0) { std::vector<Big> v(n); for (size_t i = 0; i < n; i++) { v[i].k = k[i] % 5000; v[i].id = i; }
      auto c = [](const Big& a, const Big& b) { return a.k < b.k; };
      check(v, c, [&](auto& v) { sot::stable_sort(v.data(), v.size(), c); }, "big O1", n, d);
      check(v, c, [&](auto& v) { sot::stable_sort_fast(v.data(), v.size(), c, o); }, "big fast", n, d); }
    // strings (tipo no trivial)
    if (n < 200000) { std::vector<Str> v(n); for (size_t i = 0; i < n; i++) { v[i].s = "clave-larga-para-evitar-sso-" + std::to_string(k[i] % 5000); v[i].id = (unsigned)i; }
      auto c = [](const Str& a, const Str& b) { return a.s < b.s; };
      check(v, c, [&](auto& v) { sot::stable_sort(v.data(), v.size(), c); }, "str O1", n, d);
      check(v, c, [&](auto& v) { sot::stable_sort_fast(v.data(), v.size(), c, o); }, "str fast", n, d); }
  }
  printf("casos=%ld fallas=%ld\n", cases, fails);
  return fails != 0;
}

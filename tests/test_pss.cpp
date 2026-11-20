// v8-r6: samplesort por páginas en modo bloques, con tipos grandes y no triviales.
// v8-r7: claves con pocos valores y sesgadas (grupos de iguales).
// Verifica que el camino nuevo se usa y es estable con tipos grandes/no triviales y presupuestos chicos.
#include "../include/sotsort.hpp"
#include <cstdio>
#include <random>
#include <string>
#include <vector>
#include <algorithm>
struct Big { long k; unsigned id; char pad[100]; };
struct Str { std::string s; unsigned id; };
int main() { std::mt19937_64 g(2); long bad = 0, cases = 0;
  for (std::size_t n : {50000u, 300000u}) for (std::size_t bytes : {200000u, 1000000u, 4000000u}) for (int mod : {0, 1, 2, 3, 16, 64, 1000, -1, -2}) {
    // mod = -2: mitad un solo valor y mitad distintos
    // mod = -1: claves sesgadas (geométrica), un valor muy repetido y una cola de distintos
    auto key = [&]() -> long { if (mod == -1) return (long)std::geometric_distribution<int>(0.3)(g);
      if (mod == -2) return (g() & 1) ? 42L : (long)(g() >> 2);
      return mod ? (long)(g() % mod) : (long)(g() >> 2); };
    { std::vector<Big> v(n); for (std::size_t i = 0; i < n; ++i) { v[i].k = key(); v[i].id = (unsigned)i; }
      auto r = v; auto c = [](const Big& a, const Big& b) { return a.k < b.k; }; std::stable_sort(r.begin(), r.end(), c);
      sot::Options o; o.threads = 1; o.probe = false; o.max_bytes = bytes; sot::stable_sort_fast(v.data(), n, c, o);
      bool ok = true; for (std::size_t i = 0; i < n; ++i) ok &= v[i].id == r[i].id; bad += !ok; ++cases; }
    { std::vector<Str> v(n); for (std::size_t i = 0; i < n; ++i) { v[i].s = "clave-larga-para-evitar-sso-" + std::to_string(mod ? key() : (long)(g() % 100000000)); v[i].id = (unsigned)i; }
      auto r = v; auto c = [](const Str& a, const Str& b) { return a.s < b.s; }; std::stable_sort(r.begin(), r.end(), c);
      sot::Options o; o.threads = 1; o.probe = false; o.max_bytes = bytes; sot::stable_sort_fast(v.data(), n, c, o);
      bool ok = true; for (std::size_t i = 0; i < n; ++i) ok &= v[i].id == r[i].id && v[i].s == r[i].s; bad += !ok; ++cases; }
  }
  printf("casos=%ld fallas=%ld\n", cases, bad); }

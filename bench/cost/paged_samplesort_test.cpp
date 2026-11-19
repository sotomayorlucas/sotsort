// Compilar con -I include -I bench/vmpowersort. Pruebas del samplesort estable por páginas: exhaustiva de la distribución k-vías y
// estabilidad del sort completo con entradas difíciles.
#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>
struct W { int k; int id; bool operator<(const W& o) const { return k < o.k; } };
#include "paged_samplesort.h"
int main() {
  long cases = 0, bad = 0;
  // 1) distribución exhaustiva: k grupos, separadores 1..k-1, claves en {0..k-1}, P = 1..3
  for (std::size_t k : {2u, 3u, 4u}) for (std::size_t P = 1; P <= 3; ++P) for (std::size_t m = 1; m <= 9; ++m) {
    std::size_t tot = 1; for (std::size_t i = 0; i < m; ++i) tot *= k;
    for (std::size_t code = 0; code < tot; ++code) {
      std::vector<W> a(m); std::size_t c = code;
      for (std::size_t i = 0; i < m; ++i) { a[i] = {(int)(c % k), (int)i}; c /= k; }
      auto ref = a; std::stable_sort(ref.begin(), ref.end());
      std::vector<W> buf(k * P), spl; for (std::size_t j = 1; j < k; ++j) spl.push_back({(int)j, -1});
      pss::Ctx<W, std::less<W>> cx{buf.data(), k * P, k, std::less<W>()};
      std::vector<std::uint16_t> grp(m + 2); std::vector<std::uint32_t> src(m + 2); std::size_t st[257];
      pss::distribute(a.data(), m, spl.data(), k, cx, st, grp.data(), src.data());
      bool ok = true; for (std::size_t i = 0; i < m; ++i) ok &= a[i].id == ref[i].id;
      for (std::size_t j = 0; j < k; ++j) ok &= st[j] == (std::size_t)(std::lower_bound(ref.begin(), ref.end(), W{(int)j, 0}) - ref.begin());
      ++cases; bad += !ok;
    }
  }
  printf("distribución exhaustiva: casos=%ld fallas=%ld\n", cases, bad);
  // 2) sort completo: estabilidad con entradas difíciles y presupuestos chicos
  std::mt19937_64 g(9); long c2 = 0, b2 = 0;
  for (std::size_t n : {5000u, 70000u, 300000u}) for (int d = 0; d < 9; ++d) for (std::size_t bytes : {20000u, 100000u, 1000000u}) {
    std::vector<W> v(n);
    for (std::size_t i = 0; i < n; ++i) {
      int x;
      switch (d) {
        case 0: x = (int)(g() >> 33); break;                  // aleatoria
        case 1: x = 7; break;                                  // todos iguales
        case 2: x = (int)(g() % 2); break;                     // 2 claves
        case 3: x = (int)(g() % 64); break;                    // 64 claves (= k)
        case 4: x = (int)(i % 1000); break;                    // sierra
        case 5: x = (int)(g() % 100 == 0 ? g() % 1000000 : 5); // casi todos iguales
            break;
        case 6: x = (int)((i * 2654435761u) % n); break;       // permutación
        case 7: x = (int)(n - i) / 3; break;                   // descendente con iguales
        default: x = (int)(g() % 3 ? (int)(g() >> 40) : 0);    // un valor muy repetido + ruido
      }
      v[i] = {x, (int)i};
    }
    auto ref = v; std::stable_sort(ref.begin(), ref.end());
    pss::sort(v.data(), n, bytes);
    bool ok = true; for (std::size_t i = 0; i < n; ++i) ok &= v[i].id == ref[i].id;
    ++c2; b2 += !ok; if (!ok) printf("FALLA n=%zu d=%d bytes=%zu\n", n, d, bytes);
  }
  printf("sort completo: casos=%ld fallas=%ld\n", c2, b2);
}

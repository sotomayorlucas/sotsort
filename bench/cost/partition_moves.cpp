// Modelo de costo: movimientos de stable_partition (una llamada) en función de m y B.
#include <cstdio>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>
static unsigned long long MV = 0, CMP = 0;
struct W { long k; W() = default; W(long x):k(x){} W(const W& o):k(o.k){++MV;} W& operator=(const W& o){k=o.k;++MV;return *this;}
  W(W&& o):k(o.k){++MV;} W& operator=(W&& o){k=o.k;++MV;return *this;} };
struct Lt { bool operator()(const W& a, const W& b) const { ++CMP; return a.k < b.k; } };
#define private public
#include "../../include/sotsort.hpp"
int main() {
  std::mt19937_64 g(1);
  for (std::size_t m : {1u<<16, 1u<<20, 1u<<22}) for (std::size_t B : {2048u, 8192u, 65536u, 1u<<20}) {
    if (B > m) continue;
    std::vector<W> a(m), buf(B); for (auto& x : a) x.k = (long)(g() >> 1);
    W piv; { std::vector<long> t; for (auto& x:a) t.push_back(x.k); std::nth_element(t.begin(), t.begin()+m/2, t.end()); piv.k = t[m/2]; }
    sot::Sorter<W, Lt> s(a.data(), Lt()); s.use_buffer(buf.data(), B);
    MV = CMP = 0; std::size_t mid = s.stable_partition(0, m, piv, false);
    double k = (double)m / B;
    printf("m=2^%2.0f B=%7zu m/B=%6.0f  moves/elem=%.2f  cmp/elem=%.2f  lg(m/B)=%.1f  mid/m=%.3f\n", std::log2((double)m), B, k, (double)MV/m, (double)CMP/m, std::log2(k), (double)(mid)/m);
  }
}

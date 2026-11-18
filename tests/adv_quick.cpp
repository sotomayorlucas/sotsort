// Adversario perezoso contra QuickEstable directo (sin detección de runs).
#include "../include/sotsort.hpp"
#include <cstdio>
using u64 = uint64_t;
static std::vector<u64> av; static u64 gas, nsolid, ncmp;
struct Lazy { bool operator()(u64 x, u64 y) const { ++ncmp;
  if (av[x] == gas && av[y] == gas) { av[x] = av[y] = 0; ++nsolid; return false; }
  return av[x] < av[y]; } };
int main() {
  for (size_t n : {1000ul, 2000ul, 4000ul, 8000ul, 16000ul, 32000ul}) {
    av.assign(n, ~0ull); gas = ~0ull; nsolid = 0; ncmp = 0;
    std::vector<u64> w(n); for (size_t i = 0; i < n; i++) w[i] = i;
    { sot::Sorter<u64, Lazy> s(w.data(), Lazy()); s.seed(1); s.quick_only(n); }
    printf("n=%-6zu comparaciones por elemento = %.1f\n", n, (double)ncmp / n);
  }
}

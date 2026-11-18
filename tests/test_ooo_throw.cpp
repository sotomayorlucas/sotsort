// Comparador que lanza dentro de tareas OOO: no debe colgarse ni tener errores de memoria.
#include "../include/experimental/sotsort_ooo.hpp"
#include <atomic>
#include <random>
#include <string>
#include <cstdio>
int main() {
  std::mt19937_64 R(5); int thrown = 0, done = 0;
  for (int tr = 0; tr < 60; tr++) {
    size_t n = 150000 + R() % 500000; std::vector<std::string> v(n);
    for (size_t i = 0; i < n; i++) v[i] = "s" + std::to_string(R() % 1000000);
    std::atomic<long> cnt{0}; long budget = (long)(R() % (25 * n));
    auto c = [&](const std::string& a, const std::string& b) { if (cnt.fetch_add(1, std::memory_order_relaxed) > budget) throw 1; return a < b; };
    sot::Options o; o.threads = 2 + tr % 3; if (tr % 3 == 1) o.max_bytes = 0;
    try { sot::stable_sort_ooo(v.data(), n, c, o); ++done; } catch (int) { ++thrown; }
  }
  printf("corridas=60 lanzaron=%d terminaron=%d\n", thrown, done);
}

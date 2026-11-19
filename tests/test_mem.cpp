// Verifica (v8-r5) que lo reservado durante el sort (buffer Aux + heap: Sorter, tablas) no supere max_bytes.
#include <cstdlib>
#include <cstdio>
#include <new>
static long long LIVE=0, PEAK=0; static bool ON=false;
static bool hook(std::size_t b){ if(ON){ LIVE+=b; if(LIVE>PEAK) PEAK=LIVE; } return true; }
#define SOT_TEST_ALLOC_HOOK hook
#include "../include/sotsort.hpp"
#include <random>
#include <vector>
#include <algorithm>
void* operator new(std::size_t n){ void* p=std::malloc(n+16); if(!p) throw std::bad_alloc(); *(std::size_t*)p=n; if(ON){LIVE+=n; if(LIVE>PEAK)PEAK=LIVE;} return (char*)p+16; }
void operator delete(void* p) noexcept { if(!p) return; char* q=(char*)p-16; if(ON) LIVE-=*(std::size_t*)q; std::free(q); }
void operator delete(void* p, std::size_t) noexcept { operator delete(p); }
struct E { int k; unsigned id; char pad[56]; };   // 64 bytes: entra también la regla de elementos grandes
template<class T> int run(size_t n, int S, size_t budget){
  std::mt19937 mt(n+S); std::vector<T> v(n); for(size_t i=0;i<n;i++){ v[i].k=int(mt()%1000000); v[i].id=unsigned(i);}
  for(size_t i=0;i<n;i+=S) std::sort(v.begin()+i,v.begin()+std::min(n,i+S),[](auto&a,auto&b){return a.k<b.k;});
  sot::Options o; o.threads=1; o.probe=false; o.max_bytes=budget;
  LIVE=0; PEAK=0; ON=true;
  sot::stable_sort_fast(v.data(),n,[](auto&a,auto&b){return a.k<b.k;},o);
  ON=false;
  // Aux no pasa por delete: se descuenta al final; el pico es lo que importa
  // Piso: el Sorter con su buffer propio (16 KB) se reserva siempre (modo O(1)).
  bool ok = (size_t)PEAK <= std::max<size_t>(budget, sizeof(sot::Sorter<T, bool(*)(const T&, const T&)>) + 1024);
  if(!ok) printf("EXCEDE n=%zu S=%d budget=%zu pico=%lld\n",n,S,budget,PEAK);
  return ok?0:1;
}
struct I { int k; unsigned id; };
int main(){ int bad=0, cases=0; std::mt19937 R(5);
  for(int it=0; it<300; it++){ size_t n=5000+R()%600000; int S=1+R()%5000; size_t budget=(size_t)((0.001+ (R()%1000)/1000.0)*n)*64 + R()%100000;
    bad+=run<E>(n,S,budget); bad+=run<I>(n,S,budget/8); cases+=2; }
  printf("casos=%d excesos=%d\n",cases,bad); return bad!=0; }

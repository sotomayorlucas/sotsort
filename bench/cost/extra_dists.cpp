// Distribuciones extra (claves sesgadas, secuencias intercaladas, dientes de sierra): comparaciones /
// movimientos por elemento de SotSort en modo O(1) y con presupuesto. Uso: extra_dists n bytes.
// Compilar: g++ -O2 -std=c++17 -I ../../include extra_dists.cpp
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>
#include <cstdint>
static unsigned long long MV, NC;
struct W { long k; std::uint32_t id; W()=default; W(long a,std::uint32_t i):k(a),id(i){}
 W(const W&o):k(o.k),id(o.id){++MV;} W&operator=(const W&o){k=o.k;id=o.id;++MV;return*this;}
 W(W&&o):k(o.k),id(o.id){++MV;} W&operator=(W&&o){k=o.k;id=o.id;++MV;return*this;} };
struct Lt{bool operator()(const W&a,const W&b)const{++NC;return a.k<b.k;}};
#include "sotsort.hpp"
int main(int argc,char**argv){ size_t n=atof(argv[1]); size_t vb=atof(argv[2]); std::mt19937_64 g(5);
 const char* nm[]={"heavy40+dist","2vals","3interl","desc-interl","geom","half1val","1000vals","heavy90","sawtooth64","blocks-rev"};
 for(int d=0;d<10;++d){ std::vector<W> v(n);
  for(size_t i=0;i<n;++i){ long x;
   switch(d){case 0: x=(g()%10<4)?77:(long)(g()>>2);break; case 1: x=g()&1;break; case 2: x=(long)((i%3)*n+i);break;
    case 3: x=(i&1)?(long)(n-i):(long)(2*n-i);break; case 4: x=std::geometric_distribution<int>(0.05)(g);break;
    case 5: x=(g()&1)?42:(long)(g()>>2);break; case 6: x=g()%1000;break; case 7: x=(g()%10<9)?5:(long)(g()%100000);break;
    case 8: x=i%64;break; default: x=(long)((n-1-i)/4096*4096+i%4096);break;}
   v[i]=W(x,i);}
  printf("%-13s",nm[d]);
  for(int mode=0;mode<2;++mode){ std::vector<W> w=v; MV=NC=0;
   if(mode==0) sot::stable_sort(w.data(),n,Lt()); else { sot::Options o;o.threads=1;o.probe=false;o.shortcuts=false;o.max_bytes=vb; sot::stable_sort_fast(w.data(),n,Lt(),o);}
   bool ok=true; for(size_t i=1;i<n;++i) if(w[i-1].k>w[i].k||(w[i-1].k==w[i].k&&w[i-1].id>w[i].id)) ok=false;
   printf(" | %6.2f / %6.2f%s",(double)NC/n,(double)MV/n,ok?"  ":" X"); }
  printf("\n"); } }

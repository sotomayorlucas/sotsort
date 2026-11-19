#include <cstdio>
#include <vector>
#include <random>
#include <cmath>
static unsigned long long MV=0, PH[4]={0,0,0,0};
struct W { int k; W()=default; W(int x):k(x){} W(const W&o):k(o.k){++MV;} W& operator=(const W&o){k=o.k;++MV;return *this;} W(W&&o):k(o.k){++MV;} W& operator=(W&&o){k=o.k;++MV;return *this;} bool operator<(const W&o)const{return k<o.k;} };
#include "breakdown_sotsort.hpp"
template <typename T> static size_t vm_bytes(size_t n) {
    const double target_p = sqrt((double)sizeof(T *) * 4 * n / (sizeof(T) * (log2(n) + 7)));
    const size_t log_n = 33 - __builtin_clz(n + 1), log_p = 33 - __builtin_clz((int)target_p + 1),
                 co_log_p = log_n - log_p + 1, max_runs = log_n + 4, extra_pages = max_runs + 3,
                 page_buf_sz = extra_pages << log_p, free_page_buf_sz = extra_pages + 1, ptr_buf_sz = 4 << co_log_p;
    return sizeof(T) * page_buf_sz + sizeof(T *) * (free_page_buf_sz + ptr_buf_sz); }
int main(int argc,char**argv){ size_t n=atol(argv[1]); std::mt19937 g(1); std::vector<W> v(n); for(auto&x:v)x.k=g();
 sot::Options o; o.threads=1;o.probe=false;o.shortcuts=false;o.max_bytes=vm_bytes<W>(n);
 MV=0; sot::stable_sort_fast(v.data(),n,std::less<W>(),o);
 printf("n=%zu budget=%zuB total=%.2f leaf=%.2f part<=%.2f part_eq=%.2f pivot=%.2f other=%.2f\n",n,o.max_bytes,(double)MV/n,(double)PH[0]/n,(double)PH[1]/n,(double)PH[2]/n,(double)PH[3]/n,(double)(MV-PH[0]-PH[1]-PH[2]-PH[3])/n);}

#include "inst.hpp"
#include <random>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
struct W { int t; W():t(0){} W(int x):t(x){} W(const W&o):t(o.t){++sotinst::G;} W(W&&o):t(o.t){++sotinst::G;} W& operator=(const W&o){t=o.t;++sotinst::G;return *this;} W& operator=(W&&o){t=o.t;++sotinst::G;return *this;}
 bool operator<(const W&o)const{return t<o.t;} };
template <typename T> static size_t vm_bytes(size_t n) {
    const double target_p = sqrt((double)sizeof(T *) * 4 * n / (sizeof(T) * (log2(n) + 7)));
    const size_t log_n = 33 - __builtin_clz(n + 1), log_p = 33 - __builtin_clz((int)target_p + 1),
                 co_log_p = log_n - log_p + 1, max_runs = log_n + 4, extra_pages = max_runs + 3,
                 page_buf_sz = extra_pages << log_p, free_page_buf_sz = extra_pages + 1, ptr_buf_sz = 4 << co_log_p;
    return sizeof(T) * page_buf_sz + sizeof(T *) * (free_page_buf_sz + ptr_buf_sz);
}
int main(int argc,char**argv){ int n=950000; double f=argc>1?atof(argv[1]):-1;
 for(int S: {100,1000,10000,100000,1000000}){
  std::mt19937 mt(124); std::vector<W> v(n);
  for(auto&x:v) x.t=std::uniform_int_distribution<int>(100,1000000000)(mt);
  for(int i=0;i<n;){int j=std::min(n,i+std::geometric_distribution<int>(1.0/S)(mt)); std::sort(v.begin()+i,v.begin()+j); i=j;}
  using namespace sotinst; G=ROT=MERGE=SYM=INS=0;
  sot::Options o; o.threads=1;o.probe=false;o.shortcuts=false; o.max_bytes = f<0? vm_bytes<W>(n) : size_t(f*n)*sizeof(W);
  sot::stable_sort_fast(v.data(),(size_t)n,std::less<W>(),o);
  double N=n; printf("S=%7d total=%.2f  sym=%.2f (rot=%.2f merge_buf=%.2f)  ins=%.2f  other=%.2f\n",S,G/N,SYM/N,ROT/N,MERGE/N,INS/N,(G-SYM-INS)/N);
 }}

#!/bin/bash
# Builds the comparison harness. Fetches third-party sorts (not redistributed here).
set -e
cd "$(dirname "$0")"
mkdir -p ext && cd ext
for r in scandum/blitsort scandum/quadsort scandum/fluxsort Mrrl/GrailSort BonzaiThePenguin/WikiSort; do
  [ -d "$(basename $r)" ] || git clone -q --depth 1 https://github.com/$r
done
cd ..
sed -n 1,968p ext/WikiSort/WikiSort.cpp > wiki.hpp          # the Wiki:: namespace only (drops its main)
CC="gcc -O3 -march=native -w"
for L in blit quad flux; do
  $CC -c w/w_$L.c -o w/w_$L.o && objcopy -w --keep-global-symbol="w_${L}_*" w/w_$L.o
  $CC -c w/w_${L}_p8.c -o w/w_${L}_p8.o && objcopy -w --keep-global-symbol="w_${L}_*" w/w_${L}_p8.o
done
$CC -c w/w_grail.c -DSORT_TYPE=uint32_t -DTAG=u32 -o w/g_u32.o
$CC -c w/w_grail.c -DSORT_TYPE=uint64_t -DTAG=u64 -o w/g_u64.o
$CC -c w/w_grail.c -DSORT_TYPE=uint64_t -DTAG=c8 -DCMPF=g_cmp8 -o w/g_c8.o
$CC -c w/w_grail.c -DSORT_TYPE=uint64_t -DTAG=p8 '-DKEYEXPR(e)=((*(e))>>32)' -o w/g_p8.o
$CC -c w/w_grail.c -DSORT_TYPE=r32_t -DTAG=r32 '-DKEYEXPR(e)=((e)->k)' -o w/g_r32.o
for f in w/g_*.o; do objcopy -w --keep-global-symbol='w_grail*' $f; done
(cd rsort && RUSTFLAGS="-C target-cpu=native" cargo build --release)
g++ -std=c++20 -O3 -march=native -w bench.cpp w/*.o rsort/target/release/librsort.a -lpthread -ldl -lm -o bench
g++ -std=c++17 -O3 -march=native -w bench_ooo.cpp -o bench_ooo -lpthread
echo "ok: ./bench time 1000000 u64 rand,dup16 5   |   ./bench_ooo 1000000 2 5"

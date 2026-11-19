#!/bin/bash
# v8-r6 en la PC: tiempo, n=10^6, S en {2,100,10^6}, 4 tipos. Celdas pendientes hasta agotar $1 s.
A="almost copy sotvm sothalf"; out=results-pc-r6; mkdir -p $out; end=$((SECONDS+$1))
[ -f $out/lscpu ] || { lscpu > $out/lscpu; g++ --version | head -1 > $out/compiler; }
for t in int ptr fastarr arr; do for s in 2 100 1000000; do
 f=$out/time-$t-$s.txt; grep -q AVERAGE $f 2>/dev/null && continue
 [ $SECONDS -ge $end ] && { echo PARTIAL; exit; }
 maxn=1000000 iters=10 seqlen=$s type=$t metric=time ./main_r6 $A > $f.tmp && mv $f.tmp $f
done; done; echo ALLDONE

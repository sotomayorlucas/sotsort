#!/bin/bash
# n=1e7, int, solo tiempo (los conteos no dependen de la máquina). Celdas pendientes hasta agotar $1 s.
A="almost copy sotvm sothalf"; out=results-pc-1e7-r5; mkdir -p $out; end=$((SECONDS+$1))
[ -f $out/lscpu ] || { lscpu > $out/lscpu; g++ --version | head -1 > $out/compiler; }
for s in 2 100 1000 10000 100000 1000000; do
 f=$out/time-int-$s.txt; grep -q AVERAGE $f 2>/dev/null && continue
 [ $SECONDS -ge $end ] && { echo PARTIAL; exit; }
 maxn=10000000 iters=${IT:-10} seqlen=$s type=int metric=time ./main_fin $A > $f.tmp && mv $f.tmp $f
done; echo ALLDONE

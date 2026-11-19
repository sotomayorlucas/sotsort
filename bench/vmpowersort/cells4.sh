#!/bin/bash
# Celdas pendientes hasta agotar $1 segundos: tiempo, r3 (826125b) contra r4.
A="almost copy sotvm sothalf"; out=results-pc-r4; mkdir -p $out
[ -f $out/lscpu ] || { lscpu > $out/lscpu; g++ --version | head -1 > $out/compiler; }
end=$((SECONDS+$1))
for t in int ptr fastarr arr; do for s in 2 100 1000 10000 100000 1000000; do for b in r3 r4; do
  f=$out/time-$t-$s-$b.txt; grep -q AVERAGE $f 2>/dev/null && continue
  [ $SECONDS -ge $end ] && { echo PARTIAL; exit; }
  maxn=1000000 iters=10 seqlen=$s type=$t ./main_$b $A > $f.tmp && mv $f.tmp $f && echo "$t $s $b ($SECONDS s)"
done; done; done; echo ALLDONE

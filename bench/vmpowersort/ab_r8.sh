#!/bin/bash
# A/B en el mismo binario: sot6vm (r6) contra sotvm (r8), con VM-Powersort (almost); ambos órdenes.
out=${OUT:-results-r8t}; mkdir -p $out; end=$((SECONDS+$1))
[ -f $out/lscpu ] || { lscpu > $out/lscpu; g++ --version | head -1 > $out/compiler; }
for s in 2 100 1000000; do for t in int ptr fastarr arr; do for o in a b; do
 f=$out/time-$t-$s-$o.txt; grep -q AVERAGE $f 2>/dev/null && continue
 [ $SECONDS -ge $end ] && { echo PARTIAL; exit; }
 if [ $o = a ]; then A="almost sot6vm sotvm"; else A="almost sotvm sot6vm"; fi
 maxn=1000000 iters=10 seqlen=$s type=$t metric=time ./main_r8 $A > $f.tmp && mv $f.tmp $f
done; done; done; echo ALLDONE

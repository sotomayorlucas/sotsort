#!/bin/bash
A="almost copy sotvm sothalf"; out=results-1e7-r5; mkdir -p $out; end=$((SECONDS+$1))
[ -f $out/lscpu ] || lscpu > $out/lscpu
for m in time comps assignments; do for s in 2 100 1000 10000 100000 1000000; do
 f=$out/$m-int-$s.txt; grep -q AVERAGE $f 2>/dev/null && continue
 [ $SECONDS -ge $end ] && { echo PARTIAL; exit; }
 it=10; [ $m != time ] && it=3
 maxn=10000000 iters=$it seqlen=$s type=int metric=$m taskset -c 1 ./main_fin $A > $f.tmp && mv $f.tmp $f
done; done; echo ALLDONE

#!/bin/bash
A="almost copy sotvm sothalf"; out=results-r4; end=$((SECONDS+$1))
for m in time comps assignments; do types="int ptr fastarr arr"; [ $m != time ] && types="int fastarr"
for t in $types; do for s in 2 100 1000 10000 100000 1000000; do for b in r3c r4c; do
 f=$out/$m-$t-$s-$b.txt; grep -q AVERAGE $f 2>/dev/null && continue
 [ $SECONDS -ge $end ] && { echo PARTIAL; exit; }
 it=10; [ $m != time ] && it=3
 maxn=1000000 iters=$it seqlen=$s type=$t metric=$m taskset -c 1 ./main_$b $A > $f.tmp && mv $f.tmp $f
done; done; done; done; echo ALLDONE

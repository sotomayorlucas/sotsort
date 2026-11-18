#!/bin/bash
A="almost sotvm sothalf"; out=results-ab/$(date +%H%M); mkdir -p $out
for m in time comps; do types="int ptr fastarr arr"; [ $m != time ] && types=int
 for t in $types; do for s in 2 100 1000 10000 100000 1000000; do for b in old new; do
  maxn=1000000 iters=10 seqlen=$s type=$t metric=$m taskset -c 1 ./main_$b $A > $out/$m-$t-$s-$b.txt
 done; done; done; done
echo DONE > $out/DONE

#!/bin/bash
out=results-jug; mkdir -p $out
for m in time assignments; do for s in 2 100 1000 10000 100000 1000000; do for b in rev jug; do
 maxn=1000000 iters=10 seqlen=$s type=fastarr metric=$m taskset -c 1 ./main_$b almost sotvm sothalf > $out/$m-$s-$b.txt
done; done; done; echo DONE > $out/DONE

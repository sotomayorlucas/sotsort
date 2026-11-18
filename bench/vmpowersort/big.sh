#!/bin/bash
out=results-1e7; mkdir -p $out; lscpu > $out/lscpu
for m in time comps assignments; do for s in 2 100 1000 10000 100000 1000000; do
 it=10; [ $m != time ] && it=3
 maxn=10000000 iters=$it seqlen=$s type=int metric=$m taskset -c 1 ./main_r3 almost copy simple sotvm sothalf > $out/$m-int-$s.txt
done; done; echo DONE > $out/DONE

#!/bin/bash
# Corrida de comparación sobre el harness de VM-Powersort (rama assignment-fix + SotSort)
A="almost copy simple stable wiki sot0 sotvm sothalf sotfull"
out=results-sot/$(date +%y%m%d-%H%M); mkdir -p $out; lscpu > $out/lscpu; g++ --version | head -1 > $out/compiler
echo "maxn=1000000 iters=10 algos=($A)" > $out/setup
for m in time comps assignments; do
  types="int ptr arr fastarr"; [ $m != time ] && types=int
  for t in $types; do for s in 2 100 1000 10000 100000 1000000; do
    maxn=1000000 iters=10 seqlen=$s type=$t metric=$m taskset -c 1 ./main $A > $out/$m-$t-$s.txt
  done; done
done
echo DONE > $out/DONE

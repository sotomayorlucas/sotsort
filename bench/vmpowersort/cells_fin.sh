#!/bin/bash
# Final: r4 (sot4*) y r5 (sot*) en el mismo binario. Celdas pendientes hasta agotar $1 s.
A="almost copy sot4vm sotvm sot4half sothalf"; out=${OUT:-results-fin}; BIN=${BIN:-./main_fin}; mkdir -p $out
[ -f $out/lscpu ] || { lscpu > $out/lscpu; g++ --version | head -1 > $out/compiler; echo "maxn=1000000 algos=($A)" > $out/setup; }
end=$((SECONDS+$1))
for m in time comps assignments; do types="int ptr fastarr arr"; [ $m != time ] && types="int fastarr"
for t in $types; do for s in 2 100 1000 10000 100000 1000000; do
 f=$out/$m-$t-$s.txt; grep -q AVERAGE $f 2>/dev/null && continue
 [ $SECONDS -ge $end ] && { echo PARTIAL; exit; }
 it=10; [ $m != time ] && it=2
 maxn=1000000 iters=$it seqlen=$s type=$t metric=$m ${TS} $BIN $A > $f.tmp && mv $f.tmp $f
done; done; done; echo ALLDONE

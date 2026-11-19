#!/bin/bash
# A/B en el mismo binario: sot6vm (r6, búsqueda binaria) contra sotvm (árbol sin saltos), S = 2.
out=results-pc-bl; mkdir -p $out; end=$((SECONDS+$1))
for t in int ptr fastarr arr; do for o in a b; do
 f=$out/time-$t-$o.txt; grep -q AVERAGE $f 2>/dev/null && continue
 [ $SECONDS -ge $end ] && { echo PARTIAL; exit; }
 if [ $o = a ]; then A="almost sot6vm sotvm"; else A="almost sotvm sot6vm"; fi
 maxn=1000000 iters=10 seqlen=2 type=$t metric=time ./main_bl $A > $f.tmp && mv $f.tmp $f
done; done; echo ALLDONE

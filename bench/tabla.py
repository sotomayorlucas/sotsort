import csv, sys, collections
f = sys.argv[1]; rows = list(csv.reader(open(f)))
bad = [r for r in rows if r[7] != 'ok']; print("fallas:", bad)
T = collections.defaultdict(dict); algos = []
for n,ty,d,a,mem,med,mn,ok in rows:
    T[(ty,d)][a] = float(med)
    if a not in algos: algos.append(a)
short = {'sotsort':'sot','blitsort':'blit','grail':'grail','grail512':'gr512','wikisort':'wiki','quadsort':'quad','fluxsort':'flux','glidesort':'glide','driftsort':'drift','std_stable':'stdst','ipnsort':'ipn','std_sort':'stdso'}
O1 = ['sotsort','blitsort','grail','grail512','wikisort']
for ty in ['u32','u64','p8','r32']:
    ks = [k for k in T if k[0]==ty]
    if not ks: continue
    print(f"\n== {ty} (ms, mediana) ; sot/blit = cociente")
    hdr = [a for a in algos if a in T[ks[0]]]
    print(f"{'dist':8}" + "".join(f"{short[a]:>7}" for a in hdr) + "  sot/blit  sot/mejorO1  sot/mejorEstable")
    for k in ks:
        r = T[k]
        o1 = min(r[a] for a in O1 if a in r and a!='sotsort')
        st = min(r[a] for a in r if a not in ('sotsort','ipnsort','std_sort'))
        bl = r.get('blitsort')
        print(f"{k[1]:8}" + "".join(f"{r[a]:7.2f}" if r[a]>=0.1 else f"{r[a]:7.3f}" for a in hdr)
              + (f"  {r['sotsort']/bl:8.2f}" if bl else "        -") + f"  {r['sotsort']/o1:10.2f}  {r['sotsort']/st:10.2f}")

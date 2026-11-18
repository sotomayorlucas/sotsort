import glob,os,sys,re
d=sorted(glob.glob('results-sot/*'))[-1]
A="almost copy simple stable wiki sot0 sotvm sothalf sotfull".split()
rows={}
for f in glob.glob(d+'/*.txt'):
    m,t,s=os.path.basename(f)[:-4].split('-')
    L=open(f).read().split('\n')
    if 'AVERAGE: ' not in L: continue
    i=L.index('AVERAGE: '); v=[float(x) for x in L[i+1].split()]
    rows[(m,t,int(s))]=dict(zip(A,v))
for m in ['time','comps','assignments']:
    ks=sorted(k for k in rows if k[0]==m)
    if not ks: continue
    print(f'\n## {m}'+(' (s)' if m=='time' else ' (por elemento, n≈9.5e5)'))
    print('type S | '+' '.join(f'{a:>8}' for a in A)+' | sotvm/almost')
    for k in ks:
        r=rows[k]; nn=950000
        val=lambda a:(r[a] if m=='time' else r[a]/nn)
        print(f'{k[1]:7} {k[2]:>7} | '+' '.join(f'{val(a):8.3f}' for a in A)+f' | {r["sotvm"]/r["almost"]:.2f}')

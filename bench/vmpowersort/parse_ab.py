import glob,os
d=sorted(glob.glob('results-ab/*'))[-1]; A=['almost','sotvm','sothalf']; R={}
for f in glob.glob(d+'/*.txt'):
    m,t,s,b=os.path.basename(f)[:-4].split('-'); L=open(f).read().split('\n'); i=L.index('AVERAGE: ')
    R[(m,t,int(s),b)]=dict(zip(A,map(float,L[i+1].split())))
for m in ['time','comps']:
    print(f'\n## {m}: sotvm/almost  antes -> después   | sothalf/almost antes -> después')
    for t in ['int','ptr','fastarr','arr']:
        for s in [2,100,1000,10000,100000,1000000]:
            if (m,t,s,'old') not in R: continue
            o,n=R[(m,t,s,'old')],R[(m,t,s,'new')]
            ex='' if m=='time' else f'   comps/elem sotvm {o["sotvm"]/950000:.2f} -> {n["sotvm"]/950000:.2f} (almost {n["almost"]/950000:.2f})'
            print(f'{t:7} {s:>7}  {o["sotvm"]/o["almost"]:.2f} -> {n["sotvm"]/n["almost"]:.2f}    | {o["sothalf"]/o["almost"]:.2f} -> {n["sothalf"]/n["almost"]:.2f}{ex}')

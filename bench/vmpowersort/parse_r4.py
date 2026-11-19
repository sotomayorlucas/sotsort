import glob,os,sys
d=sys.argv[1]; A=sys.argv[2].split(); R={}
for f in glob.glob(d+'/time-*.txt'):
    m,t,s,b=os.path.basename(f)[:-4].split('-'); L=open(f).read().split('\n')
    if 'AVERAGE: ' not in L: continue
    i=L.index('AVERAGE: '); R[(t,int(s),b)]=dict(zip(A,map(float,L[i+1].split())))
bs=sorted({k[2] for k in R}); o,nw=bs[0],bs[1]
print(f'{d}: sotvm/almost {o}->{nw}  |  sothalf/copy {o}->{nw}')
for t in ['int','ptr','fastarr','arr']:
    cells=[]
    for s in [2,100,1000,10000,100000,1000000]:
        if (t,s,nw) not in R: cells.append(f'S={s}: --'); continue
        a,b=R[(t,s,o)],R[(t,s,nw)]
        cells.append(f'S={s}: {a["sotvm"]/a["almost"]:.2f}->{b["sotvm"]/b["almost"]:.2f} ({a["sothalf"]/a["copy"]:.2f}->{b["sothalf"]/b["copy"]:.2f})')
    print(f'{t:8}'+'  '.join(cells))

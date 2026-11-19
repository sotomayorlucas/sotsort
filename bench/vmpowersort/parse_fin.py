import glob,os,sys
d=sys.argv[1]; A="almost copy sot4vm sotvm sot4half sothalf".split(); R={}
for f in glob.glob(d+'/*.txt'):
    m,t,s=os.path.basename(f)[:-4].split('-'); L=open(f).read().split('\n')
    if 'AVERAGE: ' not in L: continue
    i=L.index('AVERAGE: '); R[(m,t,int(s))]=dict(zip(A,map(float,L[i+1].split())))
Ss=[2,100,1000,10000,100000,1000000]
print(f'## {d}: tiempo, misma memoria que VM-Powersort: r4/VM -> r5/VM   |  memoria n/2: r5/copy')
for t in ['int','ptr','fastarr','arr']:
    if ('time',t,2) not in R: continue
    print(f'{t:8}'+'  '.join(f'S={s}: {R[("time",t,s)]["sot4vm"]/R[("time",t,s)]["almost"]:.2f}->{R[("time",t,s)]["sotvm"]/R[("time",t,s)]["almost"]:.2f} ({R[("time",t,s)]["sothalf"]/R[("time",t,s)]["copy"]:.2f})' for s in Ss if ('time',t,s) in R))
for m in ['comps','assignments']:
  for t in ['int','fastarr']:
    if (m,t,2) not in R: continue
    print(f'{m} {t} (por elemento: VM / r4 / r5): '+' | '.join(f'S={s}: {R[(m,t,s)]["almost"]/950000:.2f} / {R[(m,t,s)]["sot4vm"]/950000:.2f} / {R[(m,t,s)]["sotvm"]/950000:.2f}' for s in Ss if (m,t,s) in R))

import sys
d=sys.argv[1]
def avg(f):
    L=open(f).read().split('\n')
    for i,l in enumerate(L):
        if 'AVERAGE' in l: return [float(x) for x in L[i+1].split()]
print("celda               r6/VM   new/VM  new/r6  (por orden: r6 a b; new a b)")
for s in [2,100,1000000]:
    for t in ['int','ptr','fastarr','arr']:
        try: a=avg(f'{d}/time-{t}-{s}-a.txt'); b=avg(f'{d}/time-{t}-{s}-b.txt')
        except Exception: continue
        if not a or not b: continue
        r6=[a[1]/a[0], b[2]/b[0]]; r8=[a[2]/a[0], b[1]/b[0]]
        m6=sum(r6)/2; m8=sum(r8)/2
        print(f"{t:8s}S={s:<8d}  {m6:.3f}   {m8:.3f}   {m8/m6:.3f}   ({r6[0]:.3f} {r6[1]:.3f}; {r8[0]:.3f} {r8[1]:.3f})")

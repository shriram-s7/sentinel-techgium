import random, math, subprocess, json, sys
BIT={'heat':1,'drift':2,'noise':4,'bus':8,'power':16}
def trial(kind, rng, T=900):
    A0=rng.uniform(24,34); onset=rng.randint(300,500)
    # environment: slow wander + one sun/AC swing in every trial
    sw_t=rng.randint(150,700); sw_amp=rng.choice([-1,1])*rng.uniform(3,8); sw_len=rng.uniform(300,900)
    a=A0; die=A0+6; sen=A0; rows=[]; w=0
    M=rng.uniform(4,15); amp=rng.uniform(0.5,3); rate=rng.uniform(0.003,0.02); step=rng.uniform(1,6)
    bus_len=rng.randint(60,300); intermittent=rng.random()<0.5
    for t in range(T):
        w+=rng.gauss(0,0.01); env=A0+w+ (sw_amp*min(1,max(0,(t-sw_t)/sw_len)))
        die+= (env+6-die)/20; sen+=(env-sen)/40
        d=die+rng.gauss(0,0.4); s=sen+rng.gauss(0,0.06); i=12+rng.gauss(0,0.1); ok=1
        if rng.random()<0.1: i+=18        # radio TX burst overlaps ~1 in 10 samples
        f=t-onset
        if f>=0:
            if kind=='heat': h=M*(1-math.exp(-f/60)); d+=h; s+=0.15*M*(1-math.exp(-f/150))
            if kind=='noise' and rng.random()<0.5: s+=rng.uniform(-amp,amp)
            if kind=='drift': s+=rate*f
            if kind=='power': i+=step
            if kind=='bus' and f<bus_len: ok=0 if (not intermittent or rng.random()<0.7) else 1
        rows.append((round(d*100),round(s*100) if ok else 0,round(i*100),3300+round(rng.gauss(0,5)),ok))
    return rows, onset, dict(M=M,amp=amp,rate=rate,step=step,bus_len=bus_len,intermittent=intermittent,sw_amp=sw_amp)

def run(kinds, N=30, seed=7):
    rng=random.Random(seed); trials=[(k,)+trial(k,rng) for k in kinds for _ in range(N)]
    inp=[]; 
    for k,rows,on,p in trials: inp.append('R'); inp+=['%d %d %d %d %d'%r for r in rows]
    out=subprocess.run(['./host'],input='\n'.join(inp)+'\n',capture_output=True,text=True).stdout.split('\n')
    res=[]; idx=0; per=[]
    for k,rows,on,p in trials:
        assert out[idx]=='R'; h=[int(x) for x in out[idx+1:idx+1+len(rows)]]; idx+=1+len(rows)
        fa = any(x&63 for x in (h if k=='normal' else h[:on]))
        det=None; wrong=False
        if k!='normal':
            for t in range(on,min(on+180,len(h))):
                if h[t]&BIT[k]: det=t-on; break
                if h[t]&63 & ~BIT[k]: wrong=True
        res.append(dict(kind=k,fa=fa,det=det,wrong=wrong,p=p,h=h,on=on))
    return res
if __name__=='__main__':
    res=run(['heat','noise','bus','drift','power','normal'])
    summ={}
    for k in ['heat','noise','bus','drift','power','normal']:
        r=[x for x in res if x['kind']==k]; d=[x['det'] for x in r if x['det'] is not None]
        summ[k]=dict(n=len(r),detected=len(d),false_alarm_trials=sum(x['fa'] for x in r),wrong=sum(x['wrong'] for x in r),
                     lat_med=sorted(d)[len(d)//2] if d else None, lat_max=max(d) if d else None)
        print(k, summ[k])
    missed=[(x['kind'],{a:round(b,3) if isinstance(b,float) else b for a,b in x['p'].items()}) for x in res if x['kind']!='normal' and x['det'] is None]
    print('missed:',missed[:12])
    json.dump(summ,open('summary.json','w'))

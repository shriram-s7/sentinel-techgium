import json, math
from sim import make, host, score
tr = make(['heat', 'noise', 'bus', 'drift', 'power', 'normal'], 30, 2027)
sel = [t for t in tr if t[0] in ('heat', 'drift', 'normal')]
def lp(xs, k=16):
    m = xs[0]; o = []
    for x in xs: m += (x - m) / k; o.append(m)
    return o
def run3(alarm):            # 3 consecutive -> alarm time
    r = 0
    for t, a in enumerate(alarm):
        r = r + 1 if a else 0
        if r >= 3: return t
def det_fixed(rows):
    s = [r[1] / 100 for r in rows]; d = [r[0] / 100 for r in rows]
    bs = sum(s[300:420]) / 120; bd = sum(d[300:420]) / 120
    return run3([t >= 420 and (abs(s[t] - bs) > 2.0 or d[t] - bd > 5.0) for t in range(len(rows))])
def det_resid(rows):        # threshold on (sensor - smoothed chip): two witnesses, no CUSUM
    s = [r[1] / 100 for r in rows]; d = lp([r[0] / 100 for r in rows])
    res = [a - b for a, b in zip(s, d)]; b = sum(res[300:420]) / 120
    return run3([t >= 420 and abs(res[t] - b) > 1.0 for t in range(len(rows))])
def det_cusum_sensor(rows): # CUSUM on the sensor alone, same k=2sigma, h=8sigma, sigma floor 0.25
    s = [r[1] / 100 for r in rows]; mu = sum(s[300:420]) / 120
    sd = max(0.25, (sum((x - mu) ** 2 for x in s[300:420]) / 120) ** .5); k, h = 2 * sd, 8 * sd
    up = dn = 0; al = []
    for t, x in enumerate(s):
        if t < 420: al.append(False); continue
        z = x - mu; up = max(0, up + z - k); dn = max(0, dn - z - k); al.append(up > h or dn > h)
    return run3(al)
hs = host([t[1] for t in sel]); sc = score(sel, hs)
out = {}
for name, fn in [('Fixed thresholds on raw readings', det_fixed), ('Threshold on (sensor − chip)', det_resid), ('CUSUM on the sensor alone', det_cusum_sensor)]:
    fa = {'normal': 0}; caught = {'heat': 0, 'drift': 0}
    for k, rows, on, p in sel:
        f = fn(rows)
        if k == 'normal': fa['normal'] += f is not None
        else:
            if f is not None and on <= f <= on + 180: caught[k] += 1
            if f is not None and f < on: fa.setdefault(k + '_early', 0); fa[k + '_early'] += 1
    out[name] = dict(fa=fa['normal'], heat=caught['heat'], drift=caught['drift'], early=sum(v for a, v in fa.items() if a != 'normal'))
s_fa = sum(1 for x in sc if x['kind'] == 'normal' and x['fa'])
out['Sentinel (both + CUSUM + state machine)'] = dict(fa=s_fa, heat=sum(1 for x in sc if x['kind'] == 'heat' and x['det'] is not None),
    drift=sum(1 for x in sc if x['kind'] == 'drift' and x['det'] is not None), early=sum(1 for x in sc if x['kind'] != 'normal' and x['fa']))
for a, b in out.items(): print(a, b)
json.dump(out, open('ablation.json', 'w'), indent=1)

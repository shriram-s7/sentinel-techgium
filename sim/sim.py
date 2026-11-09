"""Fault-injection simulation. Runs the real sentinel.c (compiled for the PC as ./host)."""
import random, math, subprocess, json, sys
BIT = {'heat': 1, 'drift': 2, 'noise': 4, 'bus': 8, 'power': 16}
NAMES = ['heat', 'drift', 'noise', 'bus', 'power']

def trial(kind, rng, T=1200):
    A0 = rng.uniform(24, 34); onset = rng.randint(600, 800)
    sw_t = rng.randint(150, 1000); sw_amp = rng.choice([-1, 1]) * rng.uniform(3, 8); sw_len = rng.uniform(300, 900)
    w = 0; die = A0; sen = A0; heat0 = 0; rows = []
    M = rng.uniform(4, 15); amp = rng.uniform(0.5, 3); rate = rng.uniform(0.003, 0.02); step = rng.uniform(1, 6)
    bus_len = rng.randint(60, 300); p_fail = 1.0 if rng.random() < 0.5 else rng.uniform(0.1, 0.7)
    for t in range(T):
        w += rng.gauss(0, 0.01); env = A0 + w + sw_amp * min(1, max(0, (t - sw_t) / sw_len))
        heat0 += (6 - heat0) / 120.0                          # chip warms itself after power-on
        die += (env + heat0 - die) / 20; sen += (env - sen) / 40
        d = die + rng.gauss(0, 0.4); s = sen + rng.gauss(0, 0.06); i = 12 + rng.gauss(0, 0.1); bus = 1
        if rng.random() < 0.1: i += 18                        # radio TX burst overlaps ~1 in 10 samples
        f = t - onset
        if f >= 0:
            if kind == 'heat': d += M * (1 - math.exp(-f / 60)); s += 0.15 * M * (1 - math.exp(-f / 150))
            if kind == 'noise' and rng.random() < 0.5: s += rng.uniform(-amp, amp)   # PWM coupled into the NTC line
            if kind == 'drift': s += rate * f                                       # heater on the sensor lead only
            if kind == 'power': i += step                                           # switched load on the MCU rail
            if kind == 'bus' and f < bus_len and rng.random() < p_fail: bus = 0      # SDA to the INA219 opened
        rows.append((round(d * 100), round(s * 100), round(i * 100) if bus else 0, 3300 + round(rng.gauss(0, 5)), bus, 1, bus))
    return rows, onset, dict(M=M, amp=amp, rate=rate, step=step, bus_len=bus_len, p_fail=p_fail, sw_amp=sw_amp)

def host(trials):
    inp = []
    for rows in trials: inp.append('R'); inp += ['%d %d %d %d %d %d %d' % r for r in rows]
    out = subprocess.run(['./host'], input='\n'.join(inp) + '\n', capture_output=True, text=True).stdout.split('\n')
    res = []; idx = 0
    for rows in trials:
        assert out[idx] == 'R'; res.append([int(x) for x in out[idx + 1: idx + 1 + len(rows)]]); idx += 1 + len(rows)
    return res

def make(kinds, N, seed):
    rng = random.Random(seed)
    return [(k,) + trial(k, rng) for k in kinds for _ in range(N)]

def score(tr, hs):
    out = []
    for (k, rows, on, p), h in zip(tr, hs):
        pre = h if k == 'normal' else h[:on]
        fa = any(x & 31 for x in pre)
        first = None; det = None
        if k != 'normal':
            for t in range(on, min(on + 180, len(h))):
                if h[t] & 31:
                    if first is None: first = [n for n in NAMES if h[t] & BIT[n]][0]
                    if h[t] & BIT[k]: det = t - on; break
        else:
            for x in h:
                if x & 31: first = [n for n in NAMES if x & BIT[n]][0]; break
        out.append(dict(kind=k, fa=fa, det=det, first=first, p=p, on=on))
    return out

if __name__ == '__main__':
    seed = int(sys.argv[1]) if len(sys.argv) > 1 else 2027
    kinds = ['heat', 'noise', 'bus', 'drift', 'power', 'normal']
    tr = make(kinds, 30, seed); hs = host([t[1] for t in tr]); sc = score(tr, hs)
    S = {}
    for k in kinds:
        r = [x for x in sc if x['kind'] == k]; d = sorted(x['det'] for x in r if x['det'] is not None)
        conf = {n: sum(1 for x in r if x['first'] == n) for n in NAMES}; conf['none'] = sum(1 for x in r if x['first'] is None)
        S[k] = dict(det=len(d), fa=sum(x['fa'] for x in r), med=d[len(d) // 2] if d else None, mx=max(d) if d else None,
                    first=conf, missed=[{a: round(b, 3) for a, b in x['p'].items() if a in ('M', 'amp', 'rate', 'step', 'p_fail')} for x in r if x['det'] is None and k != 'normal'])
        print(k, S[k])
    ff = sum(len(t[1]) - 420 for t in tr if t[0] == 'normal') + sum(t[2] - 420 for t in tr if t[0] != 'normal')
    S['_faultfree_s'] = ff; print('fault-free h', ff / 3600)
    json.dump(S, open('final.json', 'w'), indent=1)
    json.dump([(t[0], t[2]) for t in tr], open('trials_meta.json', 'w'))

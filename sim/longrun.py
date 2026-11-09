import subprocess, json
from concurrent.futures import ThreadPoolExecutor
def go(a): r, seed = a; o = subprocess.run(['./longsim', str(r), '30', str(seed)], capture_output=True, text=True).stdout.split(); return (r, seed) + tuple(int(x) for x in o)
jobs = [(0, s) for s in range(1, 101)] + [(r, 1000 + i + int(r * 1000)) for r in (0.05, 0.1, 0.15, 0.2, 0.3, 0.5) for i in range(10)]
with ThreadPoolExecutor(8) as ex: res = list(ex.map(go, jobs))
norm = [x for x in res if x[0] == 0]; dr = [x for x in res if x[0] > 0]
fa_runs = [x for x in norm if x[2] >= 0]
ff_days = len(norm) * 30 + sum(x[4] for x in dr) / 86400
S = dict(normal_runs=len(norm), fa_normal=len(fa_runs), fa_flags=[x[3] for x in fa_runs],
         fa_before_onset=sum(1 for x in dr if x[2] >= 0), fault_free_days=round(ff_days), by_rate={})
for r in (0.05, 0.1, 0.15, 0.2, 0.3, 0.5):
    x = [y for y in dr if y[0] == r]; d = sorted((y[5] - y[4]) / 86400 for y in x if y[5] >= 0)
    S['by_rate'][r] = dict(caught=len(d), of=len(x), days_med=round(d[len(d) // 2], 2) if d else None, days_max=round(max(d), 2) if d else None,
                          err_med=round(r * d[len(d) // 2], 2) if d else None, err_max=round(r * max(d), 2) if d else None)
print(json.dumps(S, indent=1)); json.dump(S, open('longrun.json', 'w'), indent=1)

import random, struct
from unicorn import *; from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
f=ELFFile(open('../firmware/diag.elf','rb')); sym={s.name:s['st_value'] for s in f.get_section_by_name('.symtab').iter_symbols()}
mu=Uc(UC_ARCH_ARM, UC_MODE_THUMB|UC_MODE_MCLASS)
mu.mem_map(0x08000000,0x10000); mu.mem_map(0x20000000,0x4000); mu.mem_map(0x1FFF0000,0x10000)
for seg in f.iter_segments():
    if seg['p_type']=='PT_LOAD' and seg['p_filesz']: mu.mem_write(seg['p_paddr'], seg.data())
cnt=[0]; mu.hook_add(UC_HOOK_CODE, lambda u,a,s,d: cnt.__setitem__(0,cnt[0]+1))
RET=0x0800F000; mu.mem_write(RET, b'\x00\xbf'*4)
canary=sym['__stack_canary']; mu.mem_write(canary, struct.pack('<I',0xC0DEFACE))
def call(fn,r0=0):
    cnt[0]=0; mu.reg_write(UC_ARM_REG_SP,0x20001F00); mu.reg_write(UC_ARM_REG_R0,r0); mu.reg_write(UC_ARM_REG_LR,RET|1)
    mu.emu_start(sym[fn]|1, RET); return cnt[0]
call('sn_init'); buf=0x20001000; st=[]; mem=[]
random.seed(1)
for t in range(600):
    ok = 0 if 400<=t<420 else 1
    die=2850+int(random.gauss(0,25)); sens=2700+int(random.gauss(0,8))+(t>300)*(t-300)*3
    mu.mem_write(buf, struct.pack('<hhHHB', die, sens, 1200+int(random.gauss(0,5)), 3300, ok))
    n=call('sn_step',buf); 
    if t>=120: st.append(n)
    mem.append(call('sn_memory_check_step'))
print('sn_step instr: avg %.0f max %d'%(sum(st)/len(st),max(st)))
print('mem_check instr: avg %.0f max %d'%(sum(mem)/len(mem),max(mem)))
print('digest instr', call('sn_digest', buf))

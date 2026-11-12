"""Extract interoperability labels from the supplied OS 2.1 panel image.

Decoded firmware is an ignored build input and is never packaged. Requires
Capstone only for regenerating the checked-in header, not for plugin builds.
"""
import csv
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'build/python-tools'))
from capstone import Cs, CS_ARCH_ARM, CS_MODE_THUMB

image = ROOT / 'build/firmware/Cobalt8_panel_2-1.bin'
b = image.read_bytes()
assert hashlib.sha256(b).hexdigest() == '2bf96abfc7114188416900ede3102a58eae56385d034258c68919a68040bce7a'
cs = Cs(CS_ARCH_ARM, CS_MODE_THUMB)

def text(p):
    return b[p:p+80].split(b'\0')[0].decode('ascii').lstrip('+')

def switch_names(base, count, width):
    result = []
    for n in range(count):
        offset = b[base+n] if width == 1 else struct.unpack_from('<H', b, base+2*n)[0]
        address = 0x410000+base+2*offset
        for ins in cs.disasm(b[address-0x410000:address-0x410000+32], address):
            if ins.mnemonic == 'ldr' and '[pc' in ins.op_str:
                immediate = int(re.search(r'#(0x[\da-f]+|\d+)',ins.op_str)[1],0)
                literal = ((ins.address+4)&~3)+immediate-0x410000
                result.append(text(struct.unpack_from('<I',b,literal)[0]-0x410000))
                break
        else:
            raise ValueError(f'Unresolved enum {n}')
    return result

fx = switch_names(0xc652,27,1)
filters = switch_names(0xca1c,31,2)
labels = [[text(0x32a94+13*(i*6+j)) for j in range(6)] for i in range(27)]
assert fx[1] == 'Chorus' and filters[0] == 'Reso LP' and labels[1][0] == 'Dry:Wet Mix'
algorithms = [r['panel_name'] for r in csv.DictReader((ROOT.parent/'research/synth-character/cobalt8-algorithm-map.csv').open())]
destinations = [m[0].decode() for m in re.finditer(rb'[ -~]{3,}',b[0x35f0c:0x36150])]
assert len(destinations)==55 and len(algorithms)==40
sources = ['LFO1','LFO2','LFO3','Mod EG','Note','Velocity','Expression','Aftertouch','Joystick X+','Joystick X-','Joystick Y+','Joystick Y-']
tables = {'algorithms':algorithms,'filters':filters,'effects':fx,'sources':sources,'destinations':destinations}
out = ['// OS 2.1 interoperability labels; see tools/generate_tables.py.','#pragma once','namespace cobalt {']
for name, values in tables.items():
    out.append(f'inline constexpr const char* {name}[] = {{'+','.join(json.dumps(v) for v in values)+'};')
out.append('inline constexpr const char* effectLabels[27][6] = {'+','.join('{'+','.join(json.dumps(v) for v in row)+'}' for row in labels)+'};')
out.append('}')
(ROOT/'src').mkdir(exist_ok=True)
(ROOT/'src/tables.hpp').write_text('\n'.join(out)+'\n',encoding='utf-8')
(ROOT/'build/firmware-tables.json').write_text(json.dumps({**tables,'effect_labels':labels},indent=2))
print('Extracted 40 algorithms, 31 filters, 27 effects / 162 labels and 55 destinations.')

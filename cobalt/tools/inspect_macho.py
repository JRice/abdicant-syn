"""Read symbols and bounded byte ranges from the supplied Mac plugin; never execute it."""
import argparse
import json
import struct
import re
import subprocess
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("symbol", nargs="?", default="__ZN2c810param_midiEiii")
parser.add_argument("--out", type=Path, default=Path("cobalt/build/macho-symbol.bin"))
parser.add_argument("--disassemble", action="store_true")
parser.add_argument("--max-bytes", type=int, default=16384)
args = parser.parse_args()
data = args.binary.read_bytes()
assert data[:4] == bytes.fromhex("cafebabe")
count = struct.unpack_from(">I", data, 4)[0]
for index in range(count):
    cpu, _, offset, size, _ = struct.unpack_from(">IIIII", data, 8 + index * 20)
    if cpu == 0x1000007:
        data = data[offset:offset + size]
        break
else:
    raise ValueError("No Intel slice")
assert data[:4] == bytes.fromhex("cffaedfe")
ncmds = struct.unpack_from("<I", data, 16)[0]
cursor = 32
segments = []
for _ in range(ncmds):
    cmd, size = struct.unpack_from("<II", data, cursor)
    if cmd == 0x19:
        vmaddr, vmsize, fileoff, filesize = struct.unpack_from("<QQQQ", data, cursor + 24)
        segments.append((vmaddr, vmsize, fileoff, filesize))
    if cmd == 2:
        symoff, nsyms, stroff, strsize = struct.unpack_from("<IIII", data, cursor + 8)
    cursor += size
symbols = {}
for i in range(nsyms):
    nameoff, kind, section, desc, value = struct.unpack_from("<IBBHQ", data, symoff + 16 * i)
    if value and nameoff:
        end = data.index(0, stroff + nameoff)
        name = data[stroff + nameoff:end].decode("utf-8", errors="replace")
        symbols[name] = value
args.out.parent.mkdir(parents=True, exist_ok=True)
args.out.with_suffix(".symbols.json").write_text(json.dumps(symbols, indent=2), encoding="utf-8")
address = symbols[args.symbol]
next_address = min((v for v in symbols.values() if v > address), default=address + 1024)
length = min(next_address - address, args.max_bytes)
for vmaddr, vmsize, fileoff, filesize in segments:
    if vmaddr <= address < vmaddr + filesize:
        start = fileoff + address - vmaddr
        args.out.write_bytes(data[start:start + length])
        print(f"{args.symbol}: address={address:#x}, length={length}, file={args.out}")
        break
else:
    raise ValueError("Symbol has no file-backed segment")

def read_address(address, length):
    for vmaddr, vmsize, fileoff, filesize in segments:
        if vmaddr <= address < vmaddr + filesize:
            start = fileoff + address - vmaddr
            return data[start:start + length]
    return b""

if args.symbol == "__ZN2c89kCcParamsE":
    rows = []
    for off in range(0, length, 16):
        number, pointer = struct.unpack_from("<QQ", args.out.read_bytes(), off)
        # DYLD_CHAINED_PTR_64_OFFSET stores the image offset in the low 36 bits.
        name = read_address(pointer & ((1 << 36) - 1), 120).split(b"\0")[0].decode("ascii")
        rows.append({"cc": number, "name": name})
    args.out.with_suffix(".json").write_text(json.dumps(rows, indent=2), encoding="utf-8")
    print(json.dumps(rows, indent=2))

if args.disassemble:
    result = subprocess.run([r"C:/msys64/ucrt64/bin/objdump.exe", "-D", "-b", "binary", "-m", "i386:x86-64", str(args.out)], capture_output=True, text=True, check=True)
    names = {v: k for k, v in symbols.items()}
    lines = []
    for line in result.stdout.splitlines():
        target = re.search(r"(?:#|call\s+|jmp\s+)\s*(0x[0-9a-f]+)", line)
        if target:
            dest = address + int(target[1], 16)
            if dest in names:
                line += " ; " + names[dest]
            raw = read_address(dest, 120).split(b"\0")[0]
            if len(raw) > 2 and all(32 <= c < 127 for c in raw):
                line += " ; " + raw.decode("ascii")
        lines.append(line)
    args.out.with_suffix(".asm").write_text("\n".join(lines), encoding="utf-8")

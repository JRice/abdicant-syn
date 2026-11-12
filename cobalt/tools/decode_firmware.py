"""Decode a user-supplied COBALT8 update container; no MIDI or code execution."""
import argparse
import hashlib
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('source', type=Path)
parser.add_argument('destination', type=Path)
args = parser.parse_args()
decoded = bytearray()
sequence = 0
for fragment in args.source.read_bytes().split(b'\xf7'):
    if not fragment:
        continue
    packet = fragment + b'\xf7'
    if len(packet) != 167:
        continue  # Start/end framing messages are not firmware data packets.
    if packet[:5] != bytes.fromhex('F0 00 21 07 19'):
        raise ValueError('Not a COBALT8 firmware packet')
    index = packet[8] | (packet[9] << 7)
    if index != sequence:
        raise ValueError(f'Unexpected firmware packet {index}, wanted {sequence}')
    for group in range(19):
        high = packet[17 + group * 8]
        for column in range(7 if group < 18 else 2):
            decoded.append(packet[10 + group * 8 + column] | (((high >> (6-column)) & 1) << 7))
    sequence += 1
if not sequence:
    raise ValueError('No complete firmware data packets')
args.destination.parent.mkdir(parents=True, exist_ok=True)
args.destination.write_bytes(decoded)
print(f'{sequence} packets, {len(decoded)} bytes, SHA256 {hashlib.sha256(decoded).hexdigest()}')

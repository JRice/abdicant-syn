"""Offline COBALT8 message encoder. Extended mappings require OS 2.1 validation.

No MIDI ports are opened by this module. Addresses are protocol facts inferred
from the supplied Cobalt8VST 0.4.9 binary, not copied implementation code.
"""
from dataclasses import dataclass


@dataclass(frozen=True)
class CC:
    controller: int
    value: int


def channel_status(channel: int) -> int:
    if type(channel) is not int or not 1 <= channel <= 16:
        raise ValueError("MIDI channel must be an integer from 1 to 16")
    return 0xB0 + channel - 1


def seven_bit(value: int) -> int:
    if type(value) is not int or not 0 <= value <= 127:
        raise ValueError("MIDI data must be an integer from 0 to 127")
    return value


def cc(controller: int, value: int, channel: int = 1) -> bytes:
    return bytes((channel_status(channel), seven_bit(controller), seven_bit(value)))


def extended(group: int, section: int, field: int, value: int, channel: int = 1) -> bytes:
    """Modal transaction: 99=group, 98=section, 38=value, then 6=field.

    This order matters: do not substitute a generic 14-bit NRPN encoder.
    The final CC6 commits the field, rather than carrying a value MSB.
    Keep all four messages together when queuing output.
    """
    return b"".join(cc(c, v, channel) for c, v in (
        (99, group), (98, section), (38, value), (6, field)
    ))


def effect(slot: int, field: int, value: int, channel: int = 1) -> bytes:
    """Candidate mapping: slots 1..3, field 0=type and 1..6=effect parameters."""
    if type(slot) is not int or slot not in (1, 2, 3):
        raise ValueError("Effect slot must be 1, 2, or 3")
    if type(field) is not int or not 0 <= field <= 6:
        raise ValueError("Effect field must be 0 to 6")
    return extended(34, slot, field, value, channel)


def source_depth_field(field: int, value: int, channel: int = 1) -> bytes:
    """OS 2.1 source-depth fields: note, velocity, expression, pressure, joystick."""
    if type(field) is not int or not 0 <= field <= 7:
        raise ValueError("Source-depth field must be 0 to 7")
    return extended(32, 12, field, value, channel)


def fixed_route_field(field: int, value: int, channel: int = 1) -> bytes:
    """OS 2.1 four preassigned modulation routes, separate from source depths."""
    if type(field) is not int or not 0 <= field <= 3:
        raise ValueError("Fixed-route field must be 0 to 3")
    return extended(32, 13, field, value, channel)


def modulation(slot: int, source: int, destination: int, depth: int, channel: int = 1) -> bytes:
    """Documented CC addresses; source/destination VALUE tables still need checking.

    Temporarily zero the depth while replacing the routing. Raw MIDI depth 64
    is the provisional bipolar midpoint; verify that convention on hardware.
    """
    if type(slot) is not int or not 1 <= slot <= 8:
        raise ValueError("Modulation slot must be 1 to 8")
    for value in (source, destination, depth):
        seven_bit(value)
    i = slot - 1
    return b"".join(cc(c, v, channel) for c, v in (
        (88 + i, 64), (100 + i, source), (108 + i, destination), (88 + i, depth)
    ))

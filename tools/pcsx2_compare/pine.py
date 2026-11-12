"""A minimal client of PCSX2's PINE interface (docs/plans/validation/PCSX2_COMPARISON_PLAN.md, step 7).

PINE is PCSX2's local remote-control protocol, on Windows a TCP socket on 127.0.0.1 at the configured slot
([EmuCore] EnablePINE, PINESlot). A request is its total length (u32, header included), an opcode (u8) and its
arguments; a reply is its total length (u32) and a result (u8, 0 ok, 0xFF failed), then data. A session uses it to
have PCSX2 write a save state at a checkpoint, which holds GS memory.
"""
import socket
import struct

SLOT = 28011
SAVE_STATE, STATUS = 0x09, 0x0F
RUNNING, PAUSED, SHUTDOWN = 0, 1, 2


def request(opcode, arguments=b'', slot=SLOT, timeout=10.0):
    """Sends one request and returns the reply's data; raises OSError when PCSX2 refuses it."""
    message = struct.pack('<IB', 5 + len(arguments), opcode) + arguments
    with socket.create_connection(('127.0.0.1', slot), timeout=timeout) as connection:
        connection.sendall(message)
        header = _receive(connection, 5)
        size, result = struct.unpack('<IB', header)
        data = _receive(connection, size - 5)
    if result != 0:
        raise OSError(f'PINE request {opcode:#x} failed')
    return data


def _receive(connection, size):
    data = b''
    while len(data) < size:
        chunk = connection.recv(size - len(data))
        if not chunk:
            raise OSError('PINE connection closed')
        data += chunk
    return data


def save_state(state_slot, slot=SLOT):
    """Asks PCSX2 to write save state `state_slot` (it does so on its own thread, shortly after)."""
    request(SAVE_STATE, struct.pack('<B', state_slot), slot)


def status(slot=SLOT):
    return struct.unpack('<I', request(STATUS, slot=slot))[0]

#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
CAN IAP host for WarShip F103 bootloader (test when you have USB-CAN).

Same logical frame as UART IAP, chunked into classic CAN frames:

  Host -> MCU  StdId=0x100
  MCU  -> Host StdId=0x101

  FIRST: Data[0]=0x01, [1]=len_hi, [2]=len_lo, [3..7]=up to 5 bytes
  CONT:  Data[0]=0x02, [1]=seq,    [2..7]=up to 6 bytes

Logical frame = identical to UART:
  A5 5A | CMD | SEQ | LEN | PAYLOAD | CRC16-Modbus

Requires:
  pip install python-can
  A backend: pcan / canalystii / slcan / socketcan / ...

Examples:
  python iap_can_host.py --channel PCAN_USBBUS1 --interface pcan app_boot.hex
  python iap_can_host.py --channel can0 --interface socketcan app_boot.hex
  python iap_can_host.py --dry-run app_boot.hex   # only build/print chunks

Hold KEY0 to enter IAP before sending (same as UART).
WarShip: P9 jumper CAN, 500kbit/s, another node or USB-CAN must ACK.
"""

from __future__ import print_function

import argparse
import struct
import sys
import time

import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from iap_uart_host import (  # noqa: E402
    CMD_ERASE,
    CMD_INFO,
    CMD_JUMP,
    CMD_WRITE,
    CHUNK,
    SOH,
    ACK,
    crc16_modbus,
    load_firmware,
)

CAN_RX_ID = 0x100  # host transmits this
CAN_TX_ID = 0x101  # MCU transmits this
CAN_FIRST = 0x01
CAN_CONT = 0x02


def chunk_frame(data):
    """Yield CAN data bytearrays (DLC implicit by len)."""
    total = len(data)
    first_n = min(5, total)
    msg = bytearray(3 + first_n)
    msg[0] = CAN_FIRST
    msg[1] = (total >> 8) & 0xFF
    msg[2] = total & 0xFF
    msg[3:] = data[:first_n]
    yield bytes(msg)
    off = first_n
    idx = 1
    while off < total:
        n = min(6, total - off)
        msg = bytearray(2 + n)
        msg[0] = CAN_CONT
        msg[1] = idx & 0xFF
        msg[2:] = data[off:off + n]
        yield bytes(msg)
        off += n
        idx += 1


def build_iap_frame(cmd, seq, payload=b""):
    plen = len(payload)
    body = bytes([cmd, seq, (plen >> 8) & 0xFF, plen & 0xFF]) + payload
    crc = crc16_modbus(body)
    return SOH + body + bytes([(crc >> 8) & 0xFF, crc & 0xFF])


class CanIapHost(object):
    def __init__(self, interface, channel, bitrate=500000):
        import can  # pip install python-can

        self.can = can
        self.bus = can.Bus(interface=interface, channel=channel, bitrate=bitrate)
        self.seq = 0

    def close(self):
        self.bus.shutdown()

    def _send_frame(self, frame):
        for chunk in chunk_frame(frame):
            msg = self.can.Message(
                arbitration_id=CAN_RX_ID, is_extended_id=False, data=chunk
            )
            self.bus.send(msg)
            time.sleep(0.002)

    def _recv_asm(self, timeout=5.0):
        deadline = time.time() + timeout
        need = 0
        got = bytearray()
        expect_idx = 1
        while time.time() < deadline:
            msg = self.bus.recv(timeout=0.2)
            if msg is None or msg.arbitration_id != CAN_TX_ID:
                continue
            d = bytes(msg.data)
            if not d:
                continue
            if d[0] == CAN_FIRST:
                if len(d) < 3:
                    continue
                need = (d[1] << 8) | d[2]
                got = bytearray(d[3:])
                expect_idx = 1
            elif d[0] == CAN_CONT and need:
                if len(d) < 2 or d[1] != expect_idx:
                    need = 0
                    continue
                expect_idx += 1
                got.extend(d[2:])
            if need and len(got) >= need:
                return bytes(got[:need])
        raise TimeoutError("no CAN IAP response")

    def cmd(self, cmd, payload=b"", timeout=5.0):
        self.seq = (self.seq + 1) & 0xFF
        frame = build_iap_frame(cmd, self.seq, payload)
        self._send_frame(frame)
        resp = self._recv_asm(timeout=timeout)
        if len(resp) < 9 or resp[0:2] != ACK:
            raise RuntimeError("bad CAN resp header")
        if crc16_modbus(resp[2:-2]) != ((resp[-2] << 8) | resp[-1]):
            raise RuntimeError("CAN resp CRC error")
        if resp[2] != cmd or resp[3] != self.seq:
            raise RuntimeError("CAN resp cmd/seq mismatch")
        status = resp[4]
        plen = (resp[5] << 8) | resp[6]
        return status, resp[7:7 + plen]

    def info(self):
        st, data = self.cmd(CMD_INFO, timeout=2.0)
        if st != 0 or len(data) < 16:
            raise RuntimeError("INFO status=%d" % st)
        return struct.unpack(">IIII", data[:16])

    def erase(self, addr, size):
        st, _ = self.cmd(CMD_ERASE, struct.pack(">II", addr, size), timeout=60.0)
        if st != 0:
            raise RuntimeError("ERASE status=%d" % st)

    def write(self, addr, data):
        off, total = 0, len(data)
        while off < total:
            chunk = data[off:off + CHUNK]
            st, _ = self.cmd(CMD_WRITE, struct.pack(">I", addr + off) + chunk, timeout=5.0)
            if st != 0:
                raise RuntimeError("WRITE @0x%08X status=%d" % (addr + off, st))
            off += len(chunk)
            print("\rWRITE %d/%d" % (off, total), end="")
            sys.stdout.flush()
        print()

    def jump(self):
        st, _ = self.cmd(CMD_JUMP, timeout=2.0)
        if st != 0:
            raise RuntimeError("JUMP status=%d" % st)


def dry_run(path):
    base, blob = load_firmware(path)
    if len(blob) & 1:
        blob += b"\xFF"
    print("FW base=0x%08X size=%d" % (base, len(blob)))
    frame = build_iap_frame(CMD_INFO, 1, b"")
    print("INFO frame %d bytes, CAN chunks:" % len(frame))
    for i, c in enumerate(chunk_frame(frame)):
        print("  [%d] %s" % (i, c.hex()))
    print("OK (dry-run). Connect USB-CAN to test for real.")


def main():
    ap = argparse.ArgumentParser(description="F103 CAN IAP host (test later)")
    ap.add_argument("firmware", help="app_boot.hex or .bin")
    ap.add_argument("--interface", default="pcan", help="python-can interface")
    ap.add_argument("--channel", default="PCAN_USBBUS1", help="channel / can0 / COM for slcan")
    ap.add_argument("--bitrate", type=int, default=500000)
    ap.add_argument("--no-jump", action="store_true")
    ap.add_argument("--dry-run", action="store_true", help="no hardware, print chunks only")
    args = ap.parse_args()

    if args.dry_run:
        dry_run(args.firmware)
        return

    base, blob = load_firmware(args.firmware)
    if len(blob) & 1:
        blob += b"\xFF"
    print("FW base=0x%08X size=%d" % (base, len(blob)))
    print("Hold KEY0, enter IAP, P9 CAN jumper on, USB-CAN at 500k.")

    host = CanIapHost(args.interface, args.channel, args.bitrate)
    try:
        app, size, page, ver = host.info()
        print("Board: APP=0x%08X SIZE=0x%X PAGE=0x%X VER=0x%X" % (app, size, page, ver))
        erase_addr = base & ~(page - 1)
        erase_len = ((base + len(blob) - erase_addr + page - 1) // page) * page
        print("ERASE 0x%08X len=0x%X ..." % (erase_addr, erase_len))
        host.erase(erase_addr, erase_len)
        print("ERASE OK")
        host.write(base, blob)
        print("WRITE OK")
        if not args.no_jump:
            host.jump()
            print("JUMP sent")
    finally:
        host.close()


if __name__ == "__main__":
    main()

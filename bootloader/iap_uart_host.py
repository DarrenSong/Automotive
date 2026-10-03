#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
UART IAP host for WarShip F103 bootloader demo.

Frame host->MCU:
  A5 5A | CMD | SEQ | LEN_H LEN_L | PAYLOAD | CRC16_H CRC16_L
  CRC16-Modbus over CMD..PAYLOAD

CMD: 01 INFO  02 ERASE  03 WRITE  04 JUMP

Example:
  Hold KEY0 + reset until "IAP ready"
  pip install pyserial
  python iap_uart_host.py COM5 D:\\Automotive\\app_boot\\OBJ\\app_boot.hex
"""

from __future__ import print_function

import argparse
import struct
import sys
import time

try:
    import serial
except ImportError:
    print("Need: pip install pyserial")
    sys.exit(1)

SOH = b"\xA5\x5A"
ACK = b"\x5A\xA5"
CMD_INFO, CMD_ERASE, CMD_WRITE, CMD_JUMP = 0x01, 0x02, 0x03, 0x04
# WRITE payload = addr(4) + data; must be <= board IAP_MAX_PAYLOAD - 4
CHUNK = 252
APP_ADDR_DEFAULT = 0x08008000
STATUS_NAME = {
    0: "OK",
    1: "CRC",
    2: "PARAM",
    3: "FLASH",
    4: "RANGE",
    5: "APP",
    6: "TIMEOUT",
    7: "CMD",
}


def crc16_modbus(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else (crc >> 1)
    return crc & 0xFFFF


def load_firmware(path):
    if path.lower().endswith(".bin"):
        with open(path, "rb") as f:
            return APP_ADDR_DEFAULT, f.read()

    mem = {}
    ext = 0
    with open(path, "r", encoding="ascii", errors="ignore") as f:
        for line in f:
            line = line.strip()
            if not line.startswith(":"):
                continue
            ll = int(line[1:3], 16)
            addr = int(line[3:7], 16)
            rtype = int(line[7:9], 16)
            data = bytes.fromhex(line[9:9 + ll * 2])
            if rtype == 0x00:
                abs_addr = (ext << 16) + addr
                for i, b in enumerate(data):
                    mem[abs_addr + i] = b
            elif rtype == 0x04:
                ext = int.from_bytes(data, "big")
            elif rtype == 0x01:
                break
    if not mem:
        raise ValueError("empty hex")
    lo = min(mem)
    hi = max(mem)
    blob = bytearray([0xFF] * (hi - lo + 1))
    for a, b in mem.items():
        blob[a - lo] = b
    return lo, bytes(blob)


class IapHost(object):
    def __init__(self, port, baud=115200, timeout=0.2):
        # Avoid DTR/RTS toggling that resets many USB-UART boards on open
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baud
        self.ser.timeout = timeout
        self.ser.dsrdtr = False
        self.ser.rtscts = False
        try:
            self.ser.dtr = False
            self.ser.rts = False
        except Exception:
            pass
        self.ser.open()
        try:
            self.ser.setDTR(False)
            self.ser.setRTS(False)
        except Exception:
            pass
        self.seq = 0
        time.sleep(0.3)
        self.ser.reset_input_buffer()

    def close(self):
        self.ser.close()

    def _tx_frame(self, cmd, payload=b""):
        self.seq = (self.seq + 1) & 0xFF
        plen = len(payload)
        body = bytes([cmd, self.seq, (plen >> 8) & 0xFF, plen & 0xFF]) + payload
        crc = crc16_modbus(body)
        self.ser.write(SOH + body + bytes([(crc >> 8) & 0xFF, crc & 0xFF]))
        self.ser.flush()
        return self.seq

    def _rx_resp(self, expect_cmd, expect_seq, timeout=5.0):
        deadline = time.time() + timeout
        buf = bytearray()
        while time.time() < deadline:
            chunk = self.ser.read(128)
            if chunk:
                buf.extend(chunk)
            while True:
                idx = buf.find(ACK)
                if idx < 0:
                    if len(buf) > 8:
                        del buf[:-7]
                    break
                if idx:
                    del buf[:idx]
                if len(buf) < 9:
                    break
                plen = (buf[5] << 8) | buf[6]
                need = 9 + plen
                if len(buf) < need:
                    break
                frame = bytes(buf[:need])
                del buf[:need]
                if crc16_modbus(frame[2:-2]) != ((frame[-2] << 8) | frame[-1]):
                    raise RuntimeError("resp CRC error")
                cmd, seq, status = frame[2], frame[3], frame[4]
                if cmd == expect_cmd and seq == expect_seq:
                    return status, frame[7:7 + plen]
        raise TimeoutError("no IAP response (hold KEY0, reset, wait IAP ready)")

    def cmd(self, cmd, payload=b"", timeout=5.0):
        seq = self._tx_frame(cmd, payload)
        return self._rx_resp(cmd, seq, timeout=timeout)

    def info(self, retries=8):
        last = None
        for i in range(retries):
            try:
                self.ser.reset_input_buffer()
                st, data = self.cmd(CMD_INFO, timeout=1.5)
                if st != 0 or len(data) < 16:
                    raise RuntimeError("INFO failed status=%d" % st)
                return struct.unpack(">IIII", data[:16])
            except Exception as e:
                last = e
                print("INFO retry %d/%d: %s" % (i + 1, retries, e))
                time.sleep(0.4)
        raise TimeoutError(
            "no IAP response. Hold KEY0, then run script (USB-UART often resets on open). Last: %s"
            % last
        )

    def erase(self, addr, size):
        st, _ = self.cmd(CMD_ERASE, struct.pack(">II", addr, size), timeout=60.0)
        if st != 0:
            raise RuntimeError("ERASE failed status=%d" % st)

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
            raise RuntimeError("JUMP failed status=%d" % st)


def main():
    ap = argparse.ArgumentParser(description="F103 UART IAP host")
    ap.add_argument("port", help="e.g. COM5")
    ap.add_argument("firmware", help="app_boot.hex or .bin")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--no-jump", action="store_true")
    ap.add_argument("--addr", type=lambda x: int(x, 0), default=None)
    args = ap.parse_args()

    base, blob = load_firmware(args.firmware)
    if args.addr is not None:
        base = args.addr
    if len(blob) & 1:
        blob += b"\xFF"

    print("FW base=0x%08X size=%d" % (base, len(blob)))
    print("Note: hold KEY0 while starting script (open COM may reset MCU).")
    host = IapHost(args.port, args.baud)
    try:
        app, size, page, ver = host.info()
        print("Board: APP=0x%08X SIZE=0x%X PAGE=0x%X VER=0x%X" % (app, size, page, ver))
        if base < app or (base + len(blob)) > (app + size):
            raise RuntimeError("firmware out of APP range")

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
        else:
            print("done (--no-jump)")
    finally:
        host.close()


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Capture TABWRITE LVGL snapshots streamed over the serial console.

Firmware trigger: press Ctrl+Shift+P on the USB keyboard connected to TABWRITE.
The firmware emits a framed RGB565/RLE16 snapshot over the ESP-IDF serial console;
this script decodes it and writes a PNG without third-party Python packages.
"""

from __future__ import annotations

import argparse
import binascii
import fcntl
import os
import re
import select
import struct
import sys
import termios
import time
import zlib
from pathlib import Path

BEGIN_RE = re.compile(rb"TABWRITE_SNAPSHOT_BEGIN\s+(.*)")
END_RE = re.compile(rb"TABWRITE_SNAPSHOT_END\b")

BAUDS = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
}
for name in ("B230400", "B460800", "B921600", "B1000000", "B2000000"):
    if hasattr(termios, name):
        BAUDS[int(name[1:])] = getattr(termios, name)


def parse_kv(payload: bytes) -> dict[str, str]:
    fields: dict[str, str] = {}
    for part in payload.decode("ascii", errors="replace").strip().split():
        if "=" in part:
            key, value = part.split("=", 1)
            fields[key] = value
    return fields


def open_serial(path: str, baud: int) -> int:
    if baud not in BAUDS and sys.platform != "darwin":
        supported = ", ".join(str(v) for v in sorted(BAUDS))
        raise SystemExit(f"Unsupported baud {baud}; supported by this platform: {supported}")

    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attrs
    iflag = 0
    oflag = 0
    lflag = 0
    cflag |= termios.CLOCAL | termios.CREAD
    cflag &= ~termios.CSIZE
    cflag |= termios.CS8
    cflag &= ~termios.PARENB
    cflag &= ~termios.CSTOPB
    if hasattr(termios, "CRTSCTS"):
        cflag &= ~termios.CRTSCTS
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = 0
    speed = BAUDS.get(baud, BAUDS[9600])
    termios.tcsetattr(fd, termios.TCSANOW, [iflag, oflag, cflag, lflag, speed, speed, cc])
    if baud not in BAUDS and sys.platform == "darwin":
        # macOS termios exposes only a small baud table; IOSSIOSPEED sets
        # arbitrary USB/UART adapter speeds such as 921600.
        IOSSIOSPEED = 0x80045402
        fcntl.ioctl(fd, IOSSIOSPEED, struct.pack("I", baud))
    termios.tcflush(fd, termios.TCIOFLUSH)
    return fd


def read_some(fd: int, deadline: float) -> bytes:
    remaining = max(0.0, deadline - time.monotonic())
    if remaining <= 0:
        raise TimeoutError("timed out waiting for serial data")
    ready, _, _ = select.select([fd], [], [], min(0.25, remaining))
    if not ready:
        return b""
    return os.read(fd, 8192)


def read_line(fd: int, buffer: bytearray, deadline: float) -> bytes:
    while True:
        line = try_read_line(fd, buffer, deadline)
        if line is not None:
            return line


def try_read_line(fd: int, buffer: bytearray, deadline: float) -> bytes | None:
    newline = buffer.find(b"\n")
    if newline >= 0:
        line = bytes(buffer[: newline + 1])
        del buffer[: newline + 1]
        return line.strip(b"\r\n")
    chunk = read_some(fd, deadline)
    if not chunk:
        return None
    buffer.extend(chunk)
    newline = buffer.find(b"\n")
    if newline >= 0:
        line = bytes(buffer[: newline + 1])
        del buffer[: newline + 1]
        return line.strip(b"\r\n")
    return None


def read_exact(fd: int, buffer: bytearray, size: int, deadline: float) -> bytes:
    while len(buffer) < size:
        buffer.extend(read_some(fd, deadline))
    data = bytes(buffer[:size])
    del buffer[:size]
    return data


def send_trigger(fd: int, command: str) -> None:
    data = command.encode("ascii")
    if not data.endswith(b"\n"):
        data += b"\n"
    os.write(fd, data)


def wait_for_begin(fd: int, timeout: float, trigger_command: str | None = None) -> tuple[dict[str, str], bytearray]:
    deadline = time.monotonic() + timeout
    buffer = bytearray()
    next_trigger = time.monotonic()
    while True:
        if trigger_command is not None and time.monotonic() >= next_trigger:
            send_trigger(fd, trigger_command)
            next_trigger = time.monotonic() + 2.0
        line = try_read_line(fd, buffer, deadline)
        if line is None:
            continue
        match = BEGIN_RE.search(line)
        if match:
            return parse_kv(match.group(1)), buffer
        if line:
            print(line.decode("utf-8", errors="replace"), flush=True)


def decode_rle16(encoded: bytes, raw_size: int) -> bytes:
    out = bytearray()
    i = 0
    while i < len(encoded) and len(out) < raw_size:
        token = encoded[i]
        i += 1
        count = (token & 0x7F) + 1
        byte_count = count * 2
        if token & 0x80:
            if i + 2 > len(encoded):
                raise ValueError("truncated RLE run")
            out.extend(encoded[i : i + 2] * count)
            i += 2
        else:
            if i + byte_count > len(encoded):
                raise ValueError("truncated RLE literal")
            out.extend(encoded[i : i + byte_count])
            i += byte_count
    if len(out) < raw_size:
        raise ValueError(f"RLE decoded {len(out)} bytes, expected {raw_size}")
    return bytes(out[:raw_size])


def png_chunk(kind: bytes, data: bytes) -> bytes:
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data) & 0xFFFFFFFF)


def rgb565_to_png(raw: bytes, width: int, height: int, stride: int, output: Path) -> None:
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # PNG filter type 0
        row = raw[y * stride : y * stride + width * 2]
        for x in range(width):
            value = row[x * 2] | (row[x * 2 + 1] << 8)
            r = (value >> 11) & 0x1F
            g = (value >> 5) & 0x3F
            b = value & 0x1F
            rows.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))

    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
    png.extend(png_chunk(b"IDAT", zlib.compress(bytes(rows), level=6)))
    png.extend(png_chunk(b"IEND", b""))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(png)


def default_output(fields: dict[str, str], directory: Path) -> Path:
    stamp = time.strftime("%Y%m%d-%H%M%S")
    name = fields.get("name", "snapshot")
    safe_name = re.sub(r"[^A-Za-z0-9_.-]+", "_", name)
    return directory / f"{stamp}_{safe_name}.png"


def main() -> int:
    parser = argparse.ArgumentParser(description="Capture a TABWRITE LVGL snapshot from serial and save PNG")
    parser.add_argument("--port", required=True, help="Serial port, e.g. /dev/cu.usbmodem11301")
    parser.add_argument("--baud", type=int, default=921600, help="Serial baud rate (default: 921600)")
    parser.add_argument("--output", type=Path, help="Output PNG path; defaults under captures/")
    parser.add_argument("--output-dir", type=Path, default=Path("captures"), help="Default output directory")
    parser.add_argument("--timeout", type=float, default=180.0, help="Seconds to wait for frame/data")
    parser.add_argument("--trigger-command", default="snapshot", help="Serial command sent before waiting (default: snapshot)")
    parser.add_argument("--no-trigger", action="store_true", help="Do not send a serial trigger; wait for Ctrl+Shift+P/manual trigger")
    args = parser.parse_args()

    fd = open_serial(args.port, args.baud)
    try:
        if args.no_trigger:
            print("Waiting for TABWRITE_SNAPSHOT_BEGIN. Press Ctrl+Shift+P on the device keyboard.", flush=True)
            trigger = None
        else:
            print(f"Sending serial trigger {args.trigger_command!r} until TABWRITE_SNAPSHOT_BEGIN appears...", flush=True)
            trigger = args.trigger_command
        fields, buffer = wait_for_begin(fd, args.timeout, trigger)
        width = int(fields["width"])
        height = int(fields["height"])
        stride = int(fields["stride"])
        raw_bytes = int(fields["raw_bytes"])
        encoded_bytes = int(fields["encoded_bytes"])
        expected_crc = int(fields["crc32"], 16)
        encoding = fields.get("encoding")
        fmt = fields.get("format")
        if fmt != "RGB565_LE" or encoding != "rle16":
            raise SystemExit(f"Unsupported snapshot format/encoding: {fmt}/{encoding}")

        print(f"Receiving {width}x{height} {fmt}, {encoded_bytes} encoded bytes...", flush=True)
        deadline = time.monotonic() + args.timeout
        encoded = read_exact(fd, buffer, encoded_bytes, deadline)
        while True:
            line = read_line(fd, buffer, deadline)
            if not line:
                continue
            if END_RE.search(line):
                break
            raise ValueError(f"missing end marker after payload: {line[:80]!r}")

        raw = decode_rle16(encoded, raw_bytes)
        actual_crc = binascii.crc32(raw) & 0xFFFFFFFF
        if actual_crc != expected_crc:
            raise ValueError(f"CRC mismatch: got {actual_crc:08x}, expected {expected_crc:08x}")

        output = args.output or default_output(fields, args.output_dir)
        rgb565_to_png(raw, width, height, stride, output)
        print(f"Saved {output} ({width}x{height}, crc32={actual_crc:08x})", flush=True)
        return 0
    finally:
        os.close(fd)


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Small PlotterFlow-compatible serial smoke test.

Requires pyserial:
    python -m pip install pyserial
"""

from __future__ import annotations

import argparse
import time

import serial


COMMANDS = [
    "M115",
    "?",
    "G21",
    "G90",
    "G10 L20 P0 X0 Y0",
    "$J=G91 G21 X1 F500",
    "G1 X10 Y10 F500",
    "M114",
]


def read_for(port: serial.Serial, seconds: float) -> str:
    deadline = time.time() + seconds
    data = bytearray()
    while time.time() < deadline:
        chunk = port.read(port.in_waiting or 1)
        if chunk:
            data.extend(chunk)
    return data.decode(errors="replace")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("port", help="COM port, for example COM7")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    with serial.Serial(args.port, args.baud, timeout=0.2) as port:
        time.sleep(1.0)
        print(read_for(port, 0.5), end="")
        for command in COMMANDS:
            print(f"> {command}")
            if command == "?":
                port.write(b"?")
            else:
                port.write((command + "\n").encode())
            print(read_for(port, 1.5), end="")


if __name__ == "__main__":
    main()

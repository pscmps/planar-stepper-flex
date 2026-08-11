#!/usr/bin/env python3
"""Check that every repository file has an intentional license category."""

from __future__ import annotations

import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

THIRD_PARTY = {
    "firmware/pico2-drv8835/third_party/pico_examples/dhcpserver.c": "MIT",
    "firmware/pico2-drv8835/third_party/pico_examples/dhcpserver.h": "MIT",
    "firmware/pico2-drv8835/third_party/pico_examples/dnsserver.c": "BSD-3-Clause",
    "firmware/pico2-drv8835/third_party/pico_examples/dnsserver.h": "BSD-3-Clause",
    "hardware/pico2w-drv8835-controller/footprints/official-kicad/"
    "RaspberryPi_Pico_W_SMD_HandSolder.kicad_mod": "LicenseRef-KiCad-Libraries",
    "hardware/pico2w-drv8835-controller/footprints/official-kicad/NOTICE":
    "LicenseRef-KiCad-Libraries",
}

REQUIRED_LICENSES = {
    "LICENSES/CERN-OHL-P-2.0.txt": ("CERN Open Hardware Licence", 8000),
    "LICENSES/MIT.txt": ("MIT License", 900),
    "LICENSES/CC-BY-4.0.txt": ("Creative Commons Attribution 4.0", 15000),
    "LICENSES/BSD-3-Clause.txt": ("Redistribution and use", 1400),
    "LICENSES/LicenseRef-KiCad-Libraries.txt": ("KiCad Libraries License", 19000),
    "LICENSES/LicenseRef-CYW43-Raspberry-Pi.txt": ("Raspberry Pi Ltd", 1500),
    "LICENSES/LicenseRef-Raspberry-Pi-Pico-SDK-BSD-3-Clause.txt": ("Redistribution", 1200),
    "LICENSES/LicenseRef-lwIP-BSD-3-Clause.txt": ("Swedish Institute", 1200),
    "LICENSES/LicenseRef-TinyUSB-MIT.txt": ("tinyusb.org", 900),
    "LICENSES/LicenseRef-MicroPython-MIT.txt": ("MicroPython", 2500),
}


def repository_files() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
        cwd=ROOT,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return sorted(line.replace("\\", "/") for line in result.stdout.splitlines())


def category(path: str) -> str | None:
    if path in THIRD_PARTY:
        return THIRD_PARTY[path]
    if path.startswith("LICENSES/") or path == "REUSE.toml":
        return "license-metadata"
    if path == "manufacturing/v0.1.0/firmware/pico2w_gcode_drv8835_wifi.uf2":
        return "MIT AND BSD-3-Clause AND LicenseRef-CYW43-Raspberry-Pi"
    if path.startswith("manufacturing/v0.1.0/firmware/") and path.endswith(".uf2"):
        return "MIT AND BSD-3-Clause"
    if path.startswith("generator/assets/") and path.endswith(".kicad_mod"):
        return "CERN-OHL-P-2.0"
    if path.startswith("hardware/") and (
        path.endswith((".kicad_pcb", ".kicad_pro", ".json", ".csv"))
        or path.endswith("/fp-lib-table")
    ):
        return "CERN-OHL-P-2.0"
    if path.startswith("manufacturing/") and path.endswith(
        (".zip", ".csv", ".rpt", "SHA256SUMS.txt")
    ):
        return "CERN-OHL-P-2.0"
    if path.startswith("firmware/") and not path.endswith(".md"):
        return "MIT"
    if path.startswith("generator/") and path.endswith(".py"):
        return "MIT"
    if path.startswith("hardware/") and path.endswith(".py"):
        return "MIT"
    if path.startswith("scripts/") or path in {".gitattributes", ".gitignore"}:
        return "MIT"
    if path.endswith((".md", ".png", ".svg")) or path == "LICENSE":
        return "CC-BY-4.0"
    return None


def main() -> int:
    errors: list[str] = []
    unmapped = [path for path in repository_files() if category(path) is None]
    errors.extend(f"unmapped: {path}" for path in unmapped)

    for path, (marker, minimum_size) in REQUIRED_LICENSES.items():
        full_path = ROOT / path
        if not full_path.exists():
            errors.append(f"missing license text: {path}")
            continue
        text = full_path.read_text(encoding="utf-8-sig")
        if marker not in text or len(text.encode("utf-8")) < minimum_size:
            errors.append(f"invalid or truncated license text: {path}")

    dhcp = (ROOT / "firmware/pico2-drv8835/third_party/pico_examples/dhcpserver.c").read_text(
        encoding="utf-8"
    )
    dns = (ROOT / "firmware/pico2-drv8835/third_party/pico_examples/dnsserver.c").read_text(
        encoding="utf-8"
    )
    if "Copyright (c) 2018-2019 Damien P. George" not in dhcp or "The MIT License" not in dhcp:
        errors.append("MicroPython DHCP copyright/license header is missing")
    if "Copyright (c) 2022 Raspberry Pi (Trading) Ltd." not in dns or "SPDX-License-Identifier: BSD-3-Clause" not in dns:
        errors.append("Raspberry Pi DNS copyright/license header is missing")

    combined = "\n".join(
        (ROOT / name).read_text(encoding="utf-8-sig")
        for name in ["README.md", "RIGHTS.md", "PUBLIC_RELEASE_CHECKLIST.md"]
    )
    forbidden = ["ライセンス未決定", "ライセンスを付与していません"]
    errors.extend(f"obsolete wording remains: {word}" for word in forbidden if word in combined)
    if "使用報告や連絡は必須ではありません" not in combined:
        errors.append("Japanese optional-use-report wording is missing")
    if "Reporting your use or contacting the author is entirely optional" not in combined:
        errors.append("English optional-use-report wording is missing")

    if errors:
        print("License scope check failed:")
        for error in errors:
            print(f"  - {error}")
        return 1

    print(f"License scope check passed: {len(repository_files())} files classified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Check that every repository file has an intentional license category."""

from __future__ import annotations

import subprocess
from hashlib import sha256
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
    "LICENSES/CERN-OHL-P-2.0.txt": (
        "CERN Open Hardware Licence",
        8000,
        "6e471d647db50527d0c75ee5f1a55a74012ab177c73282bef0bdc7f6562b7ded",
    ),
    "LICENSES/MIT.txt": (
        "Copyright (c) 2026 pscmps",
        900,
        "0c7c1974d36fa61e9471caa59531f3edcf9ed88ea051b5b304be70f89542c996",
    ),
    "LICENSES/CC-BY-4.0.txt": (
        "Creative Commons Attribution 4.0 International Public License",
        18000,
        "9ba9550ad48438d0836ddab3da480b3b69ffa0aac7b7878b5a0039e7ab429411",
    ),
    "LICENSES/BSD-3-Clause.txt": (
        "Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.",
        1400,
        "483f865953435b66c443dee7558debe3cc3cf8fcbb6a112fd9fc6a795d53f1f6",
    ),
    "LICENSES/LicenseRef-KiCad-Libraries.txt": (
        "KiCad Libraries License",
        1800,
        "45d2bce75e5a4208f5afb01b8fb2c406e700371c4fe2b5f5cd5c443d46db4d8f",
    ),
    "LICENSES/LicenseRef-CYW43-Raspberry-Pi.txt": (
        "Copyright (C) 2019-2022 George Robotics Pty Ltd",
        1500,
        "67aca4f10d9edf489871f64cd8f0dcd6c5df3e4ce75bd39e1914fc54f99e40b3",
    ),
    "LICENSES/LicenseRef-Raspberry-Pi-Pico-SDK-BSD-3-Clause.txt": (
        "Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.",
        1400,
        "483f865953435b66c443dee7558debe3cc3cf8fcbb6a112fd9fc6a795d53f1f6",
    ),
    "LICENSES/LicenseRef-lwIP-BSD-3-Clause.txt": (
        "Copyright (c) 2001, 2002 Swedish Institute of Computer Science.",
        1200,
        "ef4aac92e05e87cd1cdc140870ed52206ba03d4a7fe46c1e11d7ffa6c87d252b",
    ),
    "LICENSES/LicenseRef-TinyUSB-MIT.txt": (
        "Copyright (c) 2018, hathach (tinyusb.org)",
        900,
        "b171720e8a442e7a3957d83c62cd3299dbb29da3db534cc626f9dded0de2ca44",
    ),
    "LICENSES/LicenseRef-MicroPython-MIT.txt": (
        "Copyright (c) 2013-2022 Damien P. George",
        900,
        "92878c7a25f92e1eff293885671bca515af12fb6d80434898a2e51d5e7abc74f",
    ),
}

PLACEHOLDER_MARKERS = ("<<", ">>", "<year>", "<copyright holder>", "<owner>", "{{", "}}")


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

    for path, (marker, minimum_size, expected_hash) in REQUIRED_LICENSES.items():
        full_path = ROOT / path
        if not full_path.exists():
            errors.append(f"missing license text: {path}")
            continue
        text = full_path.read_text(encoding="utf-8-sig")
        if marker not in text or len(text.encode("utf-8")) < minimum_size:
            errors.append(f"invalid or truncated license text: {path}")
        normalized = text.replace("\r\n", "\n").replace("\r", "\n")
        actual_hash = sha256(normalized.encode("utf-8")).hexdigest()
        if actual_hash != expected_hash:
            errors.append(f"license text differs from audited source: {path}")
        for placeholder in PLACEHOLDER_MARKERS:
            if placeholder in text:
                errors.append(f"license template marker {placeholder!r} remains: {path}")

    third_party_headers = {
        "firmware/pico2-drv8835/third_party/pico_examples/dhcpserver.c": (
            "Copyright (c) 2018-2019 Damien P. George",
            "The MIT License",
        ),
        "firmware/pico2-drv8835/third_party/pico_examples/dhcpserver.h": (
            "Copyright (c) 2018-2019 Damien P. George",
            "The MIT License",
        ),
        "firmware/pico2-drv8835/third_party/pico_examples/dnsserver.c": (
            "Copyright (c) 2022 Raspberry Pi (Trading) Ltd.",
            "SPDX-License-Identifier: BSD-3-Clause",
        ),
        "firmware/pico2-drv8835/third_party/pico_examples/dnsserver.h": (
            "Copyright (c) 2022 Raspberry Pi (Trading) Ltd.",
            "SPDX-License-Identifier: BSD-3-Clause",
        ),
    }
    for path, markers in third_party_headers.items():
        source = (ROOT / path).read_text(encoding="utf-8")
        if any(marker not in source for marker in markers):
            errors.append(f"third-party copyright/license header is missing: {path}")

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

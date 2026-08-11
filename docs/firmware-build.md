# ファームウェアのビルド

Pico SDK環境をDドライブへ用意し、リポジトリ直下で実行します。

```powershell
. D:\dev\pico2-tmc2209\env.ps1
powershell -ExecutionPolicy Bypass -File scripts\build-firmware.ps1
```

生成ターゲット:

| UF2 | 用途 |
|---|---|
| `pico2_gcode_drv8835_xy.uf2` | Pico 2/Pico 2 WのUSB G-code運転 |
| `pico2_gcode_drv8835_xy_test.uf2` | GPIOと相励磁の診断 |
| `pico2w_gcode_drv8835_wifi.uf2` | Pico 2 WのUSB + Wi-Fi APジョグ |

ホスト単体テストも同じスクリプトで実行します。書き込みとG-code仕様は[ファームウェアREADME](../firmware/pico2-drv8835/README.md)を参照してください。

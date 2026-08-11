# Pico 2 / Pico 2 W DRV8835 G-codeファームウェア

4個の`AE-DRV8835-S`で平面コイルのA/B/C/D相を駆動し、USBシリアルまたはPico 2 W内蔵Web UIから操作します。TMC2209用ターゲットはこの公開準備リポジトリに含めません。

## ターゲット

| ターゲット | 出力UF2 | 用途 |
|---|---|---|
| `pico2_gcode_drv8835_xy` | `pico2_gcode_drv8835_xy.uf2` | USB G-code通常運転 |
| `pico2_gcode_drv8835_xy_test` | `pico2_gcode_drv8835_xy_test.uf2` | 相単独励磁とGPIO診断 |
| `pico2w_gcode_drv8835_wifi` | `pico2w_gcode_drv8835_wifi.uf2` | USB G-code + Wi-Fi APジョグ |

ビルドはリポジトリ直下の`scripts/build-firmware.ps1`を使います。

## 対応指令

- `G0/G1 X... Y... Z... F...`
- `G2/G3`は未対応。PlotterFlow側で線分化します。
- `G20/G21`、`G90/G91`、`G92`、`G10 L20 P0`
- `$J=G91 G21 X... Y... F...`
- `M17/M18`、`M3/M5`、`M281`、`M980`、`M982`、`M983`
- `?`、`!`、`~`、ジョグキャンセル`0x85`

通常移動は汎用G-codeだけで行います。独自命令は起動時設定と診断に限定します。

## DRV8835設定 M980

```gcode
M18
M980 U8 X60 Y60 I100 J100 H500 C100 B8 R16 A1
M17
```

| 引数 | 意味 |
|---|---|
| `U` | 1フル状態の分割数。`1/2/4/8/16` |
| `X/Y` | X/Y通常PWM、1～100% |
| `I/J` | X/Y初動PWM、1～100% |
| `H` | 移動終了後の保持時間、0～5000 ms |
| `C` | Hi-Z解除時の捕捉時間、0～500 ms |
| `B` | 初動PWMを維持する各軸マイクロステップ数 |
| `R` | 通常PWMへ線形復帰する各軸マイクロステップ数 |
| `A1` | 移動中の軸だけ励磁。`A0`は両軸保持 |

設定後は`G0/G1`と`$J`へ自動適用され、移動ごとに`M980`を送る必要はありません。`B0 R0`は初動ブーストなしの従来動作です。出力OFF、保持終了、新しい軸の励磁開始時にブーストし、保持中の同軸連続G1では再開始しません。

確実な1相励磁を優先する場合は次を使います。

```gcode
M18
M980 U1 X100 Y100 I100 J100 H500 C100 B0 R0 A1
M17
```

## Zサーボ

GP12から50 Hz PWMを出力します。既定はペンアップ1000 us、ペンダウン1800 usです。

```gcode
M281 P0 S1000   ; Up
M281 P1 S1800   ; Down
G0 Z1           ; Up
G0 Z0           ; Down
```

## 任意センサ

- GP26/ADC0: 電流センサ。`M982`で確認。未接続でも運転を継続します。
- AS5600 J1: I2C0、GP8 SDA、GP9 SCL
- AS5600 J2: I2C1、GP10 SDA、GP11 SCL
- `M983`: 1回読出し、`M983 S1/S0`: 状態応答への連続掲載ON/OFF

AS5600が片方または両方なくてもエラー停止しません。詳細な配線は[`../../docs/wiring.md`](../../docs/wiring.md)を参照してください。

## Wi-Fi版

- SSID: `PlanarStepper-XXXX`
- パスワード: `planar-stepper`
- URL: `http://192.168.4.1/`

Pico 2 W自身がAP、DHCP、Web UIを提供します。接続手順とAPIは[`docs/pico2w_wifi_jog.md`](docs/pico2w_wifi_jog.md)にあります。

## 書き込み

BOOTSELを使わず、USBシリアルから同じPicoを特定してpicotoolで再起動・書き込みできます。

```powershell
powershell -ExecutionPolicy Bypass -File tools\flash_wifi.ps1 -Port COM6 -Picotool D:\path\to\picotool.exe -Uf2 D:\path\to\pico2w_gcode_drv8835_wifi.uf2
```

書き込み中はVMとサーボ電源を切ってください。USBデバイスが列挙されない場合はBOOTSEL書き込みが必要です。

## 実機確認

[`docs/drv8835_gcode_bringup.md`](docs/drv8835_gcode_bringup.md)の順に、VM OFFでGPIO Low、低電圧・電流制限、X、Y、XY、停止を確認します。

# 制御基板の人間確認手順

1. 外形80 x 85 mm、角R5 mm、固定穴、Pico 2 Wと4モジュールの向きを実物へ重ねて確認する。
2. X出力が`A+ A- B+ B-`、Y出力が`C+ C- D+ D-`の順であることを確認する。
3. VM_XとVM_Yがジャンパ開放時に絶縁され、GNDは共通であることをテスターで確認する。
4. VM-GND、3.3V-GND、異なる相出力間に短絡がないことを確認する。
5. 電解コンデンサ、Pico 2 W、AE-DRV8835-Sの向きを確認する。
6. [`../../manufacturing/v0.1.0/pico2w-drv8835-controller/drc.rpt`](../../manufacturing/v0.1.0/pico2w-drv8835-controller/drc.rpt)が違反0、未配線0であることを確認する。
7. 製造業者のGerberビューアで銅箔、マスク、シルク、PTH/NPTHを確認する。
8. VM OFFでPicoを起動し、GP15～GP22と全DRV8835入力がLowであることを確認する。
9. 各相へ直列抵抗を入れ、低電圧・電流制限からX、Yの順に通電する。

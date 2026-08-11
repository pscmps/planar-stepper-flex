# 単独ドライバ基板の人間確認手順

1. 外形48 x 60 mm、角R3 mm、φ3.2 mm固定穴4個を確認する。
2. J1上段が`VCC GND A1 A2 B1 B2`、下段が`VCC GND C1 C2 D1 D2`であることを確認する。
3. J4が`A+ A- B+ B-`、J5が`C+ C- D+ D-`であることを確認する。
4. AE-DRV8835-Sの2.54 mmピッチ、列間7.62 mm、向きを実物で確認する。
5. VM_XとVM_Yがジャンパ開放時に絶縁され、GNDは共通であることを確認する。
6. [`../../manufacturing/v0.1.0/drv8835-driver-module/drc.rpt`](../../manufacturing/v0.1.0/drv8835-driver-module/drc.rpt)が違反0、未配線0であることを確認する。
7. VM OFFで全入力のプルダウンと出力端子間の短絡がないことを確認する。
8. 直列抵抗と電流制限を使い、1相ずつ短時間通電する。

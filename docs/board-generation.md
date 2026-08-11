# FPC基板生成

## プリセット生成

KiCad 10付属Pythonを使います。

```powershell
D:\kicad10\bin\python.exe generator\generate_flex_board.py --preset 50mm --output hardware\flex-50mm\one_axis_stepper.kicad_pcb
D:\kicad10\bin\python.exe generator\generate_flex_board.py --preset 4cards --with-art --output hardware\flex-4cards\planar_stepper_flex_4cards.kicad_pcb
```

`--with-art`は名刺4枚プリセットへだけ指定できます。手描きシルクは`generator/assets/hand-drawn-silkscreen.kicad_mod`から注入されます。

## 任意寸法

```powershell
D:\kicad10\bin\python.exe generator\generate_flex_board.py --preset 50mm --active-width 100 --active-height 80 --output build\flex-100x80.kicad_pcb
```

有効配線領域を指定し、外形を省略した場合は折り返しと端子用に各方向40 mmを加えます。必要に応じて`--board-width`、`--board-height`、`--pitch`、`--trace-width`、`--spacing`、`--phase-offset`を指定できます。全オプションは`--help`で確認してください。

## 検証

```powershell
python -m unittest discover -s generator\tests -v
```

生成後は必ず`kicad-cli pcb drc`を実行してください。製造データまで一括生成する場合は`docs/manufacturing.md`を参照します。

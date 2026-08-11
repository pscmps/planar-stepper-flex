# 製造データ

`scripts/build-all.ps1`はFPCを再生成し、単体テスト、4基板のDRC、表裏PNG、Gerber、ドリル、ZIP、SHA-256をまとめて更新します。

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build-all.ps1 `
  -KicadCli D:\kicad10\bin\kicad-cli.exe `
  -KicadPython D:\kicad10\bin\python.exe
```

DRC違反または未配線が1件でもある場合、ZIP生成は失敗します。成果物は`manufacturing/v0.1.0/`へ出力されます。

Gerber ZIPは機械的に生成済みでも、そのまま発注可能という保証ではありません。FPCの銅厚、基材厚、補強板、カバーレイ、最小曲げ半径と、制御基板の部品向き・穴径を製造業者のプレビューで確認してください。

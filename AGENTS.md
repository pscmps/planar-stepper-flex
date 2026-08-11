# 作業ルール

- ユーザー向け文書は日本語で記述する。
- `hardware/flex-50mm/`と`hardware/flex-4cards/`のPCBは手編集せず、`generator/generate_flex_board.py`から再生成する。
- 名刺4枚版の手描きシルクは`generator/assets/hand-drawn-silkscreen.kicad_mod`を正本とする。
- 基板変更後は`kicad-cli pcb drc`で違反0、未配線0を確認してから製造ZIPを更新する。
- VM、コイル電源、サーボ電源を切った状態で書き込みと導通確認を行う。
- TMC2209、コイルマトリクス、M5Stack用成果物をこのリポジトリへ追加しない。
- 公開前にライセンス、第三者由来データ、個人情報、製造上の注意を再確認する。

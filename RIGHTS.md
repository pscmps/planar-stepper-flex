# ライセンスと権利

Planar Stepper Flexは、成果物の種類に応じた複数ライセンスで提供します。Copyright (c) 2026 pscmps.

## ハードウェア設計

プロジェクト独自のハードウェア設計は[CERN Open Hardware Licence Version 2 - Permissive](LICENSES/CERN-OHL-P-2.0.txt)、SPDX識別子`CERN-OHL-P-2.0`で提供します。

対象:

- `hardware/`以下の`.kicad_pcb`、`.kicad_pro`、配線マニフェスト、BOMなどの設計データ
- `generator/assets/hand-drawn-silkscreen.kicad_mod`
- FPC、回路、PCB、Gerber、ドリル、製造ZIPなど、それらから生成されたハードウェア設計データ

製造、改造、再配布、商用利用が可能です。CERN-OHL-P-2.0はpermissive版であり、改造設計を同じライセンスで公開することを要求しません。ただし、ライセンス本文が定めるNotice保持、変更表示、製品受領者からNoticeへアクセスできる状態などの条件は守ってください。

## ソフトウェア

プロジェクト独自のソフトウェアは[MIT License](LICENSES/MIT.txt)、SPDX識別子`MIT`で提供します。

対象:

- `firmware/`のプロジェクト独自ソース、Web UI、ツール、テスト、G-code例
- `generator/`のPythonコード。ただしハードウェア設計アセットは前項
- `hardware/`内の基板生成・自動配線Pythonスクリプト
- `scripts/`とその他のプロジェクト独自ソフトウェア

`firmware/pico2-drv8835/third_party/`などの第三者コードには元のライセンスが適用され、プロジェクトのMIT Licenseで上書きしません。UF2はプロジェクト独自MITコードと第三者コンポーネントを含む複合物です。

## 文書と画像

プロジェクト独自の文書と説明画像は[Creative Commons Attribution 4.0 International](LICENSES/CC-BY-4.0.txt)、SPDX識別子`CC-BY-4.0`で提供します。

対象:

- `README.md`、`AGENTS.md`、`PUBLIC_RELEASE_CHECKLIST.md`、`docs/`
- 各サブディレクトリのREADME、確認手順、説明文書
- プロジェクトで新規作成したPNG、SVG、基板プレビュー、説明図

推奨する表示例は「Planar Stepper Flex by pscmps, CC BY 4.0, https://github.com/pscmps/planar-stepper-flex」です。これはCC BY 4.0の表示を分かりやすくする例であり、ライセンスが要求する範囲を超える追加条件ではありません。

## 第三者成果物と例外

第三者由来のコード、ライブラリ、フットプリント、資料、画像、商標には上記のプロジェクトライセンスを適用しません。[第三者通知](THIRD_PARTY_NOTICES.md)と各ソース内の著作権・SPDXヘッダーを優先してください。

特に、KiCad公式ライブラリ由来フットプリントはCC BY-SA 4.0とKiCad libraries exceptionの組み合わせです。KiCadの例外により、そのライブラリ要素を使って作成した基板設計と生成ファイルへCERN-OHL-P-2.0を適用できますが、収録したフットプリント単体はKiCad側の条件を維持します。`REUSE.toml`では、この組み合わせを`LicenseRef-KiCad-Libraries`として表します。

Pico 2 W Wi-Fi版UF2に含まれる`cyw43-driver`は`LicenseRef-CYW43-Raspberry-Pi`です。Raspberry Pi Ltd製のRP2040、RP2350その他同社製半導体との組み合わせに限定して利用・再配布できます。この制約はWi-Fiバイナリ内の第三者部分に由来し、プロジェクト独自ソースのMIT Licenseを変更するものではありません。

正式な対応表は[`REUSE.toml`](REUSE.toml)にも記載しています。第三者ファイルを含め、対象ごとの情報を`precedence = "override"`で一意にしています。ソース内の著作権・ライセンス表示も維持しています。

## 使用報告

この設計を使って何か作った場合、作品や写真を教えていただけると嬉しいです。使用報告や連絡は必須ではありません。

If you build, modify, or incorporate this design into a project, the author would be happy to hear about it and see photos of your work. Reporting your use or contacting the author is entirely optional.

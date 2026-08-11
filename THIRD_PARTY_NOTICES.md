# 第三者通知

プロジェクト独自のライセンスは、次の第三者成果物を上書きしません。ソース内の著作権表示とライセンスヘッダーは変更せず維持しています。

## リポジトリへ直接収録しているもの

| 成果物 | 収録場所 | 著作権・ライセンス | 本文 |
|---|---|---|---|
| MicroPython DHCP server由来コード | `firmware/pico2-drv8835/third_party/pico_examples/dhcpserver.c/.h` | Copyright (c) 2018-2019 Damien P. George、MIT | [LicenseRef-MicroPython-MIT.txt](LICENSES/LicenseRef-MicroPython-MIT.txt) |
| Raspberry Pi Pico examples DNS server | `firmware/pico2-drv8835/third_party/pico_examples/dnsserver.c/.h` | Copyright (c) 2022 Raspberry Pi (Trading) Ltd.、BSD-3-Clause | [BSD-3-Clause.txt](LICENSES/BSD-3-Clause.txt) |
| KiCad公式Pico Wフットプリント | `hardware/pico2w-drv8835-controller/footprints/official-kicad/` | KiCad library contributors、CC-BY-SA-4.0 + KiCad libraries exception | [LicenseRef-KiCad-Libraries.txt](LICENSES/LicenseRef-KiCad-Libraries.txt) |

KiCadフットプリントの形状はRaspberry Pi Pico W/Pico 2 W公式寸法資料を参照しています。Raspberry Piの名称、商標、データシート、製品設計に対する権利は各権利者に帰属し、このリポジトリのライセンス対象ではありません。

## ビルド依存と配布UF2

次の依存物はリポジトリへソースを複製していませんが、Pico SDKを通してファームウェアのビルドまたは配布UF2へ組み込まれます。

| 依存物 | ライセンス | 本文 |
|---|---|---|
| Raspberry Pi Pico SDK | BSD-3-Clause | [LicenseRef-Raspberry-Pi-Pico-SDK-BSD-3-Clause.txt](LICENSES/LicenseRef-Raspberry-Pi-Pico-SDK-BSD-3-Clause.txt) |
| lwIP | BSD-3-Clause系 | [LicenseRef-lwIP-BSD-3-Clause.txt](LICENSES/LicenseRef-lwIP-BSD-3-Clause.txt) |
| TinyUSB | MIT | [LicenseRef-TinyUSB-MIT.txt](LICENSES/LicenseRef-TinyUSB-MIT.txt) |
| cyw43-driver | Raspberry Pi製半導体との組み合わせに限定した再配布許諾 | [LicenseRef-CYW43-Raspberry-Pi.txt](LICENSES/LicenseRef-CYW43-Raspberry-Pi.txt) |

`manufacturing/v0.1.0/firmware/`のUF2を再配布する場合も、この第三者通知と該当ライセンス本文を一緒に提供してください。実際にリンクされるコンポーネントはPico SDKのバージョンとCMake設定に依存します。特にWi-Fi版UF2の`cyw43-driver`は、Pico 2 Wを含むRaspberry Pi Ltd製半導体と組み合わせて使用・再配布する場合に限られます。

## KiCad標準ライブラリを使った設計

KiCad標準ライブラリはCC BY-SA 4.0にKiCad libraries exceptionを加えた条件です。この例外は、ライブラリデータを使って作成した回路・基板設計や生成ファイルを別ライセンスで提供することを認めています。そのため、このプロジェクト独自の基板設計とGerberにはCERN-OHL-P-2.0を適用します。再配布するKiCadライブラリ要素そのものには元のライセンスを維持します。

## 外部リンク

READMEや`docs/references.md`から参照する外部記事、データシート、Webサイトの内容や画像は、このリポジトリのライセンス対象ではありません。現時点で第三者記事の画像や本文はリポジトリへ転載していません。

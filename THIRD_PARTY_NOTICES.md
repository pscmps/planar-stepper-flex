# 第三者通知

プロジェクト独自のライセンスは、次の第三者成果物を上書きしません。ソース内の著作権表示とライセンスヘッダーは変更せず維持しています。

## リポジトリへ直接収録しているもの

| 成果物 | 収録場所 | 収録ファイルの著作権・ライセンス | 正式本文と由来 |
|---|---|---|---|
| MicroPython由来DHCP server | `firmware/pico2-drv8835/third_party/pico_examples/dhcpserver.c/.h` | ファイルヘッダーはCopyright (c) 2018-2019 Damien P. George、MIT | pico-examplesの当該ディレクトリに付属するLICENSE（Copyright (c) 2013-2022 Damien P. George）を[LicenseRef-MicroPython-MIT.txt](LICENSES/LicenseRef-MicroPython-MIT.txt)へ原文のまま収録 |
| Raspberry Pi Pico examples DNS server | `firmware/pico2-drv8835/third_party/pico_examples/dnsserver.c/.h` | ファイルヘッダーはCopyright (c) 2022 Raspberry Pi (Trading) Ltd.、`BSD-3-Clause` | pico-examplesルートのLICENSE.TXT（Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.）を[BSD-3-Clause.txt](LICENSES/BSD-3-Clause.txt)へ原文のまま収録 |
| KiCad公式Pico Wフットプリント | `hardware/pico2w-drv8835-controller/footprints/official-kicad/` | KiCad library contributors、CC BY-SA 4.0とKiCad libraries exception | KiCad公式`LICENSE.md`を[LicenseRef-KiCad-Libraries.txt](LICENSES/LicenseRef-KiCad-Libraries.txt)へ原文のまま収録 |

DHCP/DNSの派生元は、Raspberry Pi公式pico-examplesのコミット`eed0c298202ddbdc3aadb386abedb71ae528e5fb`です。DHCP付属LICENSEはコミット`1c5d9aa567598e6e3eadf6d7f2d8a9342b44dab4`、pico-examplesルートLICENSE.TXTはコミット`46078742c7f8dea8b5a0998c73b38ff970fb1b64`の原文を確認しました。収録DHCPコードには再接続時の要求IP補完、ホスト名・送受信診断ログ、初期化結果の返却を追加しています。DNSコードの差分は未使用引数警告の抑制です。元の著作権・ライセンスヘッダーは変更していません。

`BSD-3-Clause.txt`とPico SDK付属LICENSEの冒頭にある`Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.`は、年が重複して見えますがRaspberry Pi公式原文の表記です。このリポジトリでは訂正せず、一字一句そのまま収録しています。

KiCadフットプリントは、公式コミット`18227ee1af2680cab46c6ed908dd53e9272a6116`の同名ファイルと比較し、2個のkeep-out zoneから`F.Paste`と`B.Paste`を除いた差分だけであることを確認しました。詳細は同ディレクトリの`NOTICE`を参照してください。

KiCadフットプリントの形状はRaspberry Pi Pico W/Pico 2 W公式寸法資料を参照しています。Raspberry Piの名称、商標、データシート、製品設計に対する権利は各権利者に帰属し、このリポジトリのライセンス対象ではありません。

## ビルド依存と配布UF2

次の依存物はリポジトリへソースを複製していませんが、Pico SDKを通してファームウェアのビルドまたは配布UF2へ組み込まれます。

| 依存物 | 確認したリビジョン | 著作権・ライセンス | 依存元から複製した本文 |
|---|---|---|---|
| Raspberry Pi Pico SDK | `a1438dff1d38bd9c65dbd693f0e5db4b9ae91779` | Copyright 2020 (c) 2020 Raspberry Pi (Trading) Ltd.、BSD-3-Clause | [LicenseRef-Raspberry-Pi-Pico-SDK-BSD-3-Clause.txt](LICENSES/LicenseRef-Raspberry-Pi-Pico-SDK-BSD-3-Clause.txt) |
| lwIP | `77dcd25a72509eb83f72b033d219b1d40cd8eb95` | Copyright (c) 2001, 2002 Swedish Institute of Computer Science、BSD系3条項 | [LicenseRef-lwIP-BSD-3-Clause.txt](LICENSES/LicenseRef-lwIP-BSD-3-Clause.txt) |
| TinyUSB | `86ad6e56c1700e85f1c5678607a762cfe3aa2f47` | Copyright (c) 2018, hathach (tinyusb.org)、MIT | [LicenseRef-TinyUSB-MIT.txt](LICENSES/LicenseRef-TinyUSB-MIT.txt) |
| cyw43-driver | `dd7568229f3bf7a37737b9e1ef250c26efe75b23` | Copyright (C) 2019-2022 George Robotics Pty Ltd、Raspberry Pi製半導体との組み合わせに限定した許諾 | [LicenseRef-CYW43-Raspberry-Pi.txt](LICENSES/LicenseRef-CYW43-Raspberry-Pi.txt) |

`manufacturing/v0.1.0/firmware/`のUF2を再配布する場合も、この第三者通知と該当ライセンス本文を一緒に提供してください。実際にリンクされるコンポーネントはPico SDKのバージョンとCMake設定に依存します。特にWi-Fi版UF2の`cyw43-driver`は、Pico 2 Wを含むRaspberry Pi Ltd製半導体と組み合わせて使用・再配布する場合に限られます。

## KiCad標準ライブラリを使った設計

KiCad標準ライブラリはCC BY-SA 4.0にKiCad libraries exceptionを加えた条件です。この例外は、ライブラリデータを使って作成した回路・基板設計や生成ファイルを別ライセンスで提供することを認めています。そのため、このプロジェクト独自の基板設計とGerberにはCERN-OHL-P-2.0を適用します。再配布するKiCadライブラリ要素そのものには元のライセンスを維持します。

## 外部リンク

READMEや`docs/references.md`から参照する外部記事、データシート、Webサイトの内容や画像は、このリポジトリのライセンス対象ではありません。現時点で第三者記事の画像や本文はリポジトリへ転載していません。

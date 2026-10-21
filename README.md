# DEN-HOME ENV NODE

**ESP32-C3 Environmental Monitoring Node / ESP32-C3 環境監視ノード**

**Firmware / ファームウェア:** `v2.0 beta7` (`DEN_HOME_ENV_NODE_v2_0_7.ino`)

[日本語](#日本語) | [English](#english)

> **Hardware revision notice / 基板リビジョンに関する注意**  
> This README documents the GPIO assignments in the published **v2.0 beta7** sketch. The prototype custom PCB routes I²C to GPIO20/21 instead, so it requires matching firmware changes. Check the `#define` values against your hardware before flashing.  
> このREADMEは公開中の **v2.0 beta7** のGPIO割り当てに基づきます。試作発注したオリジナルPCBではI²C配線にGPIO20/21を使う設計のため、そのままのコードでは一致しません。実装・書き込み前にコード内の `#define` と基板の接続を照合してください。

---

# 日本語

## 概要

DEN-HOME ENV NODEは、**DEN-HOME LAB**の一環として開発しているESP32-C3ベースの環境監視システムです。温度・湿度・気圧などを計測し、Wi-FiまたはWIZnet Ethernet経由でWeb画面、JSON API、Prometheus形式のメトリクスを提供します。

**このプロジェクトの原点は、Arduinoを使用した気圧観測ノードの製作でした。** 私自身が片頭痛持ちであり、日々の気圧の変化を観測・記録したいと考えたことが開発のきっかけです。

当初はArduinoを用いた小規模な観測から始まりましたが、観測地点を増やすためESP32へ移行しました。現在は複数の場所にノードを設置できる構成へと発展させ、温度・湿度・気圧をまとめて扱える環境監視システムを目指しています。

OLEDへのローカル表示やファームウェアのOTA更新にも対応しています。ネットワーク方式やオプション機能は、ソースコードの設定で切り替えます。

## 主な機能

- AHT20：温度・相対湿度
- BMP280：温度・気圧（I²Cアドレス `0x77` を試し、失敗時は `0x76` を試行）
- SCD40：CO₂・温度・相対湿度（オプション、安定性を検証中）
- Wi-Fi / W5500 Ethernet / W5200 Ethernetから**1方式をビルド時に選択**
- 固定IPまたはDHCP、再接続・再試行機能
- Webダッシュボード、`/status`（JSON）、`/metrics`（Prometheus）
- ArduinoOTA（Wi-Fiのみ）、Web OTA（Wi-Fi・Ethernet）
- 0.42インチ内蔵SSD1306 OLED、1.3インチ外付けSH1106 OLED（いずれもオプション）
- 5秒ごとのセンサ読み取り、5分間隔×36点の気圧履歴（最大約3時間、RAM保持）
- シリアルログの項目別ON/OFF設定

## 対応ハードウェアと接続

| 部品 | 用途・備考 |
| --- | --- |
| ESP32-C3 | メインマイコン |
| AHT20 | 温度・湿度センサ、I²C |
| BMP280 | 温度・気圧センサ、I²C |
| W5500 | Ethernet接続（本ファームウェアで使用） |
| W5200 | コード上は選択可能、実機動作未検証 |
| SCD40 | CO₂センサ、オプション |
| SSD1306 72×40 | 本体内蔵0.42インチOLED、オプション |
| SH1106 128×64 | 外付け1.3インチOLED、オプション |

**v2.0 beta7のGPIO定義（ソースコードに記載された値）**

| 接続先 | 信号 | ESP32-C3 GPIO |
| --- | --- | ---: |
| センサおよび外付けOLED | SDA | **8** |
| センサおよび外付けOLED | SCL | **9** |
| 内蔵0.42インチOLED | SDA | **5** |
| 内蔵0.42インチOLED | SCL | **6** |
| W5500/W5200 | MISO | **7** |
| W5500/W5200 | CS | **1** |
| W5500/W5200 | SCLK | **3** |
| W5500/W5200 | MOSI | **4** |
| W5500/W5200 | RST | **0** |

- 内蔵OLEDはソフトウェアI²C、外付けOLEDとセンサは `Wire` を共有します。
- W5500モジュールの**電源電圧は実物の仕様を必ず確認**してください。DEN-HOME LABで使用する3.3V専用W5500 Liteには5Vを印加しないでください。
- オリジナルPCBの別リビジョンでI²CにGPIO20/21を使用する場合、上記の設定とは異なります。**PCBの配線に合わせてファームウェア側も変更が必要**です。

## 開発環境とライブラリ

1. Arduino IDEをインストールします。
2. ボードマネージャーからEspressif Systemsの **esp32** ボードパッケージを導入します。
3. 実際のESP32-C3ボードに合ったボード設定を選択します。
4. 必要なライブラリをライブラリマネージャーなどで導入します。

| ライブラリ | 条件 |
| --- | --- |
| `Adafruit AHTX0` | 基本センサ |
| `Adafruit BMP280 Library` | 基本センサ |
| `Ethernet` | W5500 / W5200を選択する場合 |
| `ArduinoMDNS` | Ethernetビルド（このソースでは読み込みが必要） |
| `U8g2` | いずれかのOLEDを有効にする場合 |
| `SensirionI2cScd4x` | SCD40を有効にする場合 |

`Wire`、`SPI`、`WiFi`、`WebServer`、`ESPmDNS`、`ArduinoOTA`、`Update` などは使用するESP32 Arduino環境に含まれる機能です。各ライブラリの依存ライブラリも、導入時の案内に従って追加してください。

## 導入と初期設定

1. `DEN_HOME_ENV_NODE_v2_0_7.ino` をArduino IDEで開きます。
2. ソース冒頭の **Node-Specific Configuration** を編集します。
3. `NETWORK_WIFI` / `NETWORK_W5500` / `NETWORK_W5200` のうち**必ず1つだけ** `1` にします。
4. `NODE_NAME` をノードごとに設定します。
5. Wi-Fiの場合は `WIFI_SSID` と `WIFI_PASS` を設定します。
6. 固定IPを使用する場合は `local_IP`、`gateway`、`subnet`、DNSなどをネットワークに合わせて変更します。
7. Web OTAを使用する場合は `WEB_OTA_USERNAME` と `OTA_PASSWORD` に独自の認証情報を設定します。
8. 必要なセンサ・OLED機能を有効化し、書き込んで動作確認します。

```cpp
// ネットワーク方式は1つだけ有効化
#define NETWORK_WIFI   0
#define NETWORK_W5500  1
#define NETWORK_W5200  0

// ノード固有の名前
#define NODE_NAME "ETH-TEST01"

// 1 = 固定IP、0 = DHCP
#define USE_STATIC_IP 1

// オプション
#define HAS_ONBOARD_OLED  0
#define HAS_EXTERNAL_OLED 0
#define HAS_SCD40        0
#define ENABLE_WEB_OTA   1
```

**IPアドレスに注意：** アップロードされたソースの `local_IP` 初期値は `192.168.1.254` です。これはDEN-HOME LABで計画したEthernet用 `.231～.249` の範囲とは異なります。固定IP運用では使用環境に合わせて、重複しないアドレスへ必ず変更してください。

## Web画面と監視API

ネットワークに接続したあと、次のURLを使用できます（標準ポート80）。

| URL | 内容 |
| --- | --- |
| `http://<node-ip>/` | 温湿度・気圧・ネットワーク状態と気圧トレンドのWeb画面 |
| `http://<node-ip>/status` | 監視用JSON（センサ値、ノード情報、リンク状態、稼働時間など） |
| `http://<node-ip>/metrics` | Prometheusテキスト形式のメトリクス |
| `http://<node-ip>/update` | ブラウザからのファームウェア更新（有効時） |

`/status/`、`/metrics/`、`/update/` のような末尾スラッシュ付きURLも処理します。

主なPrometheusメトリクス：

- `den_env_temperature_celsius`
- `den_env_humidity_percent`
- `den_env_pressure_hpa`
- `den_env_co2_ppm`（SCD40有効時）
- `den_wifi_rssi_dbm`（Wi-Fiビルド）
- `den_network_link_up`
- `den_network_info`
- `den_uptime_seconds`
- `den_free_heap_bytes`
- `den_sensor_present`
- `den_pressure_history_points`

監視対象からPrometheusの `http://<node-ip>/metrics` を取得することで利用できます。Grafanaで表示する場合は、通常Prometheusをデータソースとして利用します。

## ファームウェア更新（OTA）

- **ArduinoOTA：** Wi-Fiビルドで使用できます。
- **Web OTA：** Wi-Fi / Ethernetの両方で利用可能です。`ENABLE_WEB_OTA` を `1` に設定します。
- Arduino IDEの**コンパイル済みバイナリのエクスポート**機能で生成した、アプリケーション本体の `.bin` ファイルを使用してください。bootloaderやpartition、mergedイメージではありません。
- Ethernetではブラウザ画面が `/update`、実際のバイナリ送信先が `/update-bin` です（ブラウザ側で処理します）。
- 更新中は電源やネットワークを切断しないでください。

**セキュリティ：** このファームウェアのWeb OTAはHTTP上のBasic認証を使用します。HTTPSによる通信の暗号化は実装されていません。インターネットへポート公開せず、信頼できるLANまたは適切に保護されたVPN内で使用してください。空のOTAパスワードにすると認証が無効になるため、公開・実運用では固有の強いパスワードを設定してください。Wi-Fi資格情報やパスワードをソースコード・Git履歴にコミットしないでください。

## 既知の制限・検証中の機能

- **Ethernet mDNS：** `ArduinoMDNS` を使った実験機能です。`<node-name>.local` の名前解決やHTTPサービス広告はネットワーク環境によって成功しない場合があります。接続できない場合はIPアドレスを使用してください。
- **W5200：** ビルド時に選択できますが、このバージョンのヘッダーでは実機未検証とされています。
- **SCD40：** オプション対応です。開発中、一部のOLED/SCD40併用構成で応答が不安定になる現象を確認しており、原因と対策を継続調査中です。安定運用を優先する場合は `HAS_SCD40 0` を推奨します。
- **気圧履歴：** RAM上に保存されるため、再起動すると消去されます。
- **カスタムPCB：** 試作基板は発注済みですが、実装・動作確認は未完了です。設計上のI²C配線はGPIO20/21で、公開版v2.0 beta7のGPIO8/9とは異なります。

## 開発の経緯

1. **Arduinoによる気圧観測：** 片頭痛と日々の気圧変化を記録するため、気圧センサを用いた観測を開始。
2. **ESP32への移行：** 観測地点の追加を見据え、ネットワークに接続できる環境監視ノードへ発展。
3. **Wi-Fi / Ethernet対応：** 設置場所や通信安定性に応じて通信方式を選択できる共通ファームウェアを開発。
4. **オリジナルPCBの設計：** ESP32-C3とW5500を搭載する専用基板を設計し、試作を発注。**実機検証はこれから**。

※開発の記録はDEN-HOME LABのブログでも紹介する予定です。

## 更新履歴

- **v2.0 beta7（このREADMEの対象）：** Wi-Fi初期接続時にもHTTP / mDNS / ArduinoOTAのサービスが開始されるよう修正。
- **v2.0 beta6：** Ethernet mDNSの実験的対応を追加。
- **v2.0 beta5：** HTTPエンドポイントで末尾スラッシュ付きURLを受け付けるよう改善。
- **v2.0 beta4：** W5500で大きなHTTP応答が途中で切れる問題に対処。

## ライセンス

ライセンスはまだこのREADMEでは指定していません。再利用・改変・再配布を認める場合は、方針に合ったライセンスを選択し、リポジトリに `LICENSE` ファイルを追加してください。

---

# English

## Overview

DEN-HOME ENV NODE is an **ESP32-C3-based environmental monitoring system** developed as part of the **DEN-HOME LAB** project. It measures temperature, humidity, and atmospheric pressure and provides a web dashboard, JSON API, and Prometheus metrics over Wi-Fi or WIZnet Ethernet.

**The project began as a simple Arduino-based atmospheric pressure monitoring node.** I experience migraines and wanted a way to observe and record changes in atmospheric pressure over time. That personal motivation became the starting point for this project.

As I wanted to expand the number of observation points, I moved from Arduino to ESP32. The project has since grown toward a network of monitoring nodes that can be deployed in different locations and collect multiple types of environmental data.

Optional OLED displays and over-the-air firmware updates are also supported. The network transport and optional features are selected at compile time.

## Features

- **AHT20:** temperature and relative humidity
- **BMP280:** temperature and atmospheric pressure (tries I²C address `0x77`, then `0x76`)
- **SCD40:** CO₂, temperature, and humidity (optional; stability is still being investigated)
- Compile-time selection of **Wi-Fi**, **W5500 Ethernet**, or **W5200 Ethernet** (select exactly one)
- Static IP or DHCP with network reconnection/retry handling
- Web dashboard, `/status` JSON API, and `/metrics` Prometheus endpoint
- ArduinoOTA on Wi-Fi; browser-based Web OTA on Wi-Fi and Ethernet
- Optional onboard **0.42-inch SSD1306 72×40 OLED** and external **1.3-inch SH1106 128×64 OLED**
- Sensor polling every **5 seconds**; pressure history sampled every **5 minutes** for **36 points** (up to about 3 hours, stored in RAM)
- Configurable serial diagnostic output

## Hardware and Wiring

| Component | Role / Notes |
| --- | --- |
| ESP32-C3 | Main microcontroller |
| AHT20 | Temperature/humidity sensor, I²C |
| BMP280 | Temperature/pressure sensor, I²C |
| W5500 | Ethernet interface used in this firmware |
| W5200 | Selectable in firmware; hardware not yet verified |
| SCD40 | Optional CO₂ sensor |
| SSD1306 72×40 | Optional onboard 0.42-inch OLED |
| SH1106 128×64 | Optional external 1.3-inch OLED |

**GPIO assignments in the published v2.0 beta7 source:**

| Device | Signal | ESP32-C3 GPIO |
| --- | --- | ---: |
| Sensors and external OLED | SDA | **8** |
| Sensors and external OLED | SCL | **9** |
| Onboard 0.42-inch OLED | SDA | **5** |
| Onboard 0.42-inch OLED | SCL | **6** |
| W5500/W5200 | MISO | **7** |
| W5500/W5200 | CS | **1** |
| W5500/W5200 | SCLK | **3** |
| W5500/W5200 | MOSI | **4** |
| W5500/W5200 | RST | **0** |

The onboard OLED uses software I²C. The external OLED and sensors share the hardware `Wire` bus.

**Check the power requirements of your exact Ethernet module.** The 3.3V-only W5500 Lite modules used by DEN-HOME LAB must not be powered from 5V.

**PCB revisions may differ:** A separately designed custom PCB may route I²C to GPIO20/21. If you use that PCB revision, update the firmware pin definitions to match before flashing.

## Development Environment and Dependencies

1. Install Arduino IDE.
2. Install the **esp32** boards package by Espressif Systems using Boards Manager.
3. Select the board configuration matching your actual ESP32-C3 board.
4. Install the appropriate libraries using Library Manager or their official installation instructions.

| Library | Required when |
| --- | --- |
| `Adafruit AHTX0` | Basic sensor operation |
| `Adafruit BMP280 Library` | Basic sensor operation |
| `Ethernet` | W5500/W5200 transport selected |
| `ArduinoMDNS` | Ethernet build (included by this sketch) |
| `U8g2` | Either OLED enabled |
| `SensirionI2cScd4x` | SCD40 enabled |

Standard facilities including `Wire`, `SPI`, `WiFi`, `WebServer`, `ESPmDNS`, `ArduinoOTA`, and `Update` are supplied by the ESP32 Arduino environment. Install any additional library dependencies when prompted.

## Getting Started

1. Open `DEN_HOME_ENV_NODE_v2_0_7.ino` in Arduino IDE.
2. Edit the **Node-Specific Configuration** section near the top of the sketch.
3. Set **exactly one** of `NETWORK_WIFI`, `NETWORK_W5500`, and `NETWORK_W5200` to `1`.
4. Give each device a suitable `NODE_NAME`.
5. For Wi-Fi, set `WIFI_SSID` and `WIFI_PASS`.
6. When using a static IP, configure `local_IP`, `gateway`, `subnet`, and DNS to match your network.
7. If Web OTA is enabled, replace `WEB_OTA_USERNAME` and `OTA_PASSWORD` with your own credentials.
8. Enable the sensors/displays you intend to use, upload the sketch, and verify operation.

```cpp
// Enable exactly one network mode.
#define NETWORK_WIFI   0
#define NETWORK_W5500  1
#define NETWORK_W5200  0

#define NODE_NAME "ETH-TEST01"
#define USE_STATIC_IP 1  // 1 = static IP, 0 = DHCP

#define HAS_ONBOARD_OLED  0
#define HAS_EXTERNAL_OLED 0
#define HAS_SCD40        0
#define ENABLE_WEB_OTA   1
```

**Important default IP:** The uploaded sketch sets `local_IP` to `192.168.1.254`, which is outside the project's intended Ethernet node range (`.231–.249`). Change it to a unique, valid address for your own network before deploying with static IP.

## Web Interface and Monitoring API

Once the node is connected, the following endpoints are available on port 80:

| Endpoint | Description |
| --- | --- |
| `http://<node-ip>/` | Dashboard with sensor readings, network status, and pressure graph |
| `http://<node-ip>/status` | JSON status API with readings, sensor presence, link status, uptime, etc. |
| `http://<node-ip>/metrics` | Prometheus exposition format |
| `http://<node-ip>/update` | Browser firmware update when enabled |

Trailing-slash variants such as `/status/` and `/metrics/` are supported.

Selected Prometheus metric names:

- `den_env_temperature_celsius`
- `den_env_humidity_percent`
- `den_env_pressure_hpa`
- `den_env_co2_ppm` (if SCD40 is enabled)
- `den_wifi_rssi_dbm` (Wi-Fi build only)
- `den_network_link_up`
- `den_network_info`
- `den_uptime_seconds`
- `den_free_heap_bytes`
- `den_sensor_present`
- `den_pressure_history_points`

Configure Prometheus to scrape `http://<node-ip>/metrics`. Grafana can then visualize these values using Prometheus as a data source.

## Firmware Updates (OTA)

- **ArduinoOTA:** available on Wi-Fi builds.
- **Web OTA:** available on both Wi-Fi and Ethernet when `ENABLE_WEB_OTA` is `1`.
- Export the **compiled application `.bin`** from Arduino IDE. Do not upload a bootloader, partition, or merged image.
- For Ethernet, open `/update` in the browser; the page uploads the binary to `/update-bin` automatically.
- Keep power and network connectivity stable throughout the update.

**Security:** Web OTA uses HTTP Basic authentication over unencrypted HTTP; the firmware does not implement HTTPS. Do not expose the HTTP service directly to the public Internet. Use a trusted LAN or properly secured VPN, and configure a unique, strong password. An empty OTA password disables authentication. Never commit real Wi-Fi or OTA credentials to Git, including its history.

## Known Limitations / Experimental Features

- **Ethernet mDNS:** Experimental `ArduinoMDNS` implementation; `<node-name>.local` resolution or HTTP service advertisement may fail in some environments. Use the node's IP address as a fallback.
- **W5200:** Selectable, but the source identifies hardware support as untested.
- **SCD40:** Optional. Some OLED/SCD40 combinations have shown responsiveness issues during development; the cause is under investigation. Keep `HAS_SCD40` set to `0` when prioritizing stability.
- **Pressure history:** Stored in RAM; cleared on reboot.
- **Custom PCB:** The prototype board has been ordered but has not yet been assembled or validated. Its I²C wiring uses GPIO20/21, unlike GPIO8/9 in the published firmware.

## Development History

1. **Atmospheric pressure monitoring with Arduino:** Started observing pressure changes to keep personal records alongside migraine experiences.
2. **Migration to ESP32:** Expanded the project toward connected observation nodes deployed in multiple locations.
3. **Wi-Fi and Ethernet support:** Developed common firmware with selectable transports to suit different installation environments.
4. **Custom PCB design:** Designed and ordered a prototype carrier board for ESP32-C3 and W5500 modules. **Hardware validation is still pending.**

Development notes are also planned for the DEN-HOME LAB blog.

## Changelog

- **v2.0 beta7 (documented here):** Fixed initial Wi-Fi service startup for HTTP, mDNS, and ArduinoOTA after a successful connection during setup.
- **v2.0 beta6:** Added experimental Ethernet mDNS support.
- **v2.0 beta5:** Added support for trailing slashes on HTTP endpoint URLs.
- **v2.0 beta4:** Addressed truncated large HTTP responses on W5500.

## License

**No license has been selected yet.** Making source code publicly viewable does not automatically grant permission to modify or redistribute it. A `LICENSE` file may be added after the project's sharing terms are decided.

---

**DEN-HOME LAB** — Home-built environmental monitoring hardware and firmware / 自作環境監視ハードウェア・ファームウェア

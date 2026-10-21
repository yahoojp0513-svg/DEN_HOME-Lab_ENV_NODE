# DEN-HOME ENV NODE

ESP32 Environmental Monitoring System

日本語 | English

## Overview / 概要

DEN-HOME ENV NODE is an ESP32-based environmental
monitoring system developed as part of the DEN-HOME LAB project.

It collects environmental data such as temperature,
humidity, and atmospheric pressure, and provides the
data over HTTP for monitoring and visualization.

Initially, I designed a pressure observation node using Arduino
because I suffer from migraines, but to expand the scale,
I used the ESP32, allowing me to set up observation points
in various locations.

DEN-HOME ENV NODEは，DEN-HOME LABの一環として
開発しているESP32ベースの環境監視システムです．

温度・湿度・気圧などの環境データを取得し，
HTTP経由で監視システムへ提供します．

最初は偏頭痛持ちのために，気圧を観測するノードを
Arduinoを用いて設計しておりましたが，
それを規模拡大させるためにESP32を用いて
様々なところに観測点を設けることができるようになった形です．

## Known Issues / 既知の問題

### Ethernet mDNS
Ethernet mDNS support is experimental.
Hostname resolution may not work correctly
in some network environments.

Ethernet mDNSは実験機能です．
ネットワーク環境によっては名前解決が
正常に動作しない場合があります．

### SCD40
SCD40 support is currently under investigation.
Some configurations may experience stability issues.

SCD40の対応状況は現在調査中です．
一部構成では動作が不安定になる可能性があります．

## Features / 主な機能
Wi-Fi / Ethernet (W5500 / W5200)
AHT20 / BMP280
OLED
Prometheus
OTA Updates

## Hardware / 使用部品

## Wiring / 配線

## Installation / 導入方法

## Configuration / 設定方法

## License / ライセンス

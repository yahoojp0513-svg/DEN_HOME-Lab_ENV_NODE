/*
 * ============================================================
 * DEN-HOME LAB Environment Sensor Node
 * ESP32-C3 Complete Common Firmware v2.0 beta7
 *
 * Network:
 *   - Wi-Fi                         [selectable]
 *   - W5500 Ethernet                [selectable]
 *   - W5200 Ethernet                [selectable / untested hardware]
 *
 * Sensors:
 *   - AHT20  : Temperature / Humidity
 *   - BMP280 : Temperature / Pressure
 *   - SCD40  : CO2 / Temperature / Humidity      [optional]
 *
 * Displays:
 *   - Onboard 0.42" SSD1306 72x40 OLED          [optional]
 *   - External 1.3" SH1106 128x64 OLED          [optional]
 *
 * Features:
 *   - Wi-Fi / WIZnet Ethernet compile-time selection
 *   - Static IP / DHCP selectable
 *   - Wi-Fi automatic reconnect + service recovery
 *   - Ethernet link / DHCP maintenance
 *   - ArduinoOTA                         [Wi-Fi only]
 *   - Web OTA (/update)                 [Wi-Fi + Ethernet]
 *   - mDNS                              [Wi-Fi + Ethernet experimental]
 *   - Prometheus /metrics
 *   - Monitor-oriented /status JSON API
 *   - Responsive Web UI
 *   - 3-hour pressure history (5 min x 36 points)
 *   - OLED page switching / dashboard
 *   - Selectable serial monitor output
 *
 * W5500 Ethernet ENV standard pins:
 *   MISO = GPIO20
 *   CS   = GPIO1
 *   SCLK = GPIO3
 *   MOSI = GPIO4
 *   RST  = GPIO0
 *
 * External I2C:
 *   SDA = GPIO8
 *   SCL = GPIO9
 *
 * IMPORTANT:
 *   Exactly ONE of NETWORK_WIFI / NETWORK_W5500 / NETWORK_W5200
 *   must be set to 1.
 * ============================================================
 */

#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <math.h>
#include <Update.h>

// ネットワーク方式ごとのライブラリは、選択された方式だけ読み込みます。
// NETWORK_* の設定はこの少し下にあります。
//
// Only the libraries required for the selected network mode are included.
// The NETWORK_* settings are defined below.

// ============================================================
// ★★★ 各ノードで変更する場所 ★★★
// ★★★ Node-Specific Configuration ★★★
// ============================================================

// ------------------------------------------------------------
// 1. ネットワーク方式
// 必ずどれか1つだけ 1 にする
//
// 1. Network Mode
// Set exactly one network mode to 1.
// ------------------------------------------------------------
#define NETWORK_WIFI   0
#define NETWORK_W5500  1
#define NETWORK_W5200  0

#if (NETWORK_WIFI + NETWORK_W5500 + NETWORK_W5200) != 1
  #error "NETWORK_WIFI / NETWORK_W5500 / NETWORK_W5200 のどれか1つだけを1にしてください"
#endif

#if NETWORK_WIFI
  #include <WiFi.h>
  #include <WebServer.h>
  #include <ESPmDNS.h>
  #include <ArduinoOTA.h>
#else
  #include <SPI.h>
  #include <Ethernet.h>
  #include <EthernetUdp.h>
  #include <ArduinoMDNS.h>
#endif

// ------------------------------------------------------------
// 2. 本体内蔵 0.42" OLED
// 1 = 使用する / 0 = 使用しない
//
// 2. Onboard 0.42" OLED
// 1 = Enable / 0 = Disable
// ------------------------------------------------------------
#define HAS_ONBOARD_OLED 0

// ------------------------------------------------------------
// 3. 外付け 1.3" SH1106 OLED
// 1 = 使用する / 0 = 使用しない
//
// 3. External 1.3" SH1106 OLED
// 1 = Enable / 0 = Disable
// ------------------------------------------------------------
#define HAS_EXTERNAL_OLED 0

// ------------------------------------------------------------
// 4. SCD40
// 1 = 使用する / 0 = 使用しない
//
// 4. SCD40 Sensor
// 1 = Enable / 0 = Disable
// ------------------------------------------------------------
#define HAS_SCD40 0

// ------------------------------------------------------------
// 5. ノード名
// Web / OTA / Prometheus label / OLED に使用
// Wi-Fi / Ethernet 共通で mDNSホスト名にも使用
//
// 5. Node Name
// Used for the Web UI, OTA, Prometheus labels, and OLED display.
// Also used as the mDNS hostname for both Wi-Fi and Ethernet.
// ------------------------------------------------------------
#define NODE_NAME "ETH-TEST01"

// ------------------------------------------------------------
// 6. Wi-Fi設定
// NETWORK_WIFI = 1 のときだけ使用
//
// 6. Wi-Fi Settings
// Used only when NETWORK_WIFI = 1.
// ------------------------------------------------------------
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// ------------------------------------------------------------
// 7. IPアドレス方式
// 1 = 固定IP / 0 = DHCP
// Wi-Fi / Ethernet 共通
//
// 7. IP Address Configuration
// 1 = Static IP / 0 = DHCP
// Applies to both Wi-Fi and Ethernet.
// ------------------------------------------------------------
#define USE_STATIC_IP 1

// ------------------------------------------------------------
// 8. IPアドレス
// 管理用 ESP32     : 192.168.1.201 ～ 192.168.1.205
// Wi-Fi ENV        : 192.168.1.211 ～ 192.168.1.229
// Ethernet ENV     : 192.168.1.231 ～ 192.168.1.249
// ★ Ethernetノードでは .231～.249 を使用すること
//
// 8. IP Address
// Management ESP32 : 192.168.1.201 - 192.168.1.205
// Wi-Fi ENV        : 192.168.1.211 - 192.168.1.229
// Ethernet ENV     : 192.168.1.231 - 192.168.1.249
// ★ Ethernet nodes must use addresses in the .231 - .249 range.
// ------------------------------------------------------------
IPAddress local_IP(192, 168, 1, 254);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 1, 1);
IPAddress secondaryDNS(8, 8, 8, 8);  // Ethernetライブラリでは未使用

// ------------------------------------------------------------
// 9. W5500 / W5200 Ethernet SPIピン
// DEN-HOME LAB Ethernet ENV 標準
//
// 9. W5500 / W5200 Ethernet SPI Pins
// DEN-HOME LAB Ethernet ENV standard pin configuration.
// ------------------------------------------------------------
#define WIZNET_MISO   7
#define WIZNET_CS     1
#define WIZNET_SCLK   3
#define WIZNET_MOSI   4
#define WIZNET_RST    0

// ------------------------------------------------------------
// 10. Ethernet MACアドレス
// 1 = ESP32固有IDから自動生成 / 0 = 下の手動MACを使用
// 通常は 1 のままでOK
//
// 10. Ethernet MAC Address
// 1 = Automatically generate from the ESP32 unique ID.
// 0 = Use the manually configured MAC address below.
// Normally, leave this setting at 1.
// ------------------------------------------------------------
#define USE_AUTO_ETHERNET_MAC 1
byte MANUAL_ETHERNET_MAC[6] = {0x02, 0xDE, 0xAD, 0xBE, 0xEF, 0x01};

// ------------------------------------------------------------
// 11. OTA パスワード
// 空文字 "" にするとパスワードなし
// ArduinoOTA : Wi-Fiのみ
// Web OTA     : Wi-Fi / Ethernet 両対応
//
// 11. OTA Password
// Set to an empty string "" to disable password authentication.
// ArduinoOTA : Wi-Fi only
// Web OTA     : Supports both Wi-Fi and Ethernet.
// ------------------------------------------------------------
const char* OTA_PASSWORD = "YOUR_OTA_PASSWORD";

// ------------------------------------------------------------
// 12. Web OTA（ブラウザからファームウェア更新）
// 1 = 使用する / 0 = 使用しない
//
// http://ノードのIP/update
// Ethernetでも使用可能。
//
// 12. Web OTA (Browser-Based Firmware Update)
// 1 = Enable / 0 = Disable
//
// http://<node-ip>/update
// Also available over Ethernet.
// ------------------------------------------------------------
#define ENABLE_WEB_OTA 1
const char* WEB_OTA_USERNAME = "User_name";
const char* WEB_OTA_PASSWORD = OTA_PASSWORD;

// ------------------------------------------------------------
// 13. シリアルモニタ全体
// 1 = 使用する / 0 = すべてのシリアル出力を停止
//
// 13. Global Serial Output
// 1 = Enable / 0 = Disable all serial output.
// ------------------------------------------------------------
#define ENABLE_SERIAL_MONITOR 1

// ------------------------------------------------------------
// 14. 定期観測値の表示項目
// 1 = 表示 / 0 = 非表示
//
// 14. Periodic Sensor Reading Display Options
// 1 = Show / 0 = Hide
// ------------------------------------------------------------
#define SERIAL_OBS_AHT_TEMP       0
#define SERIAL_OBS_AHT_HUMIDITY   0
#define SERIAL_OBS_BMP_TEMP       0
#define SERIAL_OBS_PRESSURE       0
#define SERIAL_OBS_CO2            0
#define SERIAL_OBS_SCD_TEMP       0
#define SERIAL_OBS_SCD_HUMIDITY   0
#define SERIAL_OBS_RSSI           1
#define SERIAL_OBS_IP             1
#define SERIAL_OBS_MDNS_URL       1
#define SERIAL_OBS_NETWORK_TYPE   1
#define SERIAL_OBS_UPTIME         1

// ------------------------------------------------------------
// 15. イベント / 診断ログ
//
// 15. Event / Diagnostic Logs
// ------------------------------------------------------------
#define SERIAL_EVENT_BOOT               1
#define SERIAL_EVENT_SENSOR_STATUS      1
#define SERIAL_EVENT_WIFI               1
#define SERIAL_EVENT_ETHERNET           1
#define SERIAL_EVENT_NETWORK_SERVICES   1
#define SERIAL_EVENT_OTA                1
#define SERIAL_EVENT_OTA_PROGRESS       1
#define SERIAL_EVENT_SENSOR_ERRORS      1
#define SERIAL_EVENT_PRESSURE_HISTORY   0
#define SERIAL_DIAG_FREE_HEAP           1

// ------------------------------------------------------------
// 16. Wi-Fi 自動再接続
// NETWORK_WIFI = 1 のときだけ使用
//
// 16. Wi-Fi Automatic Reconnection
// Used only when NETWORK_WIFI = 1.
// ------------------------------------------------------------
#define WIFI_AUTO_RETRY 1
const unsigned long WIFI_RETRY_INTERVAL_MS = 10000UL;

// ------------------------------------------------------------
// 17. Ethernet再試行
// DHCP取得失敗時などに再試行する間隔
//
// 17. Ethernet Retry Settings
// Retry interval for DHCP failures and other connection issues.
// ------------------------------------------------------------
#define ETHERNET_AUTO_RETRY 1
const unsigned long ETHERNET_RETRY_INTERVAL_MS = 10000UL;
const unsigned long ETHERNET_DHCP_TIMEOUT_MS = 10000UL;
const unsigned long ETHERNET_DHCP_RESPONSE_TIMEOUT_MS = 2000UL;

// ------------------------------------------------------------
// 18. Ethernet mDNS（実験機能）
// 1 = 使用する / 0 = 使用しない
//
// ArduinoMDNS ライブラリを使用して、W5500/W5200でも
// http://NODE_NAME.local/ でアクセスできるか試します。
//
// 18. Ethernet mDNS (Experimental Feature)
// 1 = Enable / 0 = Disable
//
// Uses the ArduinoMDNS library to test whether W5500/W5200
// nodes can be accessed via http://NODE_NAME.local/.
// ------------------------------------------------------------
#define ENABLE_ETHERNET_MDNS 1

// ========================================================================================================================
// ここから下は通常は変更しなくていい設定
//
// The settings below normally do not need to be modified.
// ========================================================================================================================

// Onboard OLED pins
#define ONBOARD_OLED_SDA 5
#define ONBOARD_OLED_SCL 6

// External I2C pins
#define SENSOR_SDA 8
#define SENSOR_SCL 9

// External OLED I2C address
#define EXTERNAL_OLED_ADDRESS 0x3C

// SCD40 I2C address
#define SCD40_ADDRESS 0x62

// Sensor read interval
const unsigned long SENSOR_INTERVAL_MS = 5000;

// Onboard OLED page interval
const unsigned long PAGE_INTERVAL_MS = 3000;

// Web page auto refresh
const int WEB_REFRESH_SECONDS = 10;

// Pressure history: 5 min x 36 = 3 hours
const unsigned long PRESSURE_HISTORY_INTERVAL_MS = 5UL * 60UL * 1000UL;
const int PRESSURE_HISTORY_SIZE = 36;

// Minimum vertical pressure graph range
const float PRESSURE_GRAPH_MIN_RANGE_HPA = 2.0F;

// ============================================================
// Optional libraries
// ============================================================

#if HAS_ONBOARD_OLED || HAS_EXTERNAL_OLED
#include <U8g2lib.h>
#endif

#if HAS_SCD40
#include <SensirionI2cScd4x.h>
#endif

// ============================================================
// Display objects
// ============================================================

#if HAS_ONBOARD_OLED
U8G2_SSD1306_72X40_ER_F_SW_I2C onboardOled(
  U8G2_R0,
  ONBOARD_OLED_SCL,
  ONBOARD_OLED_SDA,
  U8X8_PIN_NONE
);
#endif

#if HAS_EXTERNAL_OLED
// 1.3" / 128x64 / SH1106 / I2C
// External I2C uses the global Wire bus on GPIO8/9.
U8G2_SH1106_128X64_NONAME_F_HW_I2C externalOled(
  U8G2_R0,
  U8X8_PIN_NONE
);
#endif

// ============================================================
// Sensor / network server objects
// ============================================================

Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp(&Wire);

#if HAS_SCD40
SensirionI2cScd4x scd40;
#endif

bool mdnsReady = false;
bool networkServicesStarted = false;

#if NETWORK_WIFI
WebServer server(80);
bool webServerConfigured = false;
bool otaConfigured = false;
bool wifiWasConnected = false;
unsigned long lastWiFiRetry = 0;
#else
EthernetServer ethernetServer(80);
bool ethernetInitialized = false;
bool ethernetServerStarted = false;
bool ethernetWasLinked = false;
unsigned long lastEthernetRetry = 0;
byte ethernetMac[6] = {0};

#if ENABLE_ETHERNET_MDNS
EthernetUDP ethernetMdnsUdp;
MDNS ethernetMdns(ethernetMdnsUdp);
bool ethernetMdnsStarted = false;
#endif
#endif

#if ENABLE_WEB_OTA && NETWORK_WIFI
bool webOtaUploadAuthorized = false;
bool webOtaUpdateInProgress = false;
#endif

// ============================================================
// Sensor state
// ============================================================

bool hasAHT = false;
bool hasBMP = false;

#if HAS_SCD40
bool hasSCD40 = false;
#endif

float ahtTemp = NAN;
float humidity = NAN;
float bmpTemp = NAN;
float pressure = NAN;

#if HAS_SCD40
uint16_t co2ppm = 0;
float scdTemp = NAN;
float scdHumidity = NAN;
#endif

// ============================================================
// Pressure history
// ============================================================

float pressureHistory[PRESSURE_HISTORY_SIZE];
int pressureHistoryCount = 0;
int pressureHistoryIndex = 0;
unsigned long lastPressureHistorySave = 0;

// ============================================================
// Timers
// ============================================================

unsigned long lastSensorRead = 0;

#if HAS_ONBOARD_OLED
unsigned long lastPageChange = 0;
int onboardPage = 0;
#endif

// ============================================================
// I2C helper
// ============================================================

// ------------------------------------------------------------
// Ethernet HTTPリクエスト解析用
//
// Arduino IDEは .ino 内の関数プロトタイプを自動生成するため、
// この型は最初の関数定義より前に置いておく必要があります。
// ------------------------------------------------------------
#if !NETWORK_WIFI
struct EthernetHttpRequest {
  String method;
  String path;
  String authorization;
  size_t contentLength = 0;
};
#endif

bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

// ============================================================
// Pressure history helper
// ============================================================

float getPressureHistoryValue(int position) {
  if (position < 0 || position >= pressureHistoryCount) {
    return NAN;
  }

  int index;

  if (pressureHistoryCount < PRESSURE_HISTORY_SIZE) {
    index = position;
  } else {
    index = (pressureHistoryIndex + position) % PRESSURE_HISTORY_SIZE;
  }

  return pressureHistory[index];
}

bool getPressureRange(float& minP, float& maxP) {
  if (pressureHistoryCount < 1) {
    return false;
  }

  minP = 9999.0F;
  maxP = -9999.0F;

  for (int i = 0; i < pressureHistoryCount; i++) {
    float p = getPressureHistoryValue(i);
    if (isnan(p)) continue;
    if (p < minP) minP = p;
    if (p > maxP) maxP = p;
  }

  return true;
}

void normalizePressureRange(float& minP, float& maxP) {
  float range = maxP - minP;

  if (range < PRESSURE_GRAPH_MIN_RANGE_HPA) {
    float center = (maxP + minP) / 2.0F;
    minP = center - (PRESSURE_GRAPH_MIN_RANGE_HPA / 2.0F);
    maxP = center + (PRESSURE_GRAPH_MIN_RANGE_HPA / 2.0F);
  }
}

void savePressureHistory() {
  if (!hasBMP || isnan(pressure)) return;

  pressureHistory[pressureHistoryIndex] = pressure;
  pressureHistoryIndex++;

  if (pressureHistoryIndex >= PRESSURE_HISTORY_SIZE) {
    pressureHistoryIndex = 0;
  }

  if (pressureHistoryCount < PRESSURE_HISTORY_SIZE) {
    pressureHistoryCount++;
  }

#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_PRESSURE_HISTORY
  Serial.print("[HISTORY] Pressure=");
  Serial.print(pressure, 2);
  Serial.print("hPa Points=");
  Serial.print(pressureHistoryCount);
  Serial.print("/");
  Serial.println(PRESSURE_HISTORY_SIZE);
#endif
}

// ============================================================
// SCD40
// ============================================================

#if HAS_SCD40

void initSCD40() {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_SENSOR_STATUS
  Serial.println("[SENSOR] SCD40 probing address=0x62");
#endif

  if (!i2cDevicePresent(SCD40_ADDRESS)) {
    hasSCD40 = false;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_SENSOR_STATUS
    Serial.println("[SENSOR] SCD40=NG reason=not_found");
#endif
    return;
  }

  scd40.begin(Wire, SCD40_ADDRESS);
  delay(30);

  // Put the sensor in a known state before starting periodic measurement.
  // stopPeriodicMeasurement() can be called even after a previous run.
  int16_t error = scd40.stopPeriodicMeasurement();

  if (error != 0) {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_SENSOR_ERRORS
    Serial.print("[ERROR][SCD40] stopPeriodicMeasurement code=");
    Serial.println(error);
#endif
  }

  delay(500);

  error = scd40.startPeriodicMeasurement();

  if (error == 0) {
    hasSCD40 = true;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_SENSOR_STATUS
    Serial.println("[SENSOR] SCD40=OK periodic_measurement=started");
#endif
  } else {
    hasSCD40 = false;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_SENSOR_ERRORS
    Serial.print("[ERROR][SCD40] startPeriodicMeasurement code=");
    Serial.println(error);
#endif
  }
}

void readSCD40() {
  if (!hasSCD40) return;

  bool dataReady = false;
  int16_t error = scd40.getDataReadyStatus(dataReady);

  if (error != 0) {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_SENSOR_ERRORS
    Serial.print("[ERROR][SCD40] getDataReadyStatus code=");
    Serial.println(error);
#endif
    return;
  }

  if (!dataReady) return;

  uint16_t newCO2 = 0;
  float newTemp = NAN;
  float newHumidity = NAN;

  error = scd40.readMeasurement(newCO2, newTemp, newHumidity);

  if (error != 0) {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_SENSOR_ERRORS
    Serial.print("[ERROR][SCD40] readMeasurement code=");
    Serial.println(error);
#endif
    return;
  }

  // 0 ppm is not a valid normal measurement; keep the previous value.
  if (newCO2 != 0) {
    co2ppm = newCO2;
    scdTemp = newTemp;
    scdHumidity = newHumidity;
  }
}

#endif

// ============================================================
// Network common helpers
// ============================================================

bool ipIsZero(const IPAddress& ip) {
  return ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0;
}

const char* networkTypeName() {
#if NETWORK_WIFI
  return "wifi";
#elif NETWORK_W5500
  return "w5500";
#else
  return "w5200";
#endif
}

IPAddress networkLocalIP() {
#if NETWORK_WIFI
  return WiFi.localIP();
#else
  return Ethernet.localIP();
#endif
}

bool networkConnected() {
#if NETWORK_WIFI
  return WiFi.status() == WL_CONNECTED;
#else
  if (!ethernetInitialized) return false;
  if (Ethernet.hardwareStatus() == EthernetNoHardware) return false;
  if (Ethernet.linkStatus() == LinkOFF) return false;
  return !ipIsZero(Ethernet.localIP());
#endif
}

int networkRSSI() {
#if NETWORK_WIFI
  return (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : 0;
#else
  return 0;
#endif
}

const char* wiznetHardwareName() {
#if NETWORK_WIFI
  return "N/A";
#else
  switch (Ethernet.hardwareStatus()) {
    case EthernetW5100: return "W5100";
    case EthernetW5200: return "W5200";
    case EthernetW5500: return "W5500";
    case EthernetNoHardware: return "NO_HW";
    default: return "UNKNOWN";
  }
#endif
}

String networkDisplayText() {
#if NETWORK_WIFI
  if (!networkConnected()) return "WiFi NG";
  return String(networkRSSI()) + "dBm";
#else
  if (!networkConnected()) return "ETH NG";
  return String("ETH ") + wiznetHardwareName();
#endif
}

// ============================================================
// Serial monitor helpers
// ============================================================

#if ENABLE_SERIAL_MONITOR

void printSerialObservation() {
  Serial.print("[OBS] Node=");
  Serial.print(NODE_NAME);

#if SERIAL_OBS_AHT_TEMP
  Serial.print(" AHT_T=");
  if (!isnan(ahtTemp)) { Serial.print(ahtTemp, 1); Serial.print("C"); }
  else { Serial.print("N/A"); }
#endif

#if SERIAL_OBS_AHT_HUMIDITY
  Serial.print(" AHT_RH=");
  if (!isnan(humidity)) { Serial.print(humidity, 1); Serial.print("%"); }
  else { Serial.print("N/A"); }
#endif

#if SERIAL_OBS_BMP_TEMP
  Serial.print(" BMP_T=");
  if (!isnan(bmpTemp)) { Serial.print(bmpTemp, 1); Serial.print("C"); }
  else { Serial.print("N/A"); }
#endif

#if SERIAL_OBS_PRESSURE
  Serial.print(" P=");
  if (!isnan(pressure)) { Serial.print(pressure, 1); Serial.print("hPa"); }
  else { Serial.print("N/A"); }
#endif

#if HAS_SCD40 && SERIAL_OBS_CO2
  Serial.print(" CO2=");
  if (hasSCD40 && co2ppm > 0) { Serial.print(co2ppm); Serial.print("ppm"); }
  else { Serial.print("N/A"); }
#endif

#if HAS_SCD40 && SERIAL_OBS_SCD_TEMP
  Serial.print(" SCD_T=");
  if (!isnan(scdTemp)) { Serial.print(scdTemp, 1); Serial.print("C"); }
  else { Serial.print("N/A"); }
#endif

#if HAS_SCD40 && SERIAL_OBS_SCD_HUMIDITY
  Serial.print(" SCD_RH=");
  if (!isnan(scdHumidity)) { Serial.print(scdHumidity, 1); Serial.print("%"); }
  else { Serial.print("N/A"); }
#endif

#if SERIAL_OBS_RSSI
  Serial.print(" RSSI=");
#if NETWORK_WIFI
  if (networkConnected()) { Serial.print(networkRSSI()); Serial.print("dBm"); }
  else { Serial.print("N/A"); }
#else
  Serial.print("N/A");
#endif
#endif

#if SERIAL_OBS_IP
  Serial.print(" IP=");
  if (networkConnected()) { Serial.print(networkLocalIP()); }
  else { Serial.print("N/A"); }
#endif

#if SERIAL_OBS_MDNS_URL
  Serial.print(" mDNS=");
  if (networkConnected() && mdnsReady) {
    Serial.print("http://");
    Serial.print(NODE_NAME);
    Serial.print(".local/");
  } else {
    Serial.print("N/A");
  }
#endif

#if SERIAL_OBS_NETWORK_TYPE
  Serial.print(" NET=");
  Serial.print(networkTypeName());
#endif

#if SERIAL_OBS_UPTIME
  Serial.print(" Uptime=");
  Serial.print(millis() / 1000UL);
  Serial.print("s");
#endif

  Serial.println();
}

void printSerialDiagnostics() {
#if SERIAL_DIAG_FREE_HEAP
  Serial.print("[DIAG] FreeHeap=");
  Serial.print(ESP.getFreeHeap());
  Serial.println("bytes");
#endif
}

#endif

// ============================================================
// Sensor read
// ============================================================

void readSensors() {
  if (hasAHT) {
    sensors_event_t humidityEvent;
    sensors_event_t temperatureEvent;

    aht.getEvent(&humidityEvent, &temperatureEvent);
    ahtTemp = temperatureEvent.temperature;
    humidity = humidityEvent.relative_humidity;
  }

  if (hasBMP) {
    bmpTemp = bmp.readTemperature();
    pressure = bmp.readPressure() / 100.0F;
  }

#if HAS_SCD40
  readSCD40();
#endif

#if ENABLE_SERIAL_MONITOR
  printSerialObservation();
  printSerialDiagnostics();
#endif
}

// ============================================================
// Onboard 0.42" OLED
// ============================================================

#if HAS_ONBOARD_OLED

void drawOnboardBoot(const char* line1, const char* line2 = "") {
  onboardOled.clearBuffer();
  onboardOled.setFont(u8g2_font_5x8_tf);
  onboardOled.drawStr(0, 8, NODE_NAME);
  onboardOled.drawStr(0, 22, line1);
  onboardOled.drawStr(0, 36, line2);
  onboardOled.sendBuffer();
}

void drawOnboardSummary() {
  char line[32];

  onboardOled.clearBuffer();
  onboardOled.setFont(u8g2_font_5x8_tf);
  onboardOled.drawStr(0, 8, NODE_NAME);

  if (!isnan(ahtTemp) && !isnan(humidity)) {
    snprintf(line, sizeof(line), "T %.1fC H %.0f%%", ahtTemp, humidity);
    onboardOled.drawStr(0, 22, line);
  } else {
    onboardOled.drawStr(0, 22, "AHT N/A");
  }

  if (!isnan(pressure)) {
    snprintf(line, sizeof(line), "P %.1fhPa", pressure);
    onboardOled.drawStr(0, 36, line);
  } else {
    onboardOled.drawStr(0, 36, "BMP N/A");
  }

  onboardOled.sendBuffer();
}

void drawOnboardAHT() {
  char line[32];

  onboardOled.clearBuffer();
  onboardOled.setFont(u8g2_font_5x8_tf);
  onboardOled.drawStr(0, 8, "AHT20");

  if (!isnan(ahtTemp)) {
    snprintf(line, sizeof(line), "Temp %.1f C", ahtTemp);
    onboardOled.drawStr(0, 22, line);
  } else {
    onboardOled.drawStr(0, 22, "Temp N/A");
  }

  if (!isnan(humidity)) {
    snprintf(line, sizeof(line), "Hum %.1f%%", humidity);
    onboardOled.drawStr(0, 36, line);
  } else {
    onboardOled.drawStr(0, 36, "Hum N/A");
  }

  onboardOled.sendBuffer();
}

void drawOnboardBMP() {
  char line[32];

  onboardOled.clearBuffer();
  onboardOled.setFont(u8g2_font_5x8_tf);
  onboardOled.drawStr(0, 8, "BMP280");

  if (!isnan(bmpTemp)) {
    snprintf(line, sizeof(line), "Temp %.1f C", bmpTemp);
    onboardOled.drawStr(0, 22, line);
  } else {
    onboardOled.drawStr(0, 22, "Temp N/A");
  }

  if (!isnan(pressure)) {
    snprintf(line, sizeof(line), "%.1f hPa", pressure);
    onboardOled.drawStr(0, 36, line);
  } else {
    onboardOled.drawStr(0, 36, "Pres N/A");
  }

  onboardOled.sendBuffer();
}

#if HAS_SCD40
void drawOnboardSCD40() {
  char line[32];

  onboardOled.clearBuffer();
  onboardOled.setFont(u8g2_font_5x8_tf);
  onboardOled.drawStr(0, 8, "SCD40 CO2");

  if (hasSCD40 && co2ppm > 0) {
    snprintf(line, sizeof(line), "%u ppm", co2ppm);
    onboardOled.drawStr(0, 24, line);

    if (!isnan(scdTemp)) {
      snprintf(line, sizeof(line), "T %.1f H %.0f", scdTemp, scdHumidity);
      onboardOled.drawStr(0, 38, line);
    }
  } else {
    onboardOled.drawStr(0, 26, "Collecting...");
  }

  onboardOled.sendBuffer();
}
#endif

void drawOnboardPressureGraph() {
  onboardOled.clearBuffer();

  if (pressureHistoryCount < 2) {
    onboardOled.setFont(u8g2_font_5x8_tf);
    onboardOled.drawStr(0, 10, "Pressure");
    onboardOled.drawStr(0, 26, "Collecting...");
    onboardOled.sendBuffer();
    return;
  }

  float minP, maxP;
  if (!getPressureRange(minP, maxP)) return;
  normalizePressureRange(minP, maxP);
  float range = maxP - minP;

  const int graphLeft = 1;
  const int graphRight = 70;
  const int graphTop = 1;
  const int graphBottom = 38;

  int previousX = -1;
  int previousY = -1;

  for (int i = 0; i < pressureHistoryCount; i++) {
    float value = getPressureHistoryValue(i);

    int x = graphLeft +
      (i * (graphRight - graphLeft)) /
      (pressureHistoryCount - 1);

    float normalized = (value - minP) / range;
    int y = graphBottom - (int)(normalized * (graphBottom - graphTop));

    if (previousX >= 0 && previousY >= 0) {
      onboardOled.drawLine(previousX, previousY, x, y);
    }

    previousX = x;
    previousY = y;
  }

  onboardOled.sendBuffer();
}

void drawOnboardNetwork() {
  char line[32];

  onboardOled.clearBuffer();
  onboardOled.setFont(u8g2_font_5x8_tf);
  onboardOled.drawStr(0, 8, NODE_NAME);

  if (networkConnected()) {
    String ip = networkLocalIP().toString();
    onboardOled.drawStr(0, 22, ip.c_str());
#if NETWORK_WIFI
    snprintf(line, sizeof(line), "%d dBm", networkRSSI());
#else
    snprintf(line, sizeof(line), "ETH %s", wiznetHardwareName());
#endif
    onboardOled.drawStr(0, 36, line);
  } else {
#if NETWORK_WIFI
    onboardOled.drawStr(0, 26, "WiFi LOST");
#else
    onboardOled.drawStr(0, 26, "ETH LOST");
#endif
  }

  onboardOled.sendBuffer();
}

int getOnboardPageCount() {
#if HAS_SCD40
  return 6; // Summary / AHT / BMP / SCD40 / Graph / Network
#else
  return 5; // Summary / AHT / BMP / Graph / Network
#endif
}

void drawCurrentOnboardPage() {
#if HAS_SCD40
  switch (onboardPage) {
    case 0: drawOnboardSummary(); break;
    case 1: drawOnboardAHT(); break;
    case 2: drawOnboardBMP(); break;
    case 3: drawOnboardSCD40(); break;
    case 4: drawOnboardPressureGraph(); break;
    case 5: drawOnboardNetwork(); break;
  }
#else
  switch (onboardPage) {
    case 0: drawOnboardSummary(); break;
    case 1: drawOnboardAHT(); break;
    case 2: drawOnboardBMP(); break;
    case 3: drawOnboardPressureGraph(); break;
    case 4: drawOnboardNetwork(); break;
  }
#endif
}

#endif

// ============================================================
// External 1.3" SH1106 128x64 OLED
//
// Bedside dashboard layout:
//   Row 1: AHT20 temp / humidity / Wi-Fi RSSI
//   Row 2: SCD40 CO2
//   Row 3: BMP280 pressure
//   Bottom: 3-hour pressure trend
// ============================================================

#if HAS_EXTERNAL_OLED

// Right-aligned text helper.
void drawExternalRightAligned(const char* text, int y) {
  int width = externalOled.getStrWidth(text);
  int x = 127 - width;
  if (x < 0) x = 0;
  externalOled.drawStr(x, y, text);
}

void drawExternalPressureMiniGraph(int left, int top, int right, int bottom) {
  // Frame the graph area so the active 128x64 boundary is obvious.
  externalOled.drawFrame(left, top, right - left + 1, bottom - top + 1);

  if (pressureHistoryCount < 2) {
    externalOled.setFont(u8g2_font_4x6_tf);

    char collecting[24];
    snprintf(
      collecting,
      sizeof(collecting),
      "collecting %d/%d",
      pressureHistoryCount,
      PRESSURE_HISTORY_SIZE
    );

    int w = externalOled.getStrWidth(collecting);
    int x = left + ((right - left + 1 - w) / 2);
    if (x < left + 1) x = left + 1;

    externalOled.drawStr(x, top + 12, collecting);
    return;
  }

  float minP, maxP;
  if (!getPressureRange(minP, maxP)) return;

  normalizePressureRange(minP, maxP);
  float range = maxP - minP;

  // Keep a one-pixel margin inside the frame.
  const int graphLeft = left + 2;
  const int graphRight = right - 2;
  const int graphTop = top + 2;
  const int graphBottom = bottom - 2;

  int previousX = -1;
  int previousY = -1;

  for (int i = 0; i < pressureHistoryCount; i++) {
    float value = getPressureHistoryValue(i);
    if (isnan(value)) continue;

    int x = graphLeft +
      (i * (graphRight - graphLeft)) /
      (pressureHistoryCount - 1);

    float normalized = (value - minP) / range;
    int y = graphBottom -
      (int)(normalized * (graphBottom - graphTop));

    if (previousX >= 0 && previousY >= 0) {
      externalOled.drawLine(previousX, previousY, x, y);
    }

    previousX = x;
    previousY = y;
  }
}

void drawExternalDashboard() {
  char leftText[28];
  char rightText[28];

  externalOled.clearBuffer();
  externalOled.setFont(u8g2_font_5x8_tf);

  // ----------------------------------------------------------
  // Row 1, y=8
  // Temperature + humidity on the left, RSSI on the right.
  // ----------------------------------------------------------
  if (!isnan(ahtTemp) && !isnan(humidity)) {
    snprintf(
      leftText,
      sizeof(leftText),
      "%.1fC %.0f%%",
      ahtTemp,
      humidity
    );
  } else {
    snprintf(leftText, sizeof(leftText), "AHT N/A");
  }

  externalOled.drawStr(0, 8, leftText);

#if NETWORK_WIFI
  if (networkConnected()) {
    snprintf(rightText, sizeof(rightText), "%ddBm", networkRSSI());
  } else {
    snprintf(rightText, sizeof(rightText), "WiFi NG");
  }
#else
  snprintf(rightText, sizeof(rightText), networkConnected() ? "ETH" : "ETH NG");
#endif

  drawExternalRightAligned(rightText, 8);

  // ----------------------------------------------------------
  // Row 2, y=18
  // CO2 label on left, value right aligned.
  // ----------------------------------------------------------
  externalOled.drawStr(0, 18, "CO2");

#if HAS_SCD40
  if (hasSCD40 && co2ppm > 0) {
    snprintf(rightText, sizeof(rightText), "%u ppm", co2ppm);
  } else if (hasSCD40) {
    snprintf(rightText, sizeof(rightText), "warming...");
  } else {
    snprintf(rightText, sizeof(rightText), "N/A");
  }
#else
  snprintf(rightText, sizeof(rightText), "disabled");
#endif

  drawExternalRightAligned(rightText, 18);

  // ----------------------------------------------------------
  // Row 3, y=28
  // Pressure label on left, value right aligned.
  // ----------------------------------------------------------
  externalOled.drawStr(0, 28, "P");

  if (!isnan(pressure)) {
    snprintf(rightText, sizeof(rightText), "%.1f hPa", pressure);
  } else {
    snprintf(rightText, sizeof(rightText), "N/A");
  }

  drawExternalRightAligned(rightText, 28);

  // Separator.
  externalOled.drawHLine(0, 32, 128);

  // Small graph title, intentionally short so it never clips.
  externalOled.setFont(u8g2_font_4x6_tf);
  externalOled.drawStr(1, 39, "PRESSURE TREND 3H");

  // Graph uses the rest of the screen.
  drawExternalPressureMiniGraph(0, 41, 127, 63);

  externalOled.sendBuffer();
}

void drawExternalBoot(const char* line1, const char* line2 = "") {
  externalOled.clearBuffer();
  externalOled.setFont(u8g2_font_6x10_tf);

  externalOled.drawStr(0, 12, NODE_NAME);
  externalOled.drawHLine(0, 16, 128);
  externalOled.drawStr(0, 34, line1);
  externalOled.drawStr(0, 52, line2);

  externalOled.sendBuffer();
}

#endif

// ============================================================
// OLED common helpers used by Wi-Fi / OTA
// ============================================================

void showStatusOnDisplays(const char* line1, const char* line2 = "") {
#if HAS_ONBOARD_OLED
  drawOnboardBoot(line1, line2);
#endif
#if HAS_EXTERNAL_OLED
  drawExternalBoot(line1, line2);
#endif
}

// ============================================================
// HTML pressure SVG
// ============================================================

String buildPressureSVG() {
  String svg;
  svg.reserve(2200);

  svg +=
    "<svg viewBox='0 0 600 180' preserveAspectRatio='none' class='pressuregraph'>";

  svg += "<line x1='0' y1='45' x2='600' y2='45' class='grid'/>";
  svg += "<line x1='0' y1='90' x2='600' y2='90' class='grid'/>";
  svg += "<line x1='0' y1='135' x2='600' y2='135' class='grid'/>";

  if (pressureHistoryCount < 2) {
    svg +=
      "<text x='300' y='95' text-anchor='middle' class='svgtext'>"
      "Collecting pressure history..."
      "</text></svg>";
    return svg;
  }

  float minP, maxP;
  if (!getPressureRange(minP, maxP)) {
    svg += "</svg>";
    return svg;
  }

  normalizePressureRange(minP, maxP);
  float range = maxP - minP;

  svg += "<polyline fill='none' class='pressureline' points='";

  for (int i = 0; i < pressureHistoryCount; i++) {
    float value = getPressureHistoryValue(i);
    float x = ((float)i / (pressureHistoryCount - 1)) * 600.0F;
    float normalized = (value - minP) / range;
    float y = 170.0F - normalized * 160.0F;

    svg += String(x, 1);
    svg += ",";
    svg += String(y, 1);
    svg += " ";
  }

  svg += "'/>";
  svg += "<text x='5' y='176' class='svglabel'>-3h</text>";
  svg += "<text x='595' y='176' text-anchor='end' class='svglabel'>now</text>";
  svg += "</svg>";

  return svg;
}

// ============================================================
// Web content builders
// ============================================================

void appendMetricCard(String& html, const char* label, const String& value, const char* unit = "") {
  html += "<div class='card'><div class='label'>";
  html += label;
  html += "</div><div class='value'>";
  html += value;

  if (unit[0] != '\0') {
    html += "<span class='unit'> ";
    html += unit;
    html += "</span>";
  }

  html += "</div></div>";
}

String buildRootHtml() {
  String html;
  html.reserve(12000);

  html +=
    "<!DOCTYPE html><html lang='ja'><head>"
    "<meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>";

  html += "<meta http-equiv='refresh' content='";
  html += String(WEB_REFRESH_SECONDS);
  html += "'>";

  html += "<title>";
  html += NODE_NAME;
  html += " | DEN-HOME LAB</title>";

  html += R"rawliteral(
<style>
*{box-sizing:border-box}
body{margin:0;padding:24px;font-family:system-ui,-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;background:#111;color:#eee}
header{margin-bottom:20px}
h1{margin:0;font-size:1.8rem}
.sub{opacity:.65;margin-top:6px}
.cards{display:grid;grid-template-columns:repeat(5,minmax(150px,1fr));gap:14px}
.card{background:#1c1c1c;border:1px solid #333;border-radius:12px;padding:18px}
.label{font-size:.85rem;opacity:.65}
.value{margin-top:8px;font-size:2rem;font-weight:600}
.unit{font-size:.9rem;opacity:.65}
.graphcard{margin-top:14px;background:#1c1c1c;border:1px solid #333;border-radius:12px;padding:18px}
.pressuregraph{display:block;width:100%;height:220px;margin-top:12px}
.grid{stroke:#333;stroke-width:1}
.pressureline{stroke:currentColor;stroke-width:3;vector-effect:non-scaling-stroke}
.svgtext,.svglabel{fill:currentColor;opacity:.65;font-size:14px}
.status{display:grid;grid-template-columns:repeat(5,minmax(150px,1fr));gap:14px;margin-top:14px}
.small{font-size:1.05rem}
a{color:inherit}
@media(max-width:900px){.cards{grid-template-columns:repeat(2,1fr)}}
@media(max-width:700px){body{padding:14px}.cards,.status{grid-template-columns:1fr}.pressuregraph{height:180px}h1{font-size:1.5rem}}
</style>
)rawliteral";

  html += "</head><body>";
  html += "<header><h1>DEN-HOME LAB Environment</h1><div class='sub'>";
  html += NODE_NAME;
  html += " / ";
  html += networkTypeName();
  html += " / ";
  html += networkLocalIP().toString();
  html += "</div></header>";

  html += "<div class='cards'>";
  appendMetricCard(html, "AHT20 Temperature", isnan(ahtTemp) ? "N/A" : String(ahtTemp, 1), "°C");
  appendMetricCard(html, "Humidity", isnan(humidity) ? "N/A" : String(humidity, 1), "%");
  appendMetricCard(html, "Pressure", isnan(pressure) ? "N/A" : String(pressure, 1), "hPa");
  appendMetricCard(html, "BMP280 Temperature", isnan(bmpTemp) ? "N/A" : String(bmpTemp, 1), "°C");

#if HAS_SCD40
  appendMetricCard(html, "CO2", (hasSCD40 && co2ppm > 0) ? String(co2ppm) : "N/A", "ppm");
  appendMetricCard(html, "SCD40 Temperature", isnan(scdTemp) ? "N/A" : String(scdTemp, 1), "°C");
  appendMetricCard(html, "SCD40 Humidity", isnan(scdHumidity) ? "N/A" : String(scdHumidity, 1), "%");
#endif
  html += "</div>";

  html += "<div class='graphcard'><div class='label'>Pressure trend / last 3 hours</div>";
  html += buildPressureSVG();
  html += "</div>";

  html += "<div class='status'>";
  html += "<div class='card'><div class='label'>Network</div><div class='value small'>";
  html += networkTypeName();
  html += "</div></div>";

  html += "<div class='card'><div class='label'>Link / Signal</div><div class='value small'>";
#if NETWORK_WIFI
  html += networkConnected() ? String(networkRSSI()) + " dBm" : "Wi-Fi DOWN";
#else
  html += networkConnected() ? String("LINK UP / ") + wiznetHardwareName() : "LINK DOWN";
#endif
  html += "</div></div>";

  html += "<div class='card'><div class='label'>Uptime</div><div class='value small'>";
  html += String(millis() / 1000UL);
  html += " sec</div></div>";

  html += "<div class='card'><div class='label'>Sensors</div><div class='value small'>";
  html += hasAHT ? "AHT20 OK" : "AHT20 NG";
  html += "<br>";
  html += hasBMP ? "BMP280 OK" : "BMP280 NG";
#if HAS_SCD40
  html += "<br>";
  html += hasSCD40 ? "SCD40 OK" : "SCD40 NG";
#endif
  html += "</div></div>";

  html += "<div class='card'><div class='label'>Free Heap</div><div class='value small'>";
  html += String(ESP.getFreeHeap());
  html += " bytes</div></div>";
  html += "</div>";

  html += "<p class='sub'><a href='/metrics'>Prometheus /metrics</a> · <a href='/status'>/status JSON</a>";
#if ENABLE_WEB_OTA
  html += " · <a href='/update'>Firmware Update</a>";
#endif
  if (mdnsReady) {
    html += " · mDNS: <a href='http://";
    html += NODE_NAME;
    html += ".local/'>http://";
    html += NODE_NAME;
    html += ".local/</a>";
  }
  html += "</p></body></html>";
  return html;
}

String buildMetrics() {
  String metrics;
  metrics.reserve(3200);

  metrics += "# HELP den_env_temperature_celsius Environment temperature in Celsius\n";
  metrics += "# TYPE den_env_temperature_celsius gauge\n";
  if (!isnan(ahtTemp)) {
    metrics += "den_env_temperature_celsius{node=\"" + String(NODE_NAME) + "\",sensor=\"aht20\"} " + String(ahtTemp, 2) + "\n";
  }
  if (!isnan(bmpTemp)) {
    metrics += "den_env_temperature_celsius{node=\"" + String(NODE_NAME) + "\",sensor=\"bmp280\"} " + String(bmpTemp, 2) + "\n";
  }
#if HAS_SCD40
  if (!isnan(scdTemp)) {
    metrics += "den_env_temperature_celsius{node=\"" + String(NODE_NAME) + "\",sensor=\"scd40\"} " + String(scdTemp, 2) + "\n";
  }
#endif

  metrics += "# HELP den_env_humidity_percent Relative humidity percent\n";
  metrics += "# TYPE den_env_humidity_percent gauge\n";
  if (!isnan(humidity)) {
    metrics += "den_env_humidity_percent{node=\"" + String(NODE_NAME) + "\",sensor=\"aht20\"} " + String(humidity, 2) + "\n";
  }
#if HAS_SCD40
  if (!isnan(scdHumidity)) {
    metrics += "den_env_humidity_percent{node=\"" + String(NODE_NAME) + "\",sensor=\"scd40\"} " + String(scdHumidity, 2) + "\n";
  }
#endif

  metrics += "# HELP den_env_pressure_hpa Atmospheric pressure in hPa\n";
  metrics += "# TYPE den_env_pressure_hpa gauge\n";
  if (!isnan(pressure)) {
    metrics += "den_env_pressure_hpa{node=\"" + String(NODE_NAME) + "\",sensor=\"bmp280\"} " + String(pressure, 2) + "\n";
  }

#if HAS_SCD40
  metrics += "# HELP den_env_co2_ppm Carbon dioxide concentration in ppm\n";
  metrics += "# TYPE den_env_co2_ppm gauge\n";
  if (hasSCD40 && co2ppm > 0) {
    metrics += "den_env_co2_ppm{node=\"" + String(NODE_NAME) + "\",sensor=\"scd40\"} " + String(co2ppm) + "\n";
  }
#endif

#if NETWORK_WIFI
  metrics += "# HELP den_wifi_rssi_dbm WiFi signal strength\n";
  metrics += "# TYPE den_wifi_rssi_dbm gauge\n";
  metrics += "den_wifi_rssi_dbm{node=\"" + String(NODE_NAME) + "\"} " + String(networkRSSI()) + "\n";
#endif

  metrics += "# HELP den_network_link_up Network link state\n";
  metrics += "# TYPE den_network_link_up gauge\n";
  metrics += "den_network_link_up{node=\"" + String(NODE_NAME) + "\",transport=\"" + String(networkTypeName()) + "\"} ";
  metrics += networkConnected() ? "1\n" : "0\n";

  metrics += "# HELP den_network_info Network transport information\n";
  metrics += "# TYPE den_network_info gauge\n";
  metrics += "den_network_info{node=\"" + String(NODE_NAME) + "\",transport=\"" + String(networkTypeName()) + "\",chip=\"" + String(wiznetHardwareName()) + "\"} 1\n";

  metrics += "# HELP den_uptime_seconds ESP32 uptime seconds\n";
  metrics += "# TYPE den_uptime_seconds counter\n";
  metrics += "den_uptime_seconds{node=\"" + String(NODE_NAME) + "\"} " + String(millis() / 1000UL) + "\n";

  metrics += "# HELP den_free_heap_bytes ESP32 free heap\n";
  metrics += "# TYPE den_free_heap_bytes gauge\n";
  metrics += "den_free_heap_bytes{node=\"" + String(NODE_NAME) + "\"} " + String(ESP.getFreeHeap()) + "\n";

  metrics += "# HELP den_sensor_present Sensor present\n";
  metrics += "# TYPE den_sensor_present gauge\n";
  metrics += "den_sensor_present{node=\"" + String(NODE_NAME) + "\",sensor=\"aht20\"} " + String(hasAHT ? 1 : 0) + "\n";
  metrics += "den_sensor_present{node=\"" + String(NODE_NAME) + "\",sensor=\"bmp280\"} " + String(hasBMP ? 1 : 0) + "\n";
#if HAS_SCD40
  metrics += "den_sensor_present{node=\"" + String(NODE_NAME) + "\",sensor=\"scd40\"} " + String(hasSCD40 ? 1 : 0) + "\n";
#endif

  metrics += "# HELP den_pressure_history_points Pressure history points stored in RAM\n";
  metrics += "# TYPE den_pressure_history_points gauge\n";
  metrics += "den_pressure_history_points{node=\"" + String(NODE_NAME) + "\"} " + String(pressureHistoryCount) + "\n";

  metrics += "# HELP den_onboard_oled_enabled Onboard OLED enabled in firmware\n";
  metrics += "# TYPE den_onboard_oled_enabled gauge\n";
  metrics += "den_onboard_oled_enabled{node=\"" + String(NODE_NAME) + "\"} ";
#if HAS_ONBOARD_OLED
  metrics += "1\n";
#else
  metrics += "0\n";
#endif

  metrics += "# HELP den_external_oled_enabled External OLED enabled in firmware\n";
  metrics += "# TYPE den_external_oled_enabled gauge\n";
  metrics += "den_external_oled_enabled{node=\"" + String(NODE_NAME) + "\"} ";
#if HAS_EXTERNAL_OLED
  metrics += "1\n";
#else
  metrics += "0\n";
#endif

  return metrics;
}

void appendJsonFloat(String& json, const char* key, float value) {
  json += "\"";
  json += key;
  json += "\":";
  if (isnan(value)) json += "null";
  else json += String(value, 2);
}

String buildStatusJson() {
  String json;
  json.reserve(1000);
  json += "{";
  json += "\"node\":\"" + String(NODE_NAME) + "\",";
  json += "\"network\":\"" + String(networkTypeName()) + "\",";
  json += "\"ip\":\"" + networkLocalIP().toString() + "\",";
  json += "\"link\":" + String(networkConnected() ? "true" : "false") + ",";
#if NETWORK_WIFI
  json += "\"rssi\":" + String(networkConnected() ? networkRSSI() : 0) + ",";
#else
  json += "\"rssi\":null,";
#endif
  json += "\"chip\":\"" + String(wiznetHardwareName()) + "\",";
  appendJsonFloat(json, "aht_temp", ahtTemp); json += ",";
  appendJsonFloat(json, "humidity", humidity); json += ",";
  appendJsonFloat(json, "bmp_temp", bmpTemp); json += ",";
  appendJsonFloat(json, "pressure", pressure); json += ",";
#if HAS_SCD40
  json += "\"co2\":";
  if (hasSCD40 && co2ppm > 0) json += String(co2ppm); else json += "null";
  json += ",";
  appendJsonFloat(json, "scd_temp", scdTemp); json += ",";
  appendJsonFloat(json, "scd_humidity", scdHumidity); json += ",";
#else
  json += "\"co2\":null,\"scd_temp\":null,\"scd_humidity\":null,";
#endif
  json += "\"aht_present\":" + String(hasAHT ? "true" : "false") + ",";
  json += "\"bmp_present\":" + String(hasBMP ? "true" : "false") + ",";
#if HAS_SCD40
  json += "\"scd40_present\":" + String(hasSCD40 ? "true" : "false") + ",";
#else
  json += "\"scd40_present\":false,";
#endif
  json += "\"uptime\":" + String(millis() / 1000UL) + ",";
  json += "\"free_heap\":" + String(ESP.getFreeHeap());
  json += "}";
  return json;
}

// ============================================================
// Wi-Fi Web server / OTA / mDNS
// ============================================================

#if NETWORK_WIFI

void handleRoot() {
  server.send(200, "text/html; charset=utf-8", buildRootHtml());
}

void handleMetrics() {
  server.send(200, "text/plain; version=0.0.4; charset=utf-8", buildMetrics());
}

void handleStatus() {
  server.send(200, "application/json; charset=utf-8", buildStatusJson());
}

#if ENABLE_WEB_OTA
bool webOtaAuthenticated() {
  if (WEB_OTA_PASSWORD == nullptr || WEB_OTA_PASSWORD[0] == '\0') return true;
  return server.authenticate(WEB_OTA_USERNAME, WEB_OTA_PASSWORD);
}

void requestWebOtaAuthentication() {
  server.requestAuthentication();
}

void handleWebUpdatePage() {
  if (!webOtaAuthenticated()) {
    requestWebOtaAuthentication();
    return;
  }

  String html;
  html.reserve(3500);
  html += R"rawliteral(
<!DOCTYPE html><html lang='ja'><head><meta charset='UTF-8'>
<meta name='viewport' content='width=device-width,initial-scale=1'>
<title>Firmware Update | DEN-HOME LAB</title>
<style>*{box-sizing:border-box}body{margin:0;padding:24px;font-family:system-ui;background:#111;color:#eee}main{max-width:720px;margin:auto}.card{background:#1c1c1c;border:1px solid #333;border-radius:12px;padding:20px;margin-top:16px}input[type=file]{display:block;width:100%;padding:12px;background:#111;border:1px solid #444;border-radius:8px;color:#eee;margin:14px 0}button{width:100%;padding:12px;border:0;border-radius:8px;font-size:1rem;font-weight:700}a{color:inherit}.sub{opacity:.7}</style></head><body><main>
)rawliteral";
  html += "<h1>DEN-HOME LAB Firmware Update</h1><div class='sub'>" + String(NODE_NAME) + " / " + networkLocalIP().toString() + "</div>";
  html += R"rawliteral(<div class='card'><p>Arduino IDEで出力した <strong>.bin</strong> ファイルを選択してください。書き込み中は電源・ネットワークを切断しないでください。</p><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='firmware' accept='.bin,application/octet-stream' required><button type='submit'>ファームウェアを書き込む</button></form></div><p class='sub'><a href='/'>← 戻る</a></p></main></body></html>)rawliteral";
  server.send(200, "text/html; charset=utf-8", html);
}

void handleWebUpdateUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    webOtaUploadAuthorized = webOtaAuthenticated();
    webOtaUpdateInProgress = false;
    if (!webOtaUploadAuthorized) return;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_OTA
    Serial.print("[WEB-OTA] Start filename="); Serial.println(upload.filename);
#endif
    showStatusOnDisplays("Web OTA", "Starting...");
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) return;
    webOtaUpdateInProgress = true;
  } else if (webOtaUploadAuthorized && webOtaUpdateInProgress && upload.status == UPLOAD_FILE_WRITE) {
    Update.write(upload.buf, upload.currentSize);
  } else if (webOtaUploadAuthorized && webOtaUpdateInProgress && upload.status == UPLOAD_FILE_END) {
    bool success = Update.end(true);
    webOtaUpdateInProgress = false;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_OTA
    Serial.println(success ? "[WEB-OTA] Done" : "[WEB-OTA] FAILED");
#endif
    showStatusOnDisplays(success ? "Web OTA Done" : "Web OTA Error", success ? "Rebooting" : "Update failed");
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    webOtaUpdateInProgress = false;
  }
}

void handleWebUpdateFinished() {
  if (!webOtaAuthenticated()) { requestWebOtaAuthentication(); return; }
  bool success = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(success ? 200 : 500, "text/html; charset=utf-8",
              success ? "<html><meta charset='UTF-8'><body><h2>更新成功</h2><p>再起動します。</p></body></html>"
                      : "<html><meta charset='UTF-8'><body><h2>更新失敗</h2></body></html>");
  if (success) { delay(500); ESP.restart(); }
}
#endif

void setupWebServer() {
  if (!webServerConfigured) {
    server.on("/", HTTP_GET, handleRoot);

    // 末尾スラッシュあり/なしの両方を受け付ける
    server.on("/metrics", HTTP_GET, handleMetrics);
    server.on("/metrics/", HTTP_GET, handleMetrics);

    server.on("/status", HTTP_GET, handleStatus);
    server.on("/status/", HTTP_GET, handleStatus);

#if ENABLE_WEB_OTA
    server.on("/update", HTTP_GET, handleWebUpdatePage);
    server.on("/update/", HTTP_GET, handleWebUpdatePage);

    server.on("/update", HTTP_POST, handleWebUpdateFinished, handleWebUpdateUpload);
    server.on("/update/", HTTP_POST, handleWebUpdateFinished, handleWebUpdateUpload);
#endif

    webServerConfigured = true;
  }

  server.begin();

#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_NETWORK_SERVICES
  Serial.println("[NET] HTTP server ready port=80");
#endif
}

void setupMDNS() {
  mdnsReady = false;
  if (WiFi.status() != WL_CONNECTED) return;
  if (MDNS.begin(NODE_NAME)) {
    mdnsReady = true;
    MDNS.addService("http", "tcp", 80);
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_NETWORK_SERVICES
    Serial.print("[NET] mDNS ready url=http://"); Serial.print(NODE_NAME); Serial.println(".local/");
#endif
  }
}

int lastOtaProgressBucket = -1;
void setupOTA() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (!otaConfigured) {
    ArduinoOTA.setHostname(NODE_NAME);
    if (OTA_PASSWORD && OTA_PASSWORD[0] != '\0') ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.onStart([](){
      lastOtaProgressBucket = -1;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_OTA
      Serial.println("[OTA] Start");
#endif
      showStatusOnDisplays("OTA Start", "Updating...");
    });
    ArduinoOTA.onEnd([](){
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_OTA
      Serial.println("[OTA] Done");
#endif
      showStatusOnDisplays("OTA Done", "Rebooting");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total){
      unsigned int percent = (progress * 100U) / total;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_OTA_PROGRESS
      int bucket = (int)(percent / 10U) * 10;
      if (bucket != lastOtaProgressBucket) { lastOtaProgressBucket = bucket; Serial.printf("[OTA] Progress=%d%%\n", bucket); }
#endif
      char line[20]; snprintf(line, sizeof(line), "%u %%", percent); showStatusOnDisplays("OTA Update", line);
    });
    ArduinoOTA.onError([](ota_error_t error){
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_OTA
      Serial.print("[OTA] Error="); Serial.println(error);
#endif
      showStatusOnDisplays("OTA Error", "Update failed");
    });
    otaConfigured = true;
  }
  ArduinoOTA.begin();
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_OTA
  Serial.println("[OTA] Ready");
#endif
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
#if WIFI_AUTO_RETRY
  WiFi.setAutoReconnect(true);
#endif
#if USE_STATIC_IP
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_WIFI
  Serial.println("[WIFI] Addressing=Static");
#endif
  WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
#else
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_WIFI
  Serial.println("[WIFI] Addressing=DHCP");
#endif
#endif
  showStatusOnDisplays("WiFi connect");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 80) { delay(250); retry++; }
  if (WiFi.status() == WL_CONNECTED) {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_WIFI
    Serial.print("[WIFI] Connected IP="); Serial.print(WiFi.localIP()); Serial.print(" RSSI="); Serial.println(WiFi.RSSI());
#endif
    String ip = WiFi.localIP().toString(); showStatusOnDisplays("WiFi OK", ip.c_str());
  } else {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_WIFI
    Serial.println("[WIFI] FAILED");
#endif
    showStatusOnDisplays("WiFi failed", "offline");
  }
  wifiWasConnected = (WiFi.status() == WL_CONNECTED);
  lastWiFiRetry = millis();
}

void startNetworkServices() {
  if (networkServicesStarted || WiFi.status() != WL_CONNECTED) return;
  setupMDNS();
  setupWebServer();
  setupOTA();
  networkServicesStarted = true;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_NETWORK_SERVICES
  Serial.println("[NET] Network services started");
#endif
}

void stopNetworkServices() {
  if (!networkServicesStarted) { mdnsReady = false; return; }
#if ENABLE_WEB_OTA
  if (webOtaUpdateInProgress) Update.abort();
  webOtaUploadAuthorized = false;
  webOtaUpdateInProgress = false;
#endif
  server.stop();
  ArduinoOTA.end();
  MDNS.end();
  mdnsReady = false;
  networkServicesStarted = false;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_NETWORK_SERVICES
  Serial.println("[NET] Network services stopped");
#endif
}

void maintainWiFi() {
  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected) {
    if (!wifiWasConnected) {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_WIFI
      Serial.print("[WIFI] Reconnected IP=");
      Serial.print(WiFi.localIP());
      Serial.print(" RSSI=");
      Serial.println(WiFi.RSSI());
#endif
      String ip = WiFi.localIP().toString();
      showStatusOnDisplays("WiFi OK", ip.c_str());
    }

    wifiWasConnected = true;

    // 起動直後にWi-Fi接続済みでも、HTTP / mDNS / OTAが
    // まだ開始されていなければ必ず起動する。
    if (!networkServicesStarted) {
      startNetworkServices();
    }

    return;
  }
  if (wifiWasConnected) {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_WIFI
    Serial.println("[WIFI] Disconnected");
#endif
    showStatusOnDisplays("WiFi lost", "retrying");
    stopNetworkServices();
    wifiWasConnected = false;
  }
#if WIFI_AUTO_RETRY
  unsigned long now = millis();
  if (now - lastWiFiRetry >= WIFI_RETRY_INTERVAL_MS) {
    lastWiFiRetry = now;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_WIFI
    Serial.println("[WIFI] Retry...");
#endif
    WiFi.reconnect();
  }
#endif
}

#else // Ethernet =================================================

// ------------------------------------------------------------
// Ethernet helpers
// ------------------------------------------------------------

void buildEthernetMac() {
#if USE_AUTO_ETHERNET_MAC
  uint64_t id = ESP.getEfuseMac();
  ethernetMac[0] = 0x02; // locally administered unicast MAC
  ethernetMac[1] = (id >>  0) & 0xFF;
  ethernetMac[2] = (id >>  8) & 0xFF;
  ethernetMac[3] = (id >> 16) & 0xFF;
  ethernetMac[4] = (id >> 24) & 0xFF;
  ethernetMac[5] = (id >> 32) & 0xFF;
#else
  memcpy(ethernetMac, MANUAL_ETHERNET_MAC, 6);
#endif
}

void resetWiznet() {
  pinMode(WIZNET_RST, OUTPUT);
  digitalWrite(WIZNET_RST, HIGH); delay(50);
  digitalWrite(WIZNET_RST, LOW);  delay(100);
  digitalWrite(WIZNET_RST, HIGH); delay(300);
}

void logEthernetStatus() {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_ETHERNET
  Serial.print("[ETH] Chip="); Serial.print(wiznetHardwareName());
  Serial.print(" Link="); Serial.print(Ethernet.linkStatus() == LinkON ? "UP" : "DOWN");
  Serial.print(" IP="); Serial.println(Ethernet.localIP());
#endif
}

bool beginEthernetAddressing() {
#if USE_STATIC_IP
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_ETHERNET
  Serial.println("[ETH] Addressing=Static");
#endif
  Ethernet.begin(ethernetMac, local_IP, primaryDNS, gateway, subnet);
  delay(300);
  return !ipIsZero(Ethernet.localIP());
#else
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_ETHERNET
  Serial.println("[ETH] Addressing=DHCP");
#endif
  int ok = Ethernet.begin(ethernetMac, ETHERNET_DHCP_TIMEOUT_MS, ETHERNET_DHCP_RESPONSE_TIMEOUT_MS);
  return ok == 1;
#endif
}

// ------------------------------------------------------------
// Ethernet mDNS
//
// ArduinoMDNSはEthernetUDPのマルチキャスト機能を使います。
// HTTPサーバと同じく、LINK UP + IP確定後に開始します。
// ------------------------------------------------------------
void startEthernetMDNS() {
#if ENABLE_ETHERNET_MDNS
  if (ethernetMdnsStarted) {
    mdnsReady = true;
    return;
  }

  if (!ethernetInitialized) return;
  if (Ethernet.linkStatus() != LinkON) return;
  if (ipIsZero(Ethernet.localIP())) return;

  int result = ethernetMdns.begin(Ethernet.localIP(), NODE_NAME);

  if (result == MDNSSuccess) {
    ethernetMdnsStarted = true;
    mdnsReady = true;

    String serviceName = String(NODE_NAME) + "._http";
    int serviceResult = ethernetMdns.addServiceRecord(
      serviceName.c_str(),
      80,
      MDNSServiceTCP
    );

#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_NETWORK_SERVICES
    Serial.print("[NET] Ethernet mDNS ready url=http://");
    Serial.print(NODE_NAME);
    Serial.println(".local/");
    Serial.print("[NET] Ethernet mDNS HTTP service=");
    Serial.println(serviceResult == MDNSSuccess ? "OK" : "FAILED");
#endif
  } else {
    ethernetMdnsStarted = false;
    mdnsReady = false;

#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_NETWORK_SERVICES
    Serial.print("[NET] Ethernet mDNS FAILED code=");
    Serial.println(result);
#endif
  }
#else
  mdnsReady = false;
#endif
}

void startEthernetServer() {
  // 固定IPではリンク確立前でもlocalIP()に値が入るため、
  // LINK UPを確認してからHTTPサーバを開始する。
  if (ethernetServerStarted) return;
  if (!ethernetInitialized) return;
  if (Ethernet.linkStatus() != LinkON) return;
  if (ipIsZero(Ethernet.localIP())) return;

  ethernetServer.begin();
  ethernetServerStarted = true;
  networkServicesStarted = true;

#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_NETWORK_SERVICES
  Serial.println("[NET] Ethernet HTTP server ready port=80");
#endif

  // HTTPが待受可能になった後でmDNSを開始する
  startEthernetMDNS();
}

void setupEthernet() {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_ETHERNET
  Serial.printf("[ETH] SPI MISO=%d CS=%d SCLK=%d MOSI=%d RST=%d\n", WIZNET_MISO, WIZNET_CS, WIZNET_SCLK, WIZNET_MOSI, WIZNET_RST);
#endif
  buildEthernetMac();
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_ETHERNET
  Serial.printf("[ETH] MAC=%02X:%02X:%02X:%02X:%02X:%02X\n", ethernetMac[0], ethernetMac[1], ethernetMac[2], ethernetMac[3], ethernetMac[4], ethernetMac[5]);
#endif
  resetWiznet();
  SPI.begin(WIZNET_SCLK, WIZNET_MISO, WIZNET_MOSI, WIZNET_CS);
  Ethernet.init(WIZNET_CS);
  beginEthernetAddressing();
  ethernetInitialized = (Ethernet.hardwareStatus() != EthernetNoHardware);

#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_ETHERNET
  if (!ethernetInitialized) Serial.println("[ETH] ERROR: WIZnet controller not found");
#if NETWORK_W5500
  else if (Ethernet.hardwareStatus() != EthernetW5500) Serial.println("[ETH] WARNING: NETWORK_W5500 selected, but detected chip is not W5500");
#elif NETWORK_W5200
  else if (Ethernet.hardwareStatus() != EthernetW5200) Serial.println("[ETH] WARNING: NETWORK_W5200 selected, but detected chip is not W5200");
#endif
#endif

  ethernetWasLinked = (Ethernet.linkStatus() == LinkON);
  logEthernetStatus();

  // HTTPサーバはリンク確立後に開始する。
  if (ethernetInitialized && ethernetWasLinked && !ipIsZero(Ethernet.localIP())) {
    startEthernetServer();
  } else {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_NETWORK_SERVICES
    if (ethernetInitialized && !ipIsZero(Ethernet.localIP())) {
      Serial.println("[NET] Ethernet HTTP server waiting for LINK UP");
    }
#endif
  }

  lastEthernetRetry = millis();

  if (networkConnected()) {
    String ip = Ethernet.localIP().toString(); showStatusOnDisplays("Ethernet OK", ip.c_str());
  } else {
    showStatusOnDisplays("Ethernet", "offline");
  }
}

// ------------------------------------------------------------
// Ethernet HTTP server
// ------------------------------------------------------------

String base64Encode(const String& input) {
  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  int val = 0, valb = -6;
  for (size_t i = 0; i < input.length(); i++) {
    val = (val << 8) + (uint8_t)input[i];
    valb += 8;
    while (valb >= 0) { out += table[(val >> valb) & 0x3F]; valb -= 6; }
  }
  if (valb > -6) out += table[((val << 8) >> (valb + 8)) & 0x3F];
  while (out.length() % 4) out += '=';
  return out;
}

bool ethernetOtaAuthenticated(const String& authorization) {
  if (WEB_OTA_PASSWORD == nullptr || WEB_OTA_PASSWORD[0] == '\0') return true;
  String expected = "Basic " + base64Encode(String(WEB_OTA_USERNAME) + ":" + String(WEB_OTA_PASSWORD));
  return authorization == expected;
}

// ------------------------------------------------------------
// Ethernet HTTP送信補助
//
// Arduino Ethernet + W5x00では1回の大きなwrite/printが
// W5500の送信バッファサイズを超えると、末尾が送信されない場合がある。
// そのため送信可能量を確認しながら小分けにして最後まで送る。
// ------------------------------------------------------------
bool ethernetWriteAll(EthernetClient& client,
                      const uint8_t* data,
                      size_t length,
                      unsigned long timeoutMs = 5000UL) {
  size_t offset = 0;
  unsigned long lastProgress = millis();

  while (offset < length) {
    if (!client.connected()) return false;

    int writable = client.availableForWrite();

    if (writable <= 0) {
      if (millis() - lastProgress >= timeoutMs) return false;
      delay(1);
      continue;
    }

    size_t remaining = length - offset;
    size_t chunk = remaining;

    // W5500側の現在の空き容量を超えて投げない
    if (chunk > (size_t)writable) chunk = (size_t)writable;

    // さらに512byteずつに制限して安全側で送信する
    if (chunk > 512U) chunk = 512U;

    size_t written = client.write(data + offset, chunk);

    if (written == 0) {
      if (millis() - lastProgress >= timeoutMs) return false;
      delay(1);
      continue;
    }

    offset += written;
    lastProgress = millis();
  }

  return true;
}

bool ethernetWriteAll(EthernetClient& client,
                      const String& data,
                      unsigned long timeoutMs = 5000UL) {
  return ethernetWriteAll(
    client,
    reinterpret_cast<const uint8_t*>(data.c_str()),
    data.length(),
    timeoutMs
  );
}

void ethernetSendResponse(EthernetClient& client,
                          int status,
                          const char* statusText,
                          const char* contentType,
                          const String& body,
                          const char* extraHeader = nullptr) {
  String header;
  header.reserve(220);

  header += "HTTP/1.1 ";
  header += String(status);
  header += " ";
  header += statusText;
  header += "\r\n";

  header += "Content-Type: ";
  header += contentType;
  header += "\r\n";

  header += "Content-Length: ";
  header += String(body.length());
  header += "\r\n";

  header += "Connection: close\r\n";

  if (extraHeader && extraHeader[0]) {
    header += extraHeader;
    header += "\r\n";
  }

  header += "\r\n";

  bool headerOk = ethernetWriteAll(client, header);
  bool bodyOk = false;

  if (headerOk) {
    bodyOk = ethernetWriteAll(client, body);
  }

#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_NETWORK_SERVICES
  Serial.print("[HTTP][ETH] Status=");
  Serial.print(status);
  Serial.print(" Body=");
  Serial.print(body.length());
  Serial.print(" bytes TX=");
  Serial.println((headerOk && bodyOk) ? "OK" : "FAILED");
#endif
}

void ethernetSendUnauthorized(EthernetClient& client) {
  String body = "Authentication required";
  ethernetSendResponse(client, 401, "Unauthorized", "text/plain; charset=utf-8", body,
                       "WWW-Authenticate: Basic realm=\"DEN-HOME LAB OTA\"");
}

bool readEthernetHttpRequest(EthernetClient& client, EthernetHttpRequest& req) {
  unsigned long start = millis();
  String line;
  bool firstLine = true;

  while (client.connected() && millis() - start < 3000UL) {
    while (client.available()) {
      char c = client.read();
      if (c == '\r') continue;
      if (c == '\n') {
        if (firstLine) {
          int p1 = line.indexOf(' ');
          int p2 = line.indexOf(' ', p1 + 1);
          if (p1 < 0 || p2 < 0) return false;
          req.method = line.substring(0, p1);
          req.path = line.substring(p1 + 1, p2);

          // クエリ文字列はルーティング判定から除外する
          int q = req.path.indexOf('?');
          if (q >= 0) req.path = req.path.substring(0, q);

          // "/" 以外は末尾スラッシュを取り除いて正規化する。
          // 例:
          //   /status/   -> /status
          //   /metrics/  -> /metrics
          //   /update/   -> /update
          //   /update-bin/ -> /update-bin
          while (req.path.length() > 1 && req.path.endsWith("/")) {
            req.path.remove(req.path.length() - 1);
          }

          firstLine = false;
        } else if (line.length() == 0) {
          return true;
        } else {
          int colon = line.indexOf(':');
          if (colon > 0) {
            String key = line.substring(0, colon); key.trim(); key.toLowerCase();
            String value = line.substring(colon + 1); value.trim();
            if (key == "authorization") req.authorization = value;
            else if (key == "content-length") req.contentLength = (size_t)value.toInt();
          }
        }
        line = "";
      } else if (line.length() < 512) {
        line += c;
      }
    }
    delay(1);
  }
  return false;
}

String buildEthernetUpdatePage() {
  String html;
  html.reserve(4500);
  html += R"rawliteral(<!DOCTYPE html><html lang='ja'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>Firmware Update</title><style>*{box-sizing:border-box}body{margin:0;padding:24px;font-family:system-ui;background:#111;color:#eee}main{max-width:720px;margin:auto}.card{background:#1c1c1c;border:1px solid #333;border-radius:12px;padding:20px;margin-top:16px}input{display:block;width:100%;padding:12px;margin:14px 0}button{width:100%;padding:12px;font-weight:700}.sub{opacity:.7}a{color:inherit}</style></head><body><main>)rawliteral";
  html += "<h1>DEN-HOME LAB Firmware Update</h1><div class='sub'>" + String(NODE_NAME) + " / " + Ethernet.localIP().toString() + "</div>";
  html += R"rawliteral(<div class='card'><p>.bin ファイルを選択してください。書き込み中は電源とLANを切断しないでください。</p><input id='fw' type='file' accept='.bin,application/octet-stream'><button onclick='uploadFw()'>ファームウェアを書き込む</button><p id='msg'></p></div><p class='sub'><a href='/'>← 戻る</a></p><script>async function uploadFw(){const f=document.getElementById('fw').files[0];const m=document.getElementById('msg');if(!f){m.textContent='ファイルを選択してください';return;}m.textContent='書き込み中...';try{const r=await fetch('/update-bin',{method:'POST',body:f,credentials:'same-origin'});m.innerHTML=await r.text();}catch(e){m.textContent='通信が切断されました。更新後の再起動であれば正常な場合があります。';}}</script></main></body></html>)rawliteral";
  return html;
}

void handleEthernetFirmwareUpload(EthernetClient& client, const EthernetHttpRequest& req) {
#if !ENABLE_WEB_OTA
  ethernetSendResponse(client, 404, "Not Found", "text/plain", "Web OTA disabled");
  return;
#else
  if (!ethernetOtaAuthenticated(req.authorization)) { ethernetSendUnauthorized(client); return; }
  if (req.contentLength == 0) {
    ethernetSendResponse(client, 411, "Length Required", "text/plain; charset=utf-8", "Content-Length がありません");
    return;
  }
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_OTA
  Serial.print("[WEB-OTA][ETH] Start bytes="); Serial.println(req.contentLength);
#endif
  showStatusOnDisplays("Web OTA", "Starting...");
  if (!Update.begin(req.contentLength)) {
    ethernetSendResponse(client, 500, "Internal Server Error", "text/plain; charset=utf-8", "Update.begin failed");
    return;
  }

  uint8_t buffer[1024];
  size_t received = 0;
  unsigned long lastData = millis();
  bool writeOk = true;

  while (received < req.contentLength && client.connected()) {
    int available = client.available();
    if (available > 0) {
      size_t want = min((size_t)available, min(sizeof(buffer), req.contentLength - received));
      int n = client.read(buffer, want);
      if (n > 0) {
        if (Update.write(buffer, n) != (size_t)n) { writeOk = false; break; }
        received += n;
        lastData = millis();
      }
    } else {
      if (millis() - lastData > 60000UL) { writeOk = false; break; }
      delay(1);
    }
  }

  bool success = writeOk && received == req.contentLength && Update.end(true);
  if (!success) {
    Update.abort();
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_OTA
    Serial.print("[WEB-OTA][ETH] FAILED received="); Serial.print(received); Serial.print(" error="); Serial.println(Update.getError());
#endif
    ethernetSendResponse(client, 500, "Internal Server Error", "text/html; charset=utf-8", "<h2>更新失敗</h2><p>シリアルログを確認してください。</p>");
    return;
  }

#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_OTA
  Serial.print("[WEB-OTA][ETH] Done bytes="); Serial.println(received);
#endif
  ethernetSendResponse(client, 200, "OK", "text/html; charset=utf-8", "<h2>更新成功</h2><p>ESP32を再起動します。</p>");
  client.flush();
  delay(700);
  ESP.restart();
#endif
}

void handleEthernetWebServer() {
  if (!ethernetServerStarted) return;
  EthernetClient client = ethernetServer.available();
  if (!client) return;

  EthernetHttpRequest req;
  if (!readEthernetHttpRequest(client, req)) { client.stop(); return; }

  if (req.method == "GET" && req.path == "/") {
    ethernetSendResponse(client, 200, "OK", "text/html; charset=utf-8", buildRootHtml());
  } else if (req.method == "GET" && req.path == "/metrics") {
    ethernetSendResponse(client, 200, "OK", "text/plain; version=0.0.4; charset=utf-8", buildMetrics());
  } else if (req.method == "GET" && req.path == "/status") {
    ethernetSendResponse(client, 200, "OK", "application/json; charset=utf-8", buildStatusJson());
#if ENABLE_WEB_OTA
  } else if (req.method == "GET" && req.path == "/update") {
    if (!ethernetOtaAuthenticated(req.authorization)) ethernetSendUnauthorized(client);
    else ethernetSendResponse(client, 200, "OK", "text/html; charset=utf-8", buildEthernetUpdatePage());
  } else if (req.method == "POST" && req.path == "/update-bin") {
    handleEthernetFirmwareUpload(client, req);
#endif
  } else {
    ethernetSendResponse(client, 404, "Not Found", "text/plain; charset=utf-8", "404 Not Found");
  }
  delay(1);
  client.stop();
}

void retryEthernetIfNeeded() {
#if ETHERNET_AUTO_RETRY
  unsigned long now = millis();
  if (now - lastEthernetRetry < ETHERNET_RETRY_INTERVAL_MS) return;
  lastEthernetRetry = now;

  if (!ethernetInitialized) {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_ETHERNET
    Serial.println("[ETH] Retry hardware init...");
#endif
    setupEthernet();
    return;
  }

#if !USE_STATIC_IP
  if (Ethernet.linkStatus() == LinkON && ipIsZero(Ethernet.localIP())) {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_ETHERNET
    Serial.println("[ETH] Retry DHCP...");
#endif
    beginEthernetAddressing();
    if (!ipIsZero(Ethernet.localIP())) {
      ethernetServerStarted = false;
      startEthernetServer();
    }
  }
#endif
#endif
}

void maintainEthernet() {
  if (ethernetInitialized) {
#if !USE_STATIC_IP
    Ethernet.maintain();
#endif
    bool linked = (Ethernet.linkStatus() == LinkON);

    if (linked != ethernetWasLinked) {
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_ETHERNET
      Serial.println(linked ? "[ETH] Link UP" : "[ETH] Link DOWN");
#endif
      showStatusOnDisplays(linked ? "Ethernet link" : "Ethernet lost",
                           linked ? "UP" : "DOWN");

      if (!linked) {
        // リンク断後は、次のLINK UPでHTTPサーバをやり直せるようにする。
        ethernetServerStarted = false;
        networkServicesStarted = false;
        mdnsReady = false;
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_NETWORK_SERVICES
        Serial.println("[NET] Ethernet HTTP server marked for restart");
        Serial.println("[NET] Ethernet mDNS waiting for LINK UP");
#endif
      }

      ethernetWasLinked = linked;
    }

    // PHYのリンク確立後に初めてHTTPサーバを開始する。
    // リンク断→復旧の場合もここで再度begin()される。
    if (linked && !ethernetServerStarted && !ipIsZero(Ethernet.localIP())) {
      startEthernetServer();
    }

#if ENABLE_ETHERNET_MDNS
    // LINK復帰後にmDNS状態を戻す。
    // 固定IP運用では既存のmDNSソケットをそのまま再利用できます。
    if (linked && ethernetServerStarted && !mdnsReady) {
      startEthernetMDNS();
    }
#endif
  }

  retryEthernetIfNeeded();
}

#endif // NETWORK_WIFI / Ethernet

// ============================================================
// Network common setup / maintenance
// ============================================================

void setupNetwork() {
#if NETWORK_WIFI
  setupWiFi();

  // setupWiFi()内で接続に成功した場合は、
  // 初回loopを待たずにmDNS / HTTP / OTAを開始する。
  if (WiFi.status() == WL_CONNECTED) {
    startNetworkServices();
  }
#else
  setupEthernet();
#endif
}

void maintainNetwork() {
#if NETWORK_WIFI
  maintainWiFi();
#else
  maintainEthernet();
#endif
}

// ============================================================
// SETUP
// ============================================================

void setup() {
#if ENABLE_SERIAL_MONITOR
  Serial.begin(115200);
  delay(1000);
#endif

#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_BOOT
  Serial.print("[BOOT] DEN-HOME LAB ENV NODE Node=");
  Serial.println(NODE_NAME);
#endif

  // External I2C bus shared by sensors + external OLED.
  Wire.begin(SENSOR_SDA, SENSOR_SCL);
  Wire.setClock(100000);

#if HAS_ONBOARD_OLED
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_BOOT
  Serial.println("[BOOT] OnboardOLED=Enabled");
#endif
  onboardOled.begin();
  drawOnboardBoot("Booting...");
#else
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_BOOT
  Serial.println("[BOOT] OnboardOLED=Disabled");
#endif
#endif

#if HAS_EXTERNAL_OLED
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_BOOT
  Serial.println("[BOOT] ExternalOLED=Enabled");
#endif
  externalOled.setI2CAddress(EXTERNAL_OLED_ADDRESS << 1);
  externalOled.begin();
  drawExternalBoot("Booting...");
#else
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_BOOT
  Serial.println("[BOOT] ExternalOLED=Disabled");
#endif
#endif

  // AHT20
  hasAHT = aht.begin(&Wire);
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_SENSOR_STATUS
  Serial.print("[SENSOR] AHT20=");
  Serial.println(hasAHT ? "OK" : "NG");
#endif

  // BMP280
  hasBMP = bmp.begin(0x77);
  if (!hasBMP) {
    hasBMP = bmp.begin(0x76);
  }
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_SENSOR_STATUS
  Serial.print("[SENSOR] BMP280=");
  Serial.println(hasBMP ? "OK" : "NG");
#endif

#if HAS_SCD40
  initSCD40();
#else
#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_SENSOR_STATUS
  Serial.println("[SENSOR] SCD40=Disabled");
#endif
#endif

  setupNetwork();

  readSensors();

  // First pressure history point immediately after boot.
  savePressureHistory();
  lastPressureHistorySave = millis();

#if HAS_ONBOARD_OLED
  drawCurrentOnboardPage();
#endif

#if HAS_EXTERNAL_OLED
  drawExternalDashboard();
#endif

#if ENABLE_SERIAL_MONITOR && SERIAL_EVENT_BOOT
  Serial.println("[BOOT] Setup complete");
#endif
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  maintainNetwork();

#if NETWORK_WIFI
  if (networkServicesStarted && WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    server.handleClient();
  }
#else
#if ENABLE_ETHERNET_MDNS
  if (mdnsReady) {
    ethernetMdns.run();
  }
#endif
  handleEthernetWebServer();
#endif

  unsigned long now = millis();

  // Sensor update
  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;
    readSensors();

#if HAS_EXTERNAL_OLED
    drawExternalDashboard();
#endif
  }

  // Pressure history update
  if (now - lastPressureHistorySave >= PRESSURE_HISTORY_INTERVAL_MS) {
    lastPressureHistorySave = now;
    savePressureHistory();

#if HAS_EXTERNAL_OLED
    drawExternalDashboard();
#endif
  }

#if HAS_ONBOARD_OLED
  // Onboard OLED page switch
  if (now - lastPageChange >= PAGE_INTERVAL_MS) {
    lastPageChange = now;
    onboardPage++;

    if (onboardPage >= getOnboardPageCount()) {
      onboardPage = 0;
    }

    drawCurrentOnboardPage();
  }
#endif
}

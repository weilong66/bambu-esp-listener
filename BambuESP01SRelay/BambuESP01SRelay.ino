/*
 * Bambu ESP-01S Relay Listener —— Arduino IDE 主文件
 * ------------------------------------------------------------------
 * 纯监视器：拓竹打印机局域网状态监视（MQTT/TLS）+ 一路继电器自动/手动控制 + HTTP OTA。
 *
 * 文件结构（同一 sketch 目录内）：
 *   config.h       常量、数据结构、继电器规则（★ 改配置只改这里）
 *   storage.h      EEPROM 配置持久化
 *   relay.h        继电器硬件 + 自动规则引擎
 *   mqtt_client.h  MQTT/TLS 客户端 + 全量状态解析
 *   html_page.h    Web UI / OTA 页面（PROGMEM）
 *   web_server.h   REST API + HTTP OTA
 *
 * Arduino IDE 使用步骤：
 *   1. 开发板管理器安装 "esp8266" (ESP8266 Community) 3.x；
 *   2. 库管理器安装 "PubSubClient" (knolleary) 与 "ArduinoJson" (6.x)；
 *   3. 开发板选 "Generic ESP8266 Module"：
 *        Flash Size : 1MB (FS:64KB OTA:~470KB)
 *        Flash Mode : DOUT
 *        CPU        : 80 MHz
 *        Upload Speed: 115200
 *   4. 烧录时必须把 ESP-01S 从继电器板上拔下，GPIO0 接地后上电进入下载模式。
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "config.h"
#include "storage.h"
#include "relay.h"
#include "mqtt_client.h"
#include "html_page.h"
#include "web_server.h"

/* =================================================================
 * 全局对象与网络管理
 * ================================================================= */

static StorageManager storage;
static DeviceConfig config;
static PrinterStatus printerStatus;
static Relay relay;
static RelayAutomation relayAuto;
static BambuMQTT bambuMQTT;
static WebServerManager webServer;

static bool apMode = false;
static unsigned long lastWifiCheck = 0;
static unsigned long lastRelayUpdate = 0;

static void startAP() {
  apMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("[WiFi] AP started, IP: ");
  Serial.println(WiFi.softAPIP());
}

static void connectSTA() {
  if (strlen(config.wifi_ssid) == 0) {
    startAP();
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.wifi_ssid, config.wifi_password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT) {
    delay(200);
  }

  if (WiFi.status() == WL_CONNECTED) {
    apMode = false;
    Serial.print("[WiFi] STA connected, IP: ");
    Serial.println(WiFi.localIP());
  } else {
    startAP();
  }
}

static void checkNetwork() {
  if (apMode) return;
  if (WiFi.status() == WL_CONNECTED) return;
  if (millis() - lastWifiCheck < 10000UL) return;
  lastWifiCheck = millis();
  Serial.println("[WiFi] reconnecting...");
  WiFi.reconnect();
}

/* =================================================================
 * setup / loop
 * ================================================================= */

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("[Boot] Bambu ESP-01S Listener");

  // 继电器最先初始化，确保上电安全态
  relay.begin();

  storage.begin();
  config = storage.load();

  // 规则引擎引用 config.relay，网页修改后立即生效
  relayAuto.attach(relay, config.relay);

  connectSTA();

  if (strlen(config.printer_ip) > 0) {
    bambuMQTT.begin(config, printerStatus);
  }

  webServer.begin(config, printerStatus, storage, bambuMQTT, relayAuto);

  Serial.println("[Boot] ready");
}

void loop() {
  checkNetwork();

  // 继电器规则按 1s 周期执行
  if (millis() - lastRelayUpdate >= RELAY_UPDATE_INTERVAL) {
    lastRelayUpdate = millis();
    relayAuto.update(printerStatus, lastRelayUpdate);
  }

  webServer.loop();

  if (!apMode) {
    bambuMQTT.loop();
  }
  delay(2);
}

#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <Updater.h>
#include <ArduinoJson.h>
#include "config.h"
#include "html_page.h"
#include "storage.h"
#include "mqtt_client.h"
#include "relay.h"

/* 纯监视器 Web 服务：状态展示 + 继电器规则 + 配置 + OTA */
class WebServerManager {
public:
  void begin(DeviceConfig& cfg, PrinterStatus& status, StorageManager& storage,
             BambuMQTT& mqtt, RelayAutomation& relay) {
    _cfg = &cfg;
    _status = &status;
    _storage = &storage;
    _mqtt = &mqtt;
    _relay = &relay;

    _server.on("/", [this]() { _sendHtml(INDEX_HTML); });
    _server.on("/api/status", HTTP_GET, [this]() { _handleStatus(); });
    _server.on("/api/config", HTTP_GET, [this]() { _handleConfig(); });
    _server.on("/api/wifi", HTTP_POST, [this]() { _handleWifi(); });
    _server.on("/api/printer", HTTP_POST, [this]() { _handlePrinter(); });
    _server.on("/api/reset", HTTP_POST, [this]() { _handleReset(); });
    _server.on("/api/relay", HTTP_GET, [this]() { _handleRelayGet(); });
    _server.on("/api/relay", HTTP_POST, [this]() { _handleRelayPost(); });
    _server.on("/update", HTTP_GET, [this]() { _handleOtaPage(); });
    _server.on("/upload", HTTP_POST,
               [this]() { _handleUploadDone(); },
               [this]() { _handleUploadData(); });
    _server.onNotFound([this]() { _server.send(404, "text/plain", "Not Found"); });

    _server.begin();
  }

  void loop() {
    _server.handleClient();
    if (_restartAt && millis() >= _restartAt) {
      _restartAt = 0;
      ESP.restart();
    }
  }

private:
  ESP8266WebServer _server{HTTP_PORT};
  DeviceConfig* _cfg = nullptr;
  PrinterStatus* _status = nullptr;
  StorageManager* _storage = nullptr;
  BambuMQTT* _mqtt = nullptr;
  RelayAutomation* _relay = nullptr;
  unsigned long _restartAt = 0;

  // OTA 上传校验状态
  bool _otaRejected = false;
  bool _otaStarted  = false;
  bool _otaFirst    = true;
  size_t _otaWritten = 0;
  size_t _otaMax     = 0;
  char _otaMsg[72]   = "";

  void _sendHtml(const char* html) {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Cache-Control", "no-store");
    _server.send_P(200, "text/html; charset=utf-8", html);
  }

  void _sendJson(int code, const String& body) {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(code, "application/json; charset=utf-8", body);
  }

  void _handleStatus() {
    StaticJsonDocument<1024> doc;
    doc["state"] = stateToStr(_status->state);

    doc["nozzle"] = _status->nozzle_temp;
    doc["nozzle_target"] = _status->nozzle_target;
    doc["bed"] = _status->bed_temp;
    doc["bed_target"] = _status->bed_target;
    doc["chamber"] = _status->chamber_temp;

    doc["percent"] = _status->mc_percent;
    // 打印机以「分钟」上报，前端 fmtTime 统一按秒格式化，这里换算
    doc["remaining"] = _status->mc_remaining_time * 60;
    doc["layer"] = _status->layer_num;
    doc["total_layer"] = _status->total_layer_num;
    doc["filename"] = _status->subtask_name;

    doc["spd_lvl"] = _status->spd_lvl;
    doc["spd_mag"] = _status->spd_mag;
    doc["fan_cooling"] = _status->cooling_fan_speed;
    doc["fan_big1"] = _status->big_fan1_speed;
    doc["fan_big2"] = _status->big_fan2_speed;
    doc["fan_heatbreak"] = _status->heatbreak_fan_speed;

    doc["bed_type"] = _status->bed_type;
    doc["nozzle_type"] = _status->nozzle_type;
    doc["nozzle_diameter"] = _status->nozzle_diameter;

    doc["ams_index"] = _status->ams_tray_now;
    doc["ams_type"] = _status->ams_tray_type;
    doc["ams_humidity"] = _status->ams_humidity;
    doc["ams_temp"] = _status->ams_temp;

    doc["print_error"] = _status->print_error;
    doc["hms"] = _status->hms_count;
    doc["fail_reason"] = _status->fail_reason;

    doc["wifi_signal"] = _status->wifi_signal;
    doc["mqtt_connected"] = _status->connected;

    // 设备自身信息
    doc["esp_rssi"] = WiFi.RSSI();
    doc["heap"] = (long)ESP.getFreeHeap();
    doc["uptime"] = (unsigned long)(millis() / 1000UL);

    String body;
    serializeJson(doc, body);
    _sendJson(200, body);
  }

  void _handleConfig() {
    StaticJsonDocument<768> doc;
    doc["wifi_ssid"] = _cfg->wifi_ssid;
    doc["wifi_password"] = _cfg->wifi_password;
    doc["printer_ip"] = _cfg->printer_ip;
    doc["printer_device_id"] = _cfg->printer_device_id;
    doc["printer_access_code"] = _cfg->printer_access_code;

    doc["relay_mode"] = _cfg->relay.mode;
    doc["relay_trigger"] = _cfg->relay.trigger;
    doc["relay_delay_on"] = _cfg->relay.delay_on;
    doc["relay_delay_off"] = _cfg->relay.delay_off;
    doc["relay_temp_threshold"] = _cfg->relay.temp_threshold;
    doc["relay_override_min"] = _cfg->relay.override_min;

    // 机型能力：前端据此隐藏不适用的参数区块
    doc["model"] = MODEL_NAME;
    doc["has_chamber"] = (bool)FEAT_CHAMBER_TEMP;
    doc["has_aux_fan"] = (bool)FEAT_AUX_FAN;
    doc["has_chamber_fan"] = (bool)FEAT_CHAMBER_FAN;
    doc["has_bed_type"] = (bool)FEAT_BED_TYPE;
    doc["has_nozzle_info"] = (bool)FEAT_NOZZLE_INFO;
    doc["has_ams"] = (bool)HAS_AMS;

    String body;
    serializeJson(doc, body);
    _sendJson(200, body);
  }

  void _handleWifi() {
    StaticJsonDocument<192> doc;
    if (deserializeJson(doc, _server.arg("plain"))) {
      _sendJson(400, "{\"error\":\"bad json\"}");
      return;
    }
    const char* ssid = doc["ssid"] | "";
    const char* pass = doc["pass"] | "";
    if (strlen(ssid) == 0) {
      _sendJson(400, "{\"error\":\"ssid required\"}");
      return;
    }
    strncpy(_cfg->wifi_ssid, ssid, sizeof(_cfg->wifi_ssid) - 1);
    _cfg->wifi_ssid[sizeof(_cfg->wifi_ssid) - 1] = '\0';
    strncpy(_cfg->wifi_password, pass, sizeof(_cfg->wifi_password) - 1);
    _cfg->wifi_password[sizeof(_cfg->wifi_password) - 1] = '\0';

    _storage->save(*_cfg);
    _sendJson(200, "{\"ok\":true}");
    _restartAt = millis() + 1500;
  }

  void _handlePrinter() {
    StaticJsonDocument<192> doc;
    if (deserializeJson(doc, _server.arg("plain"))) {
      _sendJson(400, "{\"error\":\"bad json\"}");
      return;
    }
    const char* ip = doc["ip"] | "";
    const char* did = doc["device_id"] | "";
    const char* ac = doc["access_code"] | "";

    strncpy(_cfg->printer_ip, ip, sizeof(_cfg->printer_ip) - 1);
    _cfg->printer_ip[sizeof(_cfg->printer_ip) - 1] = '\0';
    strncpy(_cfg->printer_device_id, did, sizeof(_cfg->printer_device_id) - 1);
    _cfg->printer_device_id[sizeof(_cfg->printer_device_id) - 1] = '\0';
    strncpy(_cfg->printer_access_code, ac, sizeof(_cfg->printer_access_code) - 1);
    _cfg->printer_access_code[sizeof(_cfg->printer_access_code) - 1] = '\0';

    _storage->save(*_cfg);
    _mqtt->disconnect();
    _mqtt->begin(*_cfg, *_status);
    _sendJson(200, "{\"ok\":true}");
  }

  void _handleReset() {
    _storage->reset();
    _sendJson(200, "{\"ok\":true}");
    _restartAt = millis() + 1500;
  }

  /* ---------------- 继电器 ---------------- */

  void _relayJson(String& body) {
    StaticJsonDocument<320> doc;
    unsigned long now = millis();
    doc["on"] = _relay->isOn();
    doc["mode"] = _cfg->relay.mode;
    doc["trigger"] = _cfg->relay.trigger;
    doc["delay_on"] = _cfg->relay.delay_on;
    doc["delay_off"] = _cfg->relay.delay_off;
    doc["temp_threshold"] = _cfg->relay.temp_threshold;
    doc["override_min"] = _cfg->relay.override_min;
    doc["override"] = _relay->overrideActive(now);
    doc["override_remain"] = _relay->overrideRemain(now);
    doc["want"] = _relay->want();
    serializeJson(doc, body);
  }

  void _handleRelayGet() {
    String body;
    _relayJson(body);
    _sendJson(200, body);
  }

  void _handleRelayPost() {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, _server.arg("plain"))) {
      _sendJson(400, "{\"error\":\"bad json\"}");
      return;
    }
    unsigned long now = millis();

    if (doc.containsKey("on")) {
      _relay->manualSet((bool)(doc["on"] | false), now);
    } else if (doc.containsKey("toggle")) {
      _relay->manualToggle(now);
    } else if (doc.containsKey("settings")) {
      JsonObject s = doc["settings"].as<JsonObject>();
      if (s.isNull()) {
        _sendJson(400, "{\"error\":\"missing settings\"}");
        return;
      }
      if (s.containsKey("mode")) {
        uint8_t m = (uint8_t)(s["mode"] | RELAY_MODE_MANUAL);
        _cfg->relay.mode = (m > RELAY_MODE_AUTO) ? RELAY_MODE_MANUAL : m;
      }
      if (s.containsKey("trigger")) {
        uint8_t t = (uint8_t)(s["trigger"] | 0);
        _cfg->relay.trigger = t & 0x0F;
      }
      if (s.containsKey("delay_on"))    _cfg->relay.delay_on = (uint16_t)(s["delay_on"] | 0);
      if (s.containsKey("delay_off"))   _cfg->relay.delay_off = (uint16_t)(s["delay_off"] | 0);
      if (s.containsKey("temp_threshold")) _cfg->relay.temp_threshold = (int16_t)(s["temp_threshold"] | 0);
      if (s.containsKey("override_min"))_cfg->relay.override_min = (uint16_t)(s["override_min"] | 0);
      if (_cfg->relay.trigger == 0) _cfg->relay.trigger = RELAY_TRIG_PRINTING;

      _storage->save(*_cfg);
      _relay->resetTimers();
    } else {
      _sendJson(400, "{\"error\":\"missing on/toggle/settings\"}");
      return;
    }

    String body;
    _relayJson(body);
    _sendJson(200, body);
  }

  /* ---------------- OTA ---------------- */

  size_t _otaSlotSize() {
    return (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
  }

  void _otaReject(const char* msg) {
    if (!_otaRejected) {
      _otaRejected = true;
      strncpy(_otaMsg, msg, sizeof(_otaMsg) - 1);
      _otaMsg[sizeof(_otaMsg) - 1] = '\0';
    }
  }
  void _otaReject(const String& msg) { _otaReject(msg.c_str()); }

  void _handleOtaPage() {
    size_t slot = _otaSlotSize();
    String page = FPSTR(OTA_PAGE);
    page.replace("%OTA_MAX%", String((unsigned long)slot));
    page.replace("%OTA_FREE%", String((unsigned long)(slot / 1024)) + " KB");
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.sendHeader("Cache-Control", "no-store");
    _server.send(200, "text/html", page);
  }

  void _handleUploadData() {
    HTTPUpload& up = _server.upload();

    if (up.status == UPLOAD_FILE_START) {
      _otaRejected = false;
      _otaStarted  = false;
      _otaFirst    = true;
      _otaWritten  = 0;
      _otaMsg[0]   = '\0';
      _otaMax      = _otaSlotSize();

      String name = up.filename;
      name.toLowerCase();
      if (!name.endsWith(".bin")) {
        _otaReject("仅支持 .bin 固件文件，已拒绝本次上传");
        return;
      }
      // 先按声明长度拦一层，避免写坏 OTA 分区
      size_t declared = (size_t)_server.header("Content-Length").toInt();
      if (declared && declared > _otaMax) {
        _otaReject("固件超出 OTA 空间，已拒绝本次上传");
        return;
      }
      if (!Update.begin(_otaMax, U_FLASH)) {
        _otaReject(Update.getErrorString());
        return;
      }
      _otaStarted = true;

    } else if (up.status == UPLOAD_FILE_WRITE) {
      if (_otaRejected || !_otaStarted) return;

      if (_otaFirst) {
        _otaFirst = false;
        if (up.currentSize < 1 || up.buf[0] != 0xE9) {
          _otaReject("不是有效的 ESP8266 固件（文件头应为 0xE9）");
          return;
        }
      }
      if (_otaWritten + up.currentSize > _otaMax) {
        _otaReject("固件超出 OTA 空间，写入已中止");
        return;
      }
      Update.write(up.buf, up.currentSize);
      _otaWritten += up.currentSize;

    } else if (up.status == UPLOAD_FILE_END) {
      if (_otaRejected || !_otaStarted) return;
      if (_otaFirst) {
        _otaReject("文件为空");
        return;
      }
      if (!Update.end(true)) {
        _otaReject(Update.getErrorString());
      }
    }
  }

  void _handleUploadDone() {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    if (_otaRejected) {
      _server.send(400, "text/plain; charset=utf-8",
                   _otaMsg[0] ? _otaMsg : "更新被拒绝");
      return;                       // 不重启，继续运行原固件
    }
    if (Update.hasError()) {
      _server.send(500, "text/plain; charset=utf-8", Update.getErrorString());
      return;
    }
    _server.send(200, "text/plain; charset=utf-8", "OK");
    _restartAt = millis() + 1500;
  }
};

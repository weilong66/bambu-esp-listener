#pragma once
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecureBearSSL.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "config.h"

/* 拓竹打印机本地 MQTT（TLS 8883）客户端：只读状态，不发送控制命令 */
class BambuMQTT {
public:
  BambuMQTT() {
    _client.setClient(_secure);
  }

  void begin(const DeviceConfig& cfg, PrinterStatus& status) {
    _cfg = &cfg;
    _status = &status;
    snprintf(_topicReport, sizeof(_topicReport), MQTT_TOPIC_REPORT, cfg.printer_device_id);
    snprintf(_topicRequest, sizeof(_topicRequest), MQTT_TOPIC_REQUEST, cfg.printer_device_id);

    _secure.setInsecure();
    _secure.setBufferSizes(2048, 2048);
    _client.setServer(cfg.printer_ip, MQTT_PORT);
    // 缓冲必须容纳完整 pushall（3~4KB），否则超长报文会被库整包丢弃
    _client.setBufferSize(MQTT_BUFFER_SIZE);
    _client.setKeepAlive(30);
    _client.setCallback(BambuMQTT::_staticCallback);

    _instance = this;
    _lastReconnect = 0;
    _lastPoll = 0;
  }

  void disconnect() {
    if (_client.connected()) _client.disconnect();
    if (_status) _status->connected = false;
  }

  void loop() {
    if (_cfg == nullptr || _status == nullptr) return;
    if (strlen(_cfg->printer_ip) == 0) return;

    if (!_client.connected()) {
      _status->connected = false;
      if (millis() - _lastReconnect >= MQTT_RECONNECT_INTERVAL) {
        _lastReconnect = millis();
        reconnect();
      }
      return;
    }

    _client.loop();

    if (millis() - _lastPoll >= STATUS_POLL_INTERVAL) {
      _lastPoll = millis();
      requestPushAll();
    }
  }

  bool connected() {
    return _client.connected();
  }

private:
  WiFiClientSecure _secure;
  PubSubClient _client;
  /* 完整 pushall 报文较大，文档放堆上（只分配一次，复用），避免占用回调栈 */
  DynamicJsonDocument _doc{JSON_DOC_SIZE};
  const DeviceConfig* _cfg = nullptr;
  PrinterStatus* _status = nullptr;
  char _topicReport[64];
  char _topicRequest[64];
  uint16_t _sequenceId = 1;
  unsigned long _lastPoll = 0;
  unsigned long _lastReconnect = 0;

  static BambuMQTT* _instance;
  static void _staticCallback(char* topic, uint8_t* payload, unsigned int length);

  void reconnect() {
    if (_client.connected()) return;
    char clientId[24];
    snprintf(clientId, sizeof(clientId), "esp-%08X", ESP.getChipId());
    if (_client.connect(clientId, MQTT_USERNAME, _cfg->printer_access_code)) {
      _status->connected = true;
      _client.subscribe(_topicReport);
      _lastPoll = millis();
      requestPushAll();
    }
  }

  void requestPushAll() {
    if (!_client.connected()) return;
    char buf[96];
    snprintf(buf, sizeof(buf),
             "{\"pushing\":{\"sequence_id\":\"%u\",\"command\":\"pushall\",\"version\":1,\"push_target\":1}}",
             ++_sequenceId);
    _client.publish(_topicRequest, buf);
  }

  static PrinterState mapState(const char* gs) {
    if (strcmp(gs, "IDLE") == 0) return STATE_IDLE;
    if (strcmp(gs, "RUNNING") == 0) return STATE_PRINTING;
    if (strcmp(gs, "PAUSE") == 0 || strcmp(gs, "PAUSED") == 0) return STATE_PAUSE;
    if (strcmp(gs, "ERROR") == 0 || strcmp(gs, "FAILED") == 0) return STATE_ERROR;
    if (strcmp(gs, "FINISH") == 0) return STATE_IDLE;
    return STATE_UNKNOWN;
  }

  /* 同一字段可能是数字也可能是字符串，统一取值 */
  static int readInt(JsonVariant v, int def) {
    if (v.is<int>()) return v.as<int>();
    if (v.is<const char*>()) return atoi(v.as<const char*>());
    if (v.is<float>()) return (int)v.as<float>();
    return def;
  }

  static float readFloat(JsonVariant v, float def) {
    if (v.is<float>() || v.is<double>()) return v.as<float>();
    if (v.is<const char*>()) return atof(v.as<const char*>());
    if (v.is<int>()) return (float)v.as<int>();
    return def;
  }

  static void copyStr(char* dst, size_t len, JsonVariant v) {
    const char* s = v | "";
    if (s && strlen(s)) {
      strncpy(dst, s, len - 1);
      dst[len - 1] = '\0';
    }
  }

  void onMessage(uint8_t* payload, unsigned int length) {
    _doc.clear();
    if (deserializeJson(_doc, payload, length)) {
      Serial.printf("[MQTT] JSON parse failed (%u bytes)\n", length);
      return;
    }

    JsonVariant pv = _doc["print"];
    if (pv.isNull()) return;
    JsonObject print = pv.as<JsonObject>();

    // ---- 状态 ----
    const char* gs = print["gcode_state"] | "";
    if (gs && strlen(gs)) _status->state = mapState(gs);

    // ---- 温度 ----
    _status->nozzle_temp    = readFloat(print["nozzle_temper"], _status->nozzle_temp);
    _status->nozzle_target  = readFloat(print["nozzle_target_temper"], _status->nozzle_target);
    _status->bed_temp       = readFloat(print["bed_temper"], _status->bed_temp);
    _status->bed_target     = readFloat(print["bed_target_temper"], _status->bed_target);
    _status->chamber_temp   = readFloat(print["chamber_temper"], _status->chamber_temp);

    // ---- 进度 ----
    _status->mc_percent       = readInt(print["mc_percent"], _status->mc_percent);
    _status->mc_remaining_time= readInt(print["mc_remaining_time"], _status->mc_remaining_time);
    _status->layer_num        = readInt(print["layer_num"], _status->layer_num);
    _status->total_layer_num  = readInt(print["total_layer_num"], _status->total_layer_num);

    // ---- 速度 / 风扇 ----
    _status->spd_lvl           = readInt(print["spd_lvl"], _status->spd_lvl);
    _status->spd_mag           = readInt(print["spd_mag"], _status->spd_mag);
    _status->cooling_fan_speed = readInt(print["cooling_fan_speed"], _status->cooling_fan_speed);
    _status->big_fan1_speed    = readInt(print["big_fan1_speed"], _status->big_fan1_speed);
    _status->big_fan2_speed    = readInt(print["big_fan2_speed"], _status->big_fan2_speed);
    _status->heatbreak_fan_speed = readInt(print["heatbreak_fan_speed"], _status->heatbreak_fan_speed);

    // ---- 硬件 ----
    copyStr(_status->bed_type, sizeof(_status->bed_type),
            print.containsKey("bed_type") ? print["bed_type"] : print["curr_bed_type"]);
    copyStr(_status->nozzle_type, sizeof(_status->nozzle_type), print["nozzle_type"]);
    _status->nozzle_diameter = readFloat(print["nozzle_diameter"], _status->nozzle_diameter);

    // ---- 错误 ----
    _status->print_error = readInt(print["print_error"], _status->print_error);
    copyStr(_status->fail_reason, sizeof(_status->fail_reason), print["fail_reason"]);
    JsonArray hms = print["hms"].as<JsonArray>();
    _status->hms_count = hms.isNull() ? 0 : (int)hms.size();

    // ---- 文件 ----
    copyStr(_status->subtask_name, sizeof(_status->subtask_name), print["subtask_name"]);

    // ---- WiFi 信号（形如 "-65dBm"）----
    JsonVariant wsig = print["wifi_signal"];
    if (!wsig.isNull()) {
      if (wsig.is<const char*>()) _status->wifi_signal = atoi(wsig.as<const char*>());
      else _status->wifi_signal = readInt(wsig, _status->wifi_signal);
    }

    // ---- AMS / 耗材 ----
    JsonVariant av = print["ams"];
    if (!av.isNull()) {
      JsonObject ams = av.as<JsonObject>();
      _status->ams_tray_now = readInt(ams["tray_now"], _status->ams_tray_now);

      JsonArray trays = ams["tray"].as<JsonArray>();
      if (!trays.isNull() && _status->ams_tray_now >= 0 &&
          _status->ams_tray_now < (int)trays.size()) {
        copyStr(_status->ams_tray_type, sizeof(_status->ams_tray_type),
                trays[_status->ams_tray_now]["tray_type"]);
      }

      JsonArray units = ams["ams"].as<JsonArray>();
      if (!units.isNull() && units.size() > 0) {
        _status->ams_humidity = readInt(units[0]["humidity"], _status->ams_humidity);
        _status->ams_temp     = readFloat(units[0]["temp"], _status->ams_temp);
      }
    }
  }
};

BambuMQTT* BambuMQTT::_instance = nullptr;

void BambuMQTT::_staticCallback(char* topic, uint8_t* payload, unsigned int length) {
  (void)topic;
  if (_instance) _instance->onMessage(payload, length);
}

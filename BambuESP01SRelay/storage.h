#pragma once
#include <Arduino.h>
#include <EEPROM.h>
#include "config.h"

// EEPROM 布局
#define EEPROM_MAGIC_ADDR        0     // 4B
#define EEPROM_VERSION_ADDR      4     // 1B
#define EEPROM_WIFI_SSID_ADDR    8     // 33B
#define EEPROM_WIFI_PASS_ADDR    41    // 65B
#define EEPROM_PRINTER_IP_ADDR   106   // 16B
#define EEPROM_DEVICE_ID_ADDR    122   // 33B
#define EEPROM_ACCESS_CODE_ADDR  155   // 17B
#define EEPROM_OTA_URL_ADDR      172   // 129B
#define EEPROM_RELAY_MODE_ADDR   304   // 1B
#define EEPROM_RELAY_TRIG_ADDR   305   // 1B
#define EEPROM_RELAY_ON_ADDR     306   // 2B
#define EEPROM_RELAY_OFF_ADDR    308   // 2B
#define EEPROM_RELAY_TEMP_ADDR   310   // 2B
#define EEPROM_RELAY_OVR_ADDR    312   // 2B
#define EEPROM_SIZE              512

class StorageManager {
public:
  void begin() {
    EEPROM.begin(EEPROM_SIZE);
  }

  DeviceConfig load() {
    uint32_t magic = 0;
    EEPROM.get(EEPROM_MAGIC_ADDR, magic);
    if (magic != CONFIG_MAGIC) return DeviceConfig();
    if (EEPROM.read(EEPROM_VERSION_ADDR) != CONFIG_VERSION) return DeviceConfig();

    DeviceConfig cfg;
    readStr(EEPROM_WIFI_SSID_ADDR,   cfg.wifi_ssid,           sizeof(cfg.wifi_ssid));
    readStr(EEPROM_WIFI_PASS_ADDR,   cfg.wifi_password,       sizeof(cfg.wifi_password));
    readStr(EEPROM_PRINTER_IP_ADDR,  cfg.printer_ip,          sizeof(cfg.printer_ip));
    readStr(EEPROM_DEVICE_ID_ADDR,   cfg.printer_device_id,   sizeof(cfg.printer_device_id));
    readStr(EEPROM_ACCESS_CODE_ADDR, cfg.printer_access_code, sizeof(cfg.printer_access_code));
    readStr(EEPROM_OTA_URL_ADDR,     cfg.ota_url,             sizeof(cfg.ota_url));

    cfg.relay.mode           = EEPROM.read(EEPROM_RELAY_MODE_ADDR);
    cfg.relay.trigger        = EEPROM.read(EEPROM_RELAY_TRIG_ADDR);
    EEPROM.get(EEPROM_RELAY_ON_ADDR,   cfg.relay.delay_on);
    EEPROM.get(EEPROM_RELAY_OFF_ADDR,  cfg.relay.delay_off);
    EEPROM.get(EEPROM_RELAY_TEMP_ADDR, cfg.relay.temp_threshold);
    EEPROM.get(EEPROM_RELAY_OVR_ADDR,  cfg.relay.override_min);

    // 容错：模式/掩码越界时回退默认
    if (cfg.relay.mode > RELAY_MODE_AUTO) cfg.relay.mode = RELAY_MODE_MANUAL;
    if (cfg.relay.trigger == 0) cfg.relay.trigger = RELAY_TRIG_PRINTING;
    return cfg;
  }

  void save(const DeviceConfig& cfg) {
    uint32_t magic = CONFIG_MAGIC;
    EEPROM.put(EEPROM_MAGIC_ADDR, magic);
    EEPROM.write(EEPROM_VERSION_ADDR, CONFIG_VERSION);

    writeStr(EEPROM_WIFI_SSID_ADDR,   cfg.wifi_ssid,           sizeof(cfg.wifi_ssid));
    writeStr(EEPROM_WIFI_PASS_ADDR,   cfg.wifi_password,       sizeof(cfg.wifi_password));
    writeStr(EEPROM_PRINTER_IP_ADDR,  cfg.printer_ip,          sizeof(cfg.printer_ip));
    writeStr(EEPROM_DEVICE_ID_ADDR,   cfg.printer_device_id,   sizeof(cfg.printer_device_id));
    writeStr(EEPROM_ACCESS_CODE_ADDR, cfg.printer_access_code, sizeof(cfg.printer_access_code));
    writeStr(EEPROM_OTA_URL_ADDR,     cfg.ota_url,             sizeof(cfg.ota_url));

    EEPROM.write(EEPROM_RELAY_MODE_ADDR, cfg.relay.mode);
    EEPROM.write(EEPROM_RELAY_TRIG_ADDR, cfg.relay.trigger);
    EEPROM.put(EEPROM_RELAY_ON_ADDR,   cfg.relay.delay_on);
    EEPROM.put(EEPROM_RELAY_OFF_ADDR,  cfg.relay.delay_off);
    EEPROM.put(EEPROM_RELAY_TEMP_ADDR, cfg.relay.temp_threshold);
    EEPROM.put(EEPROM_RELAY_OVR_ADDR,  cfg.relay.override_min);

    EEPROM.commit();
  }

  void reset() {
    DeviceConfig def;
    save(def);
  }

private:
  static void readStr(int addr, char* dst, size_t len) {
    for (size_t i = 0; i < len; i++) dst[i] = EEPROM.read(addr + i);
    dst[len - 1] = '\0';
  }

  static void writeStr(int addr, const char* src, size_t len) {
    size_t n = strlen(src);
    for (size_t i = 0; i < len; i++) EEPROM.write(addr + i, i < n ? src[i] : 0);
  }
};

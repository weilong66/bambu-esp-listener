#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

/* =================================================================
 * 1. 常量配置（改配置优先改这里）
 * ================================================================= */

// ---- 继电器 ----
#define RELAY_PIN             0      // GPIO0 控制继电器
#define RELAY_ACTIVE_LEVEL    HIGH   // 触发有效电平（实测可翻转）
#define RELAY_POWER_ON_STATE  false  // 上电默认：关闭

// ---- 持久化 ----
#define CONFIG_MAGIC    0x4C534231UL  // "BSL1"
#define CONFIG_VERSION  2             // v2：新增继电器规则配置

// ---- 时序 ----
#define WIFI_CONNECT_TIMEOUT     20000UL
#define MQTT_RECONNECT_INTERVAL  5000UL
#define STATUS_POLL_INTERVAL     60000UL
#define RELAY_UPDATE_INTERVAL     1000UL

// ---- HTTP ----
#define HTTP_PORT 80

// ---- JSON ----
#define JSON_DOC_SIZE        1280
#define JSON_DOC_SIZE_SMALL   128
#define JSON_DOC_SIZE_MEDIUM  256

/* 仅支持 ArduinoJson 6.x：文档容量编译期固定，体积最小且不会有堆扩容/碎片 */
#if ARDUINOJSON_VERSION_MAJOR >= 7
  #error "请安装 ArduinoJson 6.21.3（检测到 7.x，本项目使用 StaticJsonDocument）"
#endif

// ---- MQTT ----
#define MQTT_PORT          8883
#define MQTT_USERNAME      "bblp"
#define MQTT_TOPIC_REPORT  "device/%s/report"
#define MQTT_TOPIC_REQUEST "device/%s/request"

// ---- AP 配网 ----
#define AP_SSID     "BambuListener-Setup"
#define AP_PASSWORD "12345678"

// ---- 机型选择（编译期决定页面显示哪些参数） ----
#define PRINTER_MODEL_X1C    1
#define PRINTER_MODEL_X1E    2
#define PRINTER_MODEL_P1S    3
#define PRINTER_MODEL_P1P    4
#define PRINTER_MODEL_A1     5
#define PRINTER_MODEL_A1MINI 6

#define PRINTER_MODEL PRINTER_MODEL_A1MINI   // ★ 改成你的机型后重新编译

// AMS 是否安装（A1/A1mini 为选配 AMS lite；纯外挂料盘时置 0）
#define HAS_AMS 0

/*
 * 机型能力开关：决定页面显示哪些参数区块。
 * 依据各机型公开硬件差异设定；若与实机/改装情况不符，直接改下面的 0/1。
 *   FEAT_CHAMBER_TEMP  腔体温度
 *   FEAT_AUX_FAN       辅助风扇（big_fan1）
 *   FEAT_CHAMBER_FAN   腔体风扇（big_fan2）
 *   FEAT_BED_TYPE      热床类型（bed_type）
 *   FEAT_NOZZLE_INFO   喷嘴类型/直径（nozzle_type/nozzle_diameter）
 */
#if PRINTER_MODEL == PRINTER_MODEL_X1C || PRINTER_MODEL == PRINTER_MODEL_X1E
  #define MODEL_NAME        "X1C/X1E"
  #define FEAT_CHAMBER_TEMP 1
  #define FEAT_AUX_FAN      1
  #define FEAT_CHAMBER_FAN  1
  #define FEAT_BED_TYPE     1
  #define FEAT_NOZZLE_INFO  1
#elif PRINTER_MODEL == PRINTER_MODEL_P1S
  #define MODEL_NAME        "P1S"
  #define FEAT_CHAMBER_TEMP 0
  #define FEAT_AUX_FAN      1
  #define FEAT_CHAMBER_FAN  1
  #define FEAT_BED_TYPE     1
  #define FEAT_NOZZLE_INFO  1
#elif PRINTER_MODEL == PRINTER_MODEL_P1P
  #define MODEL_NAME        "P1P"
  #define FEAT_CHAMBER_TEMP 0
  #define FEAT_AUX_FAN      0
  #define FEAT_CHAMBER_FAN  0
  #define FEAT_BED_TYPE     1
  #define FEAT_NOZZLE_INFO  1
#elif PRINTER_MODEL == PRINTER_MODEL_A1
  #define MODEL_NAME        "A1"
  #define FEAT_CHAMBER_TEMP 0
  #define FEAT_AUX_FAN      0
  #define FEAT_CHAMBER_FAN  0
  #define FEAT_BED_TYPE     0
  #define FEAT_NOZZLE_INFO  0
#elif PRINTER_MODEL == PRINTER_MODEL_A1MINI
  #define MODEL_NAME        "A1mini"
  #define FEAT_CHAMBER_TEMP 0
  #define FEAT_AUX_FAN      0
  #define FEAT_CHAMBER_FAN  0
  #define FEAT_BED_TYPE     0
  #define FEAT_NOZZLE_INFO  0
#else
  #error "PRINTER_MODEL 未定义或不支持"
#endif

// ---- 继电器自动规则：触发位掩码 ----
#define RELAY_TRIG_PRINTING  0x01   // 打印中
#define RELAY_TRIG_PAUSED    0x02   // 暂停
#define RELAY_TRIG_ERROR     0x04   // 故障
#define RELAY_TRIG_TEMP      0x08   // 达到温度阈值

enum RelayMode : uint8_t {
  RELAY_MODE_MANUAL = 0,   // 仅手动开关
  RELAY_MODE_AUTO   = 1    // 按规则自动
};

/* 继电器规则配置（随 DeviceConfig 一起持久化） */
struct RelaySettings {
  uint8_t  mode           = RELAY_MODE_MANUAL;  // 默认手动，避免意外动作
  uint8_t  trigger        = RELAY_TRIG_PRINTING;// 触发条件掩码
  uint16_t delay_on       = 0;                  // 满足条件后延时吸合（秒）
  uint16_t delay_off      = 0;                  // 条件消失后延时断开（秒）
  int16_t  temp_threshold = 0;                  // 温度阈值 °C，0=禁用
  uint16_t override_min   = 5;                  // 手动操作接管分钟，0=不接管
};

/* =================================================================
 * 2. 数据结构
 * ================================================================= */

struct DeviceConfig {
  char wifi_ssid[33]           = "";
  char wifi_password[65]       = "";
  char printer_ip[16]          = "";
  char printer_device_id[33]   = "";
  char printer_access_code[17] = "";
  char ota_url[129]            = "";
  RelaySettings relay;
};

enum PrinterState : uint8_t {
  STATE_UNKNOWN = 0,
  STATE_IDLE,
  STATE_PRINTING,
  STATE_PAUSE,
  STATE_ERROR,
  STATE_OFFLINE
};

/* 打印机完整状态（尽量覆盖 report 中的可用字段） */
struct PrinterStatus {
  PrinterState state       = STATE_UNKNOWN;

  // 温度
  float nozzle_temp        = 0.0;
  float nozzle_target      = 0.0;
  float bed_temp           = 0.0;
  float bed_target         = 0.0;
  float chamber_temp       = 0.0;

  // 任务进度
  int   mc_percent         = 0;
  int   mc_remaining_time  = 0;   // 分钟
  int   layer_num          = 0;
  int   total_layer_num    = 0;

  // 运动 / 风扇（风扇值 0-15 档）
  int   spd_lvl            = 0;   // 1-4
  int   spd_mag            = 0;   // 倍率 %
  int   cooling_fan_speed  = 0;   // 部件冷却风扇
  int   big_fan1_speed     = 0;   // 辅助风扇 1
  int   big_fan2_speed     = 0;   // 辅助风扇 2
  int   heatbreak_fan_speed= 0;   // 热端风扇

  // 硬件信息
  char  bed_type[24]       = "";
  char  nozzle_type[20]    = "";
  float nozzle_diameter    = 0.0;

  // 耗材 / AMS
  int   ams_tray_now       = 255; // 255 = 未使用
  char  ams_tray_type[16]  = "";
  int   ams_humidity       = -1;  // %
  float ams_temp           = 0.0;

  // 错误
  int   print_error        = 0;
  int   hms_count          = 0;   // HMS 告警条数
  char  fail_reason[40]    = "";

  // 文件 / 系统
  char  subtask_name[64]   = "";
  int   wifi_signal        = 0;   // dBm
  bool  connected          = false;
};

const char* stateToStr(PrinterState s) {
  switch (s) {
    case STATE_IDLE:     return "idle";
    case STATE_PRINTING: return "printing";
    case STATE_PAUSE:    return "paused";
    case STATE_ERROR:    return "error";
    case STATE_OFFLINE:  return "offline";
    default:             return "unknown";
  }
}

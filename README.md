# Bambu-Listener 通用构建文档

## 项目说明

拓竹（Bambu Lab）3D 打印机监听器项目。任意主流单片机/开发板作为轻量网关，通过 MQTT/TLS 连接打印机本地 broker，提供 Web 状态面板、REST API、打印控制和 OTA 固件更新。

本文档是一份**自包含的 AI 构建提示词**，可以直接发送给 AI 用来从零生成完整代码。它适用于 ESP32、ESP8266、STM32（W5500/LwIP）、RP2040（W5100/PicoW）、Arduino Uno（W5100）等具备网络能力的开发板。

文档中所有与硬件相关的配置均以**参数化占位符**呈现，用户根据所选硬件填充即可。

---

### **免责声明**

1. 本文档（提示词/规格）由作者编写+AI扩写，但文中引用的协议细节、API 规范、库依赖等内容归各自原作者所有。引用来源见文末「[来源声明](#14-来源声明)」和「[技术参考](#13-技术参考)」章节。

2. 本项目与拓竹（Bambu Lab）官方无任何关联。「Bambu Lab」「拓竹」为拓竹科技的注册商标，本文仅用于描述兼容性，不构成任何品牌授权或合作关系。

3. 使用本文档生成代码时，生成的代码可能引入第三方开源许可证约束（如 OpenBambuAPI、PubSubClient、ArduinoJson、PlatformIO 相关库等）。使用者有义务自行遵守所涉许可证的条款要求。

4. 本文档按「仅供学习交流」原则提供。**建议**将生成的代码仅用于个人学习与研究用途；如需用于公开分享、商业产品或任何正式场景，请自行完成必要的合规审查与风险评估。作者不对任何使用后果承担责任。

5. 由于硬件差异、固件版本更新、第三方依赖变更等原因，本文档生成的代码不保证一定能正常运行。

---

## 1. 硬件规格与选型

### 1.1 最低硬件要求

| 项目 | 最低要求 | 推荐规格 |
|---|---|---|
| Flash | ≥ 512KB（足够存放程序 + OTA/存储分区） | ≥ 2MB（双 OTA 分区 + 文件系统） |
| RAM | ≥ 80KB（TLS 缓冲区 + ArduinoJson + MQTT 客户端） | ≥ 128KB |
| 网络 | WiFi 或有线以太网 | WiFi（802.11 b/g/n）或 W5500/LAN8720 |
| TLS | 需支持 TLS 1.2（MQTT over TLS 8883） | 硬件加速可选 |
| UART | 至少 1 路用于调试（可选） | — |

### 1.2 硬件兼容性矩阵

| 系列 | 典型型号 | 网络方式 | TLS 支持 | 存储方案 | OTA 方式 | PlatformIO 开发板名 | 备注 |
|---|---|---|---|---|---|---|---|
| **ESP32** | ESP32-WROOM-32 / S3 / C3 | WiFi | ✅ 原生 (mbedtls) | NVS / SPIFFS / LittleFS | 双 OTA 分区 | `esp32dev` / `esp32-s3-devkitc-1` / `esp32-c3-devkitm-1` | **首选**，内存充足、TLS 流畅、支持 ArduinoOTA |
| **ESP8266** | ESP-01S / NodeMCU / Wemos D1 | WiFi | ✅ (BearSSL) | EEPROM 模拟 / SPIFFS / LittleFS | 双 OTA 分区（需 ≥ 1MB） | `esp01` / `nodemcuv2` / `d1` | ESP-01S (1MB) 内存紧张，需严格优化；NodeMCU (4MB) 较舒适 |
| **STM32 + W5500** | STM32F103 / F407 / G4 + W5500 模块 | 有线以太网 | ⚠️ 需移植 BearSSL/mbedtls | EEPROM / Flash | 需自定义 | 自定义 board / `disco_f407vg` | 以太网稳定但布线成本高；TLS 是挑战 |
| **RP2040 (PicoW)** | Raspberry Pi Pico W 2040 | WiFi (CYW43) | ✅ (PicoSDK mbedtls) | Flash 模拟 | 双 OTA 分区（需 ≥ 2MB） | `pico_w` | 成本低、生态快速成熟，支持 Arduino Framework |
| **RP2040 + W5100** | Raspberry Pi Pico + W5100 模块 | 有线以太网 | ⚠️ 需移植 | Flash 模拟 | 自定义 | `pico` | — |
| **Arduino Uno/Nano + W5100** | ATmega328P + W5100 | 有线以太网 | ❌ RAM 不足以跑 TLS | EEPROM | 无 | `uno` | **不推荐**，TLS 无法运行；仅适用于 HTTP（非加密）原型 |

> **选型建议**：绝大多数场景选 **ESP32-S3**（或基础款 ESP32-WROOM-32），生态最成熟、Arduino/ESP-IDF 双框架、TLS 流畅、OTA 稳健。若体积/成本敏感可选 **ESP-01S** 但必须做内存优化（见 [§2.4](#24-内存优化-按平台分)）。

---

## 2. 开发环境与依赖

### 2.1 构建系统选择

| 构建系统 | 适用场景 | 推荐度 |
|---|---|---|
| **PlatformIO** | 跨平台、依赖自动安装、统一 `.ini` 配置 | ⭐⭐⭐⭐⭐ **首选** |
| **Arduino IDE** | 入门级、图形化 | ⭐⭐⭐ |
| **ESP-IDF / Zephyr / Mbed** | 追求极致性能但需改写成 C | ⭐⭐（本文档基于 Arduino Framework） |

### 2.2 PlatformIO 通用配置模板

```ini
[env:{BOARD_NAME}]
platform = {PLATFORM_PACKAGE}
board = {BOARD_ID}
framework = arduino

; --- 频率/Flash 参数（按需调整） ---
board_build.f_cpu = {CPU_FREQ}L
board_build.f_flash = {FLASH_FREQ}L
board_build.flash_mode = {FLASH_MODE}
; 双 OTA 分区脚本（根据 Flash 大小选择）
;   1MB flash:  eagle.flash.1m.ota.ld
;   4MB flash:  eagle.flash.4m.ota.ld  (ESP8266)
;   ESP32:      默认分区表或 custom.csv
board_build.ldscript = {LINKER_SCRIPT}

; --- 编译优化（统一适用） ---
build_flags = 
    -Os
    -ffunction-sections
    -fdata-sections
    -Wl,--gc-sections
    -DARDUINOJSON_ENABLE_PROGMEM=1
    -DARDUINOJSON_USE_LONG_LONG=0

; --- 核心库依赖（硬件无关） ---
lib_deps = 
    bblanchon/ArduinoJson@^6.21.3    ; JSON 解析 (必须 v6, v7 API 不同)
    knolleary/PubSubClient@^2.8       ; MQTT 客户端

; --- 上传/监视参数 ---
upload_speed = 115200
monitor_speed = 115200
```

**占位符填充指南：**

| 占位符 | ESP32 示例 | ESP8266 示例 | RP2040 (PicoW) 示例 |
|---|---|---|---|
| `{PLATFORM_PACKAGE}` | `espressif32@^6.0.0` | `espressif8266@^4.0.0` | `wizio-pico` |
| `{BOARD_ID}` | `esp32-s3-devkitc-1` | `esp01` / `nodemcuv2` | `pico_w` |
| `{CPU_FREQ}` | `240000000` | `80000000` | `133000000` |
| `{FLASH_FREQ}` | `80000000` | `40000000` | `40000000` |
| `{FLASH_MODE}` | `qio` | `dout` | `qspi` |
| `{LINKER_SCRIPT}` | 默认分区表 | `eagle.flash.1m.ota.ld` | 默认 |

### 2.3 按平台的框架内置库

> 这些库由 Arduino Framework 自动提供，**无需**写入 `lib_deps`。

| 功能 | ESP32 | ESP8266 | RP2040 (PicoW) | STM32 (W5500) |
|---|---|---|---|---|
| WiFi | `WiFi.h` | `ESP8266WiFi.h` | `WiFi.h` (CYW43) | — |
| 有线以太网 | — | — | — | `Ethernet.h` (W5500) |
| TLS | `WiFiClientSecure.h` | `WiFiClientSecure.h` (BearSSL) | `WiFiClientSecure.h` (mbedtls) | 需移植 BearSSL |
| HTTP 服务器 | `WebServer.h` | `ESP8266WebServer.h` | `WebServer.h` | `WebServer.h` (LwIP) |
| OTA | `Update.h` + `ArduinoOTA.h` | `Update.h` + `ArduinoOTA.h` | `Update.h` | 自定义 |
| 配置持久化 | `Preferences.h` (NVS) | `EEPROM.h`（模拟） / `LittleFS.h` | 自定义 Flash / `LittleFS.h` | `EEPROM.h` / Flash |
| 文件系统 | `SPIFFS.h` / `LittleFS.h` | `SPIFFS.h` / `LittleFS.h` | `LittleFS.h` | — |
| 日志 | `Serial` + `ESP_LOGx` | `Serial` | `Serial` | `Serial` |

### 2.4 内存优化（按平台分）

内存紧张的平台（ESP-01S 1MB、STM32F103）需主动优化。**内存充足平台（ESP32 系列）可以跳过此节**。

| 优化项 | ESP-01S (1MB) 推荐值 | ESP8266 (4MB) 推荐值 | ESP32 推荐值 | 说明 |
|---|---|---|---|---|
| TLS 缓冲区 | 2048 / 2048 | 4096 / 4096 | 4096+ | `WiFiClientSecure` 的 `setBufferSizes()` |
| TLS 证书验证 | `setInsecure()` | `setInsecure()` 或 CA | CA（推荐） | 跳过验证省内存但牺牲安全性；局域网场景可接受 |
| PubSubClient 缓冲区 | 1024 | 1024~2048 | 1024~2048 | `setBufferSize()` |
| ArduinoJson 文档 | 1024 / 128~256 | 1024~2048 / 256 | 1024~2048 | MQTT 解析用较大值，HTTP 处理用较小值 |
| HTML/CSS/JS | 全部 `PROGMEM` | 推荐 `PROGMEM` | 可选 | PROGMEM 将常量移入 Flash 省 RAM |
| 字符串 | `char[]` + `snprintf` 优先 | 同左 | 同左 | 避免 `String` 动态分配导致碎片 |
| OTA | HTTP 上传代替 ArduinoOTA | 两者均可 | ArduinoOTA（推荐） | ArduinoOTA 占额外 ~1.5KB RAM |

---

## 3. 项目文件结构

### 3.1 通用结构

```
{ProjectName}/
├── platformio.ini                   # PlatformIO 构建配置（多 env 可多板支持）
├── src/                             # PlatformIO 推荐 src 目录；Arduino IDE 可放根目录
│   ├── main.cpp                     # 主程序入口（.ino 也可，Arduino Framework 自动识别）
│   ├── config.h                     # 全局常量、结构体、枚举
│   ├── storage.h                    # 配置持久化（EEPROM / NVS / Flash）
│   ├── mqtt_client.h                # MQTT TLS 连接 + 消息解析 + 打印命令
│   ├── web_server.h                 # HTTP 服务器、REST API、OTA 上传
│   ├── html_page.h                  # Web UI (PROGMEM 存储)
│   └── platform_adapter.h           # [可选] 按平台适配的差异代码（网络/存储/TLS 抽象）
└── doc/                             # 参考文档
    ├── mqtt.md                      # Bambu API 协议文档（OpenBambuAPI 本地缓存）
    ├── tls.md                       # TLS 证书相关说明
    └── video.md                     # 参考视频链接
```

> **说明**：所有 `.h` 文件同时包含类声明和 `inline` 实现，无需额外 `.cpp` 文件。`platform_adapter.h` 用于隔离不同开发板在网络库、存储库上的差异，让核心逻辑跨平台复用。

### 3.2 平台适配层设计（可选但推荐）

若需同时支持多种硬件，创建 `platform_adapter.h` 用 `#ifdef` 做条件编译：

```cpp
#pragma once

// --- 网络抽象 ---
#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040)
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #define HAS_WIFI true
  #define HAS_ETHERNET false
#elif defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_AVR)
  #include <Ethernet.h>       // W5500
  #include <EthernetClient.h>
  #define HAS_WIFI false
  #define HAS_ETHERNET true
#endif

// --- 存储抽象 ---
#if defined(ESP32)
  #include <Preferences.h>    // NVS，推荐
  #define StorageImpl Preferences
#elif defined(ESP8266)
  #include <EEPROM.h>         // EEPROM 模拟
  #define StorageImpl EEPROM
#else
  #include <EEPROM.h>
  #define StorageImpl EEPROM
#endif

// --- TLS 客户端别名 ---
#if defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040)
  typedef WiFiClientSecure SecureTLSClient;
#else
  // 有线以太网通常不加密，或需额外移植 TLS
  typedef EthernetClient SecureTLSClient;
#endif
```

---

## 4. 架构与模块关系

```
  ┌──────────┐    ┌─────────────┐    ┌──────────────┐
  │ 配置存储  │◄──►│  网络管理    │◄──►│  WebServer    │
  │(storage) │    │ WiFi / Eth  │    │ (HTTP + API)  │
  └──────────┘    └─────────────┘    └──────┬───────┘
                                             │
                                      ┌──────┴──────┐
                                      │  HTML Page  │
                                      │ (PROGMEM)   │
                                      └─────────────┘
                                              ▲
                                              │ 状态监听
                                      ┌───────┴───────┐
                                      │  BambuMQTT    │
                                      │  TLS : 8883   │
                                      └───────┬───────┘
                                              │ 订阅 + 发布
                                     ┌────────┴────────┐
                                     │ Bambu Lab 打印机 │
                                     │ 本地 MQTT Broker │
                                     └─────────────────┘
```

### 网络管理模式

- **WiFi STA 模式**（WiFi 硬件）：连接已有路由器；首选。
- **WiFi AP 模式**（WiFi 硬件）：未配置或连接失败时启动 AP，用于首次配置；AP SSID/密码见常量。
- **有线以太网**（W5500 等）：无 AP 模式，改为静态 IP 或 DHCP 获取。

### 主循环执行顺序

```
网络检查 → AP/DHCP 检测 → WebServer → [ArduinoOTA] → MQTT loop → HTTP 请求处理
```

---

## 5. 配置结构体与枚举

> **重要**：本节所有结构体和枚举**与硬件无关**，任何开发板实现时必须保持字段名称和类型完全一致，因为 REST API 和 Web UI 直接依赖这些字段。

### 5.1 `DeviceConfig` (`config.h`)

```c
struct DeviceConfig {
  char wifi_ssid[33] = "";           // WiFi SSID (WiFi 硬件) / DHCP hostname (以太网)
  char wifi_password[65] = "";       // WiFi 密码 (WiFi 硬件) / 忽略 (以太网)
  char printer_ip[16] = "";          // 打印机局域网 IP
  char printer_device_id[33] = "";   // 打印机序列号 (DEVICE_ID)
  char printer_access_code[17] = ""; // LAN 访问码（MQTT 密码）
  char ota_url[129] = "";            // HTTP OTA 更新 URL（可选）
};
```

> **字段长度推导**：SSID 最大 32 字符 + `\0` = 33；WPA2 密码最大 63 + `\0` = 65；IPv4 点分十进制最大 15 + `\0` = 16；Device ID 典型长度；16 位 Access Code + `\0` = 17；URL 通用上限 128 + `\0` = 129。

### 5.2 `PrinterState` 枚举

```c
enum PrinterState {
  STATE_UNKNOWN = 0,
  STATE_IDLE,
  STATE_PRINTING,
  STATE_PAUSE,
  STATE_ERROR,
  STATE_OFFLINE   // 预留用于未来断开检测，当前版本从未由代码赋值
};
```

### 5.3 `PrinterStatus`

```c
struct PrinterStatus {
  PrinterState state = STATE_UNKNOWN;
  float nozzle_temp = 0.0;
  float nozzle_target = 0.0;
  float bed_temp = 0.0;
  float bed_target = 0.0;
  int mc_percent = 0;          // 打印百分比
  int mc_remaining_time = 0;   // 剩余时间（秒）
  char subtask_name[64] = "";  // 当前打印文件名
  bool connected = false;      // MQTT 连接状态
};
```

### 5.4 关键常量 (`#define`)

| 常量 | 默认值 | 说明 |
|---|---|---|
| `WIFI_CONNECT_TIMEOUT` | `20000UL` | WiFi 连接超时 (ms) |
| `MQTT_RECONNECT_INTERVAL` | `5000UL` | MQTT 重连间隔 (ms) |
| `STATUS_POLL_INTERVAL` | `60000UL` | pushall 轮询间隔 (ms) |
| `HTTP_PORT` | `80` | Web 服务器端口 |
| `JSON_DOC_SIZE` | `1024` | MQTT 消息 JSON 文档大小（内存紧张平台可调小至 768） |
| `JSON_DOC_SIZE_SMALL` | `128` | 小型 HTTP 请求 |
| `JSON_DOC_SIZE_MEDIUM` | `256` | 中型 HTTP 响应 |
| `MQTT_TOPIC_REPORT` | `"device/%s/report"` | 打印机状态推送主题 |
| `MQTT_TOPIC_REQUEST` | `"device/%s/request"` | 命令发送主题 |
| `AP_SSID` | `"BambuListener-Setup"` | AP 模式 SSID |
| `AP_PASSWORD` | `"12345678"` | AP 模式密码（≥ 8 字符） |
| `OTA_ENABLE` | `false` | ArduinoOTA（内存紧张平台禁用；ESP32 可启用） |
| `HTTP_OTA_ENABLE` | `true` | HTTP 上传固件（所有平台通用） |

---

## 6. MQTT 协议实现

> **来源**：本节全部协议细节（连接参数、主题格式、pushall 请求、命令 JSON、`gcode_state` 映射、字段提取表）参考自 [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)，本地缓存见 `doc/mqtt.md`。

**本节内容与硬件完全无关**，所有开发板实现时必须严格遵循。

### 6.1 连接参数

| 项目 | 值 |
|---|---|
| 协议 | TLS，端口 8883 |
| 用户名 | `bblp`（本地 broker 固定值） |
| 密码 | 打印机 LAN 访问码（`printer_access_code`） |
| 客户端 ID | `esp-{MAC后4字节}`（示例；实际前缀可自定义，但必须唯一） |
| TLS 配置 | `setInsecure()` 跳过证书验证（局域网场景安全可接受），缓冲区大小见 [§2.4](#24-内存优化-按平台分) |

### 6.2 主题

- 订阅：`device/{DEVICE_ID}/report` — 打印机主动推送状态
- 发布：`device/{DEVICE_ID}/request` — 发送命令

### 6.3 主动请求完整状态 (`pushing.pushall`)

P1/A1 系列只推送增量更新，需要定期请求完整状态：

```json
{
  "pushing": {
    "sequence_id": "1",
    "command": "pushall",
    "version": 1,
    "push_target": 1
  }
}
```

**注意**：P1P 上建议 ≥5 分钟间隔，过于频繁可能卡顿。本项目使用 60 秒（常量 `STATUS_POLL_INTERVAL`），可根据实际情况调整。

### 6.4 命令发送（暂停/继续/停止）

```json
// 暂停
{ "print": { "sequence_id": "2", "command": "pause", "param": "" } }
// 继续
{ "print": { "sequence_id": "3", "command": "resume", "param": "" } }
// 停止
{ "print": { "sequence_id": "4", "command": "stop", "param": "" } }
```

### 6.5 `gcode_state` → `PrinterState` 映射

| `gcode_state` 值 | 映射结果 |
|---|---|
| `"IDLE"` | `STATE_IDLE` |
| `"RUNNING"` | `STATE_PRINTING` |
| `"PAUSE"` / `"PAUSED"` | `STATE_PAUSE` |
| `"ERROR"` | `STATE_ERROR` |
| `"FINISH"` | `STATE_IDLE`（打印完成回到空闲） |
| 其他 | `STATE_UNKNOWN` |

### 6.6 MQTT 消息中解析的关键字段

从 `push_status` / `pushall` 报告的 `"print"` 对象中提取：

| JSON 字段 | 类型 | 映射到 |
|---|---|---|
| `gcode_state` | string | `PrinterStatus.state` |
| `nozzle_temper` | float | `nozzle_temp` |
| `nozzle_target_temper` | float | `nozzle_target` |
| `bed_temper` | float | `bed_temp` |
| `bed_target_temper` | float | `bed_target` |
| `mc_percent` | int | `mc_percent` |
| `mc_remaining_time` | int | `mc_remaining_time` |
| `subtask_name` | string | `subtask_name` |

### 6.7 sequence_id 递增

每次发送命令前 `_sequenceId++`（uint16_t 类型，自动回绕）。

---

## 7. REST API 参考

所有端点返回 JSON，统一启用 CORS (`Access-Control-Allow-Origin: *`)。本节与硬件无关。

### 7.1 状态查询

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/api/status` | 打印机状态 + MQTT 连接状态 |
| GET | `/api/config` | WiFi/打印机配置（密码字段明文返回） |

**`/api/status` 响应示例：**
```json
{
  "state": "printing",
  "nozzle": 220.0,
  "nozzle_target": 220.0,
  "bed": 60.0,
  "bed_target": 60.0,
  "percent": 45,
  "remaining": 1800,
  "filename": "Benchy.gcode",
  "mqtt_connected": true
}
```

### 7.2 打印控制

| 方法 | 路径 | 请求体 | 说明 |
|---|---|---|---|
| POST | `/api/cmd` | `{"cmd": "pause"}` | 发送 pause / resume / stop 命令 |

### 7.3 配置

| 方法 | 路径 | 请求体 | 说明 |
|---|---|---|---|
| POST | `/api/wifi` | `{"ssid":"XXX","pass":"YYY"}` | 保存 WiFi 后自动重启（WiFi 硬件） / 忽略（以太网） |
| POST | `/api/printer` | `{"ip":"...","device_id":"...","access_code":"..."}` | 保存打印机连接参数 |
| POST | `/api/reset` | 无 | 恢复默认设置并重启 |

### 7.4 OTA 固件更新

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/update` | OTA 上传页面（HTML 表单 + XHR 进度） |
| POST | `/upload` | multipart/form-data 接收 `.bin` 文件，使用平台对应的 `Update` 库写入 |

> **实现说明**：OTA 上传通过 `WebServer::on()` 注册路由特定的 upload handler（4 参数版本），直接调用 `Update.begin()` / `Update.write()` / `Update.end()` 完成固件写入。不走 `HTTPUpdateServer`（该库需要 mDNS，内存紧张平台占用太大）。

> **来源**：HTTP OTA 上传模式（路由特定 upload handler + 直写 Update 库）参考自 [kldzj/esphome-chamber-sentinel](https://github.com/kldzj/esphome-chamber-sentinel)。

---

## 8. 配置持久化存储

### 8.1 支持方案

| 存储方案 | 适用平台 | 库 | 特点 |
|---|---|---|---|
| **NVS (Preferences)** | ESP32 系列 | `Preferences.h` | 分区存储、磨损均衡、推荐 |
| **EEPROM 模拟** | ESP8266 / STM32 / AVR | `EEPROM.h` | 固定大小、简单、ESP8266 模拟 4KB |
| **Flash 自定义** | RP2040 / 其他裸 Flash | 需自己实现 | 复杂、不推荐除非无其他选项 |

### 8.2 魔数与版本（所有方案统一）

```c
#define CONFIG_MAGIC    0x4C534231UL  // "BSL1" = Bambu Speaker Listener v1
#define CONFIG_VERSION  1
```

### 8.3 字段偏移表（EEPROM / Flash 方案）

| 名称 | 偏移 | 大小 | 类型 | 说明 |
|---|---|---|---|---|
| `EEPROM_MAGIC` | 0 | 4 | `uint32_t` | 首次启动检测 |
| `EEPROM_VERSION` | 4 | 1 | `uint8_t` | 预留版本迁移 |
| （保留） | 5-7 | 3 | — | 对齐 |
| `EEPROM_WIFI_SSID` | 8 | 33 | `char[33]` | `wifi_ssid` |
| `EEPROM_WIFI_PASS` | 41 | 65 | `char[65]` | `wifi_password` |
| `EEPROM_PRINTER_IP` | 106 | 16 | `char[16]` | `printer_ip` |
| `EEPROM_DEVICE_ID` | 122 | 33 | `char[33]` | `printer_device_id` |
| `EEPROM_ACCESS_CODE` | 155 | 17 | `char[17]` | `printer_access_code` |
| `EEPROM_OTA_URL` | 172 | 129 | `char[129]` | `ota_url` |
| `EEPROM_CONFIG_END` | 301 | — | — | 配置区结束位置 |

### 8.4 NVS (ESP32) 方案键名

```
config/magic
config/version
config/wifi_ssid
config/wifi_password
config/printer_ip
config/printer_device_id
config/printer_access_code
config/ota_url
```

### 8.5 读写逻辑

- 读取时检查魔数，不匹配则返回 `DeviceConfig()` 默认值
- 所有写入后立即 commit（EEPROM 方案 `EEPROM.commit()`；NVS 方案 `Preferences.end()`）
- **注意**：EEPROM 写入寿命有限（~10万次），setter 方法立即 commit，调用方控制频率

---

## 9. 网络管理

### 9.1 WiFi 方案

- **未配置 SSID** 或**连续 3 次连接失败** → 启动 AP 模式
- AP 模式下只运行 Web 服务器（用于 WiFi 配置），不启动 MQTT
- STA 模式下连接丢失每 10 秒尝试重连
- **AP SSID**：常量 `AP_SSID`（默认 `BambuListener-Setup`）
- **AP 密码**：常量 `AP_PASSWORD`（默认 `12345678`，至少 8 字符）

### 9.2 有线以太网方案

- 使用 DHCP 或静态 IP
- 无 AP 模式，首次配置通过串口或默认 IP 访问 Web 服务器
- W5500 MAC 地址需在代码中硬编码或从平台 API 获取

### 9.3 TLS 实现要点

| 平台 | TLS 库 | 跳过证书验证 | 证书验证 |
|---|---|---|---|
| ESP32 | mbedtls (WiFiClientSecure) | `setInsecure()` | `setCACert()` + `setCertificate()` |
| ESP8266 | BearSSL (WiFiClientSecure) | `setInsecure()` | `setTrustAnchor()` |
| RP2040 (PicoW) | mbedtls | `setInsecure()` | 同 ESP32 |
| STM32 + W5500 | 需移植 BearSSL/mbedtls | 移植 `setInsecure()` 等效 | 复杂 |
| AVR + W5100 | **无法运行 TLS** | — | — |

> **局域网场景**：打印机 MQTT broker 使用自签名证书，`setInsecure()` 是最简单的方案。证书验证方案在生产环境推荐，但需要额外存储打印机 CA 证书。

---

## 10. Web UI 规格 (`html_page.h`)

单页应用，所有 HTML/CSS/JS 存入 `PROGMEM`。深色主题，CSS 内联，JS 使用原生 `fetch` API 轮询 `/api/status`（3 秒间隔）。

> **规格说明**：本章节描述的是**功能规格**，AI 生成时应据此实现**功能等价**的 UI。HTML/CSS/JS 可以自由发挥，只要满足下述模块布局和交互逻辑即可。**本节与硬件无关**。

### 页面模块

1. **打印机状态卡片** — 连接状态 Badge、状态 Badge（空闲/打印中/已暂停/错误）、喷嘴温度（当前/目标，°C）、热床温度（当前/目标，°C）、打印进度百分比、剩余时间（自动格式化 h+m 或 m）、当前打印文件名
2. **打印控制卡片** — 暂停 / 继续 / 停止按钮（均有 confirm 弹窗）
3. **网络设置卡片** — WiFi SSID 输入 + 密码输入，保存后设备自动重启（WiFi 硬件）；以太网硬件可隐藏此卡片
4. **打印机设置卡片** — IP 地址 / Device ID（序列号） / LAN 访问码，保存后自动重连 MQTT

### 额外页面

- `/update` — OTA 固件上传页面（独立 HTML 表单 + XHR 上传进度条 + 成功/失败提示）

---

## 11. 主程序流程 (`main.cpp` / `.ino`)

> **来源**：主程序初始化顺序和主循环逻辑为 AI 生成。

### 11.1 全局实例

```c
StorageManager storage;       // 具体类型根据平台选择（Preferences / EEPROM）
DeviceConfig config;
PrinterStatus printerStatus;
BambuMQTT bambuMQTT;
WebServer httpServer;
// 仅 WiFi 硬件需要：
WiFiManager wifiManager;     // 或自行实现连接/AP 切换
```

### 11.2 `setup()` 顺序

```
Serial.begin(115200)          // 调试串口，可选
storage.begin()               // 初始化存储，加载 config（检查魔数）
connectNetwork()              // WiFi STA 连接 或 DHCP（以太网）
                                 WiFi 失败 → 启动 AP 模式
bambuMQTT.begin(config, printerStatus)  // 构建 TLS 客户端、设置主题、注册回调
httpServer.setConfigRefs(...)         // 注入 config / storage / bambuMQTT 引用
httpServer.begin()                    // 注册所有 HTTP 路由
#if OTA_ENABLE
  ArduinoOTA.begin()
#endif
```

### 11.3 `loop()` 顺序

```
checkNetwork()                // WiFi: 重连检查；以太网: link 状态
if (apModeActive) {
    httpServer.loop();
    storage.loop();           // 可选，某些存储方案需要轮询
    return;
}
#if OTA_ENABLE
  ArduinoOTA.handle()
#endif
bambuMQTT.loop()              // 自动重连 + pushall 轮询
httpServer.loop()             // 所有 HTTP 请求（含 /api/* 和 /update）
storage.loop()                // 可选
```

---

## 12. 按平台快速入门

### 12.1 ESP32-S3（推荐首选）

- **PlatformIO 环境**：`[env:esp32-s3-devkitc-1]`
- **Flash**：默认 4MB OTA 分区表，无需自定义 linker script
- **存储**：`Preferences.h` (NVS)
- **OTA**：可同时启用 `ArduinoOTA` 和 HTTP OTA
- **内存**：充足，所有默认值直接用
- **TLS**：原生 mbedtls，推荐使用 CA 证书验证

### 12.2 ESP-01S（内存紧张）

- **PlatformIO 环境**：`[env:esp01]`
- **Flash**：1MB，必须使用双 OTA 分区 `eagle.flash.1m.ota.ld`
- **存储**：`EEPROM.h`（模拟 4KB）
- **OTA**：**仅启用 HTTP OTA**（`OTA_ENABLE = false`），ArduinoOTA 会占太多 RAM
- **内存**：TLS 缓冲区 2048、PubSubClient 1024、ArduinoJson 1024，**所有 HTML/CSS/JS 必须 PROGMEM**，避免 `String` 动态分配
- **TLS**：BearSSL，`setInsecure()`

### 12.3 NodeMCU / Wemos D1（ESP8266 4MB）

- **PlatformIO 环境**：`[env:nodemcuv2]` 或 `[env:d1]`
- **Flash**：4MB，双 OTA 分区 `eagle.flash.4m.ota.ld`（需自行查找或生成）
- **存储**：`EEPROM.h` 或 `LittleFS.h`
- **OTA**：两者均可
- **内存**：较舒适，TLS 缓冲区可设 4096

### 12.4 Raspberry Pi Pico W（RP2040）

- **PlatformIO 环境**：`[env:pico_w]`（使用 `wizio-pico` 平台包）
- **Flash**：2MB+，RP2040 原生不支持 OTA 分区表，需自定义或使用 HTTP OTA
- **存储**：自定义 Flash 区域或 `LittleFS.h`
- **TLS**：mbedtls via CYW43，使用 `setInsecure()`
- **WiFi API**：兼容 Arduino WiFi 库，但部分高级功能可能缺失

### 12.5 STM32 + W5500（有线以太网）

- **PlatformIO 环境**：自定义 board，`framework = arduino`
- **Flash**：至少 256KB
- **存储**：`EEPROM.h` 或内部 Flash
- **TLS**：**需移植 BearSSL/mbedtls**，是主要挑战；若不加密则直接用 `EthernetClient`
- **W5500 库**：`Ethernet.h` (SKSZ) 或 `UIPEthernet.h`（旧版兼容）

---

## 13. 技术参考

- Bambu 打印机 API 协议文档：[Doridian/OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)（本地缓存见 `doc/mqtt.md`）
- ESP8266 HTTP OTA 实现参考：[kldzj/esphome-chamber-sentinel](https://github.com/kldzj/esphome-chamber-sentinel)
- WiFi AP 配置模式 + Web Dashboard 风格参考：[VR6syncro/Bambulab_Esp32_LEDController](https://github.com/vr6syncro/Bambulab_Esp32_LEDController)

---

## 14. 来源声明

| 模块 / 章节 | 来源 | 说明 |
|---|---|---|
| **MQTT 协议细节**（连接参数、主题、pushall 请求、命令 JSON、gcode_state 映射、字段提取） | OpenBambuAPI / `doc/mqtt.md` | 协议规范源自官方 API 文档 |
| **硬件兼容性矩阵**（§1.2） | AI 生成 | 综合各平台 Flash/RAM/TLS 能力整理 |
| **PlatformIO 通用配置模板**（§2.2） | AI 生成 | 参数化占位符设计 |
| **内存优化分平台表**（§2.4） | AI 生成 | 综合 ESP8266/ESP32/RP2040 低内存最佳实践 |
| **platform_adapter.h 适配层**（§3.2） | AI 生成 | 跨平台条件编译模式 |
| **`config.h` 结构体与常量** | AI 生成 | 各字段长度由 SSID/密码/IP 等实际约束推导 |
| **`storage.h` 存储方案** | AI 生成 | EEPROM 固定地址布局 + NVS 键名方案 |
| **`mqtt_client.h` 类设计** | AI 生成 | PubSubClient + WiFiClientSecure 封装 |
| **`web_server.h` REST API 端点设计** | AI 生成 | 路由路径、请求体格式、响应格式 |
| **`html_page.h` Web UI 实现** | AI 生成 | PROGMEM 存储、fetch 轮询、3 秒刷新 |
| **主程序流程（setup/loop）** | AI 生成 | 初始化顺序、AP 模式条件分支 |
| **WiFi/AP 配置模式** | 参考 [VR6syncro/Bambulab_Esp32_LEDController](https://github.com/vr6syncro/Bambulab_Esp32_LEDController) | AP 模式 + Web 配置页面思路参考 |
| **HTTP OTA 上传模式** | 参考 [kldzj/esphome-chamber-sentinel](https://github.com/kldzj/esphome-chamber-sentinel) | 路由特定 upload handler 参考 |

---

## 15. License

你可以自由复制、修改、传播本文档，但须**署名原作者**、**不得用于商业目的**、**衍生物以相同条件发布**。

### 简化条款

> 本节为**非官方的简化摘要**，便于快速理解。具有法律效力的完整许可证文本见项目根目录的 [`LICENSE.md`](./LICENSE.md)。

| 权利 | 说明 |
|---|---|
| ✅ 自由复制 | 可以在任何媒介上复制本文档全文或部分内容 |
| ✅ 自由修改 | 可以改编、增删内容，产生衍生版本 |
| ✅ 自由传播 | 可以分发、公开展示、以个人/集体方式演示 |
| ⚠️ 须署名 | 复制、修改、传播时必须标注原作者 |
| ⚠️ 同许可 | 衍生版本必须以相同的 GNU FDL 1.3 发布 |
| ⚠️ 保留声明 | 衍生版本需保留原有的免责声明、许可证引用、历史记录 |
| ❌ 禁商用 | 未经技术来源原作者许可，禁止将本文档或其衍生物用于商业目的 |
| ❌ 无担保 | 本文档按"现状"提供，不担保任何适用性或正确性 |

### 正式许可证

本文档以 **GNU Free Documentation License v1.3** 发布。完整法律文本见项目根目录 [`LICENSE.md`](./LICENSE.md)，或访问 <https://www.gnu.org/licenses/fdl-1.3.txt>。

### 第三方内容声明

本文档中引用的以下内容归各自原作者所有，其使用须遵守对应的独立许可证：

- **Bambu Lab MQTT 协议规范**（第 6 节、`doc/mqtt.md`）：源自 [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)，其许可证可能与本文档不同，请自行查阅该项目的许可证文件。
- **PubSubClient / ArduinoJson / PlatformIO 框架**：各自保有独立许可证（LGPL-2.1 / MIT / Apache-2.0）。

> 基于本文档生成的代码，其开源许可证由所引入的第三方依赖共同决定，使用者须逐一审查并遵守各依赖的许可证条款。

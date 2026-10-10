# Bambu-ESP01S-Listener

## 项目说明

拓竹（Bambu Lab）3D 打印机监听器项目。ESP-01S (ESP8266, 1MB Flash) 作为轻量网关，通过 MQTT/TLS 连接打印机本地 broker，提供 Web 状态面板、REST API、打印控制和 OTA 固件更新。

本文档是一份**自包含的构建提示词**，可以直接发送给 AI 用来从零生成完整代码。



### **免责声明**

1. 本文档（提示词/规格）由作者编写+AI扩写，但文中引用的协议细节、API 规范、库依赖等内容归各自原作者所有。引用来源见文末「[来源声明](#13-来源声明)」和「[技术参考](#12-技术参考)」章节。

2. 本项目与拓竹（Bambu Lab）官方无任何关联。「Bambu Lab」「拓竹」为拓竹科技的注册商标，本文仅用于描述兼容性，不构成任何品牌授权或合作关系。

3. 使用本文档生成代码时，生成的代码可能引入第三方开源许可证约束（如 OpenBambuAPI、PubSubClient、ArduinoJson、PlatformIO 相关库等）。使用者有义务自行遵守所涉许可证的条款要求。

4. 本文档按「仅供学习交流」原则提供。**建议**将生成的代码仅用于个人学习与研究用途；如需用于公开分享、商业产品或任何正式场景，请自行完成必要的合规审查与风险评估。作者不对任何使用后果承担责任。

5. 由于硬件差异、固件版本更新、第三方依赖变更等原因，本文档生成的代码不保证一定能正常运行。

---

## 1. 硬件规格

| 项目 | 值 |
|---|---|
| 主控 | ESP-01S (ESP8266EX) |
| Flash | 1MB，采用双 OTA 分区 (`eagle.flash.1m.ota.ld`) |
| 工作频率 | 80 MHz |
| 串口波特率 | 115200 |

### 引脚约束（ESP-01S 特有）

- **GPIO0** — 启动模式引脚，正常启动需保持高电平，**切勿使用**
- **GPIO2** — 板载 LED，可作为状态指示灯（闪烁=WiFi/MQTT 状态）
- **无其他可用 GPIO**（GPIO1 为串口 TX，GPIO3 为串口 RX）

---

## 2. 开发环境与依赖

### PlatformIO 配置 (`platformio.ini`)

```ini
[env:esp01]
platform = espressif8266@^4.0.0
board = esp01
framework = arduino

board_build.f_cpu = 80000000L
board_build.f_flash = 40000000L
board_build.flash_mode = dout
board_build.ldscript = eagle.flash.1m.ota.ld  ; 双OTA分区

build_flags = 
    -Os
    -ffunction-sections
    -fdata-sections
    -Wl,--gc-sections
    -DARDUINOJSON_ENABLE_PROGMEM=1
    -DARDUINOJSON_USE_LONG_LONG=0

lib_deps = 
    bblanchon/ArduinoJson@^6.21.3    ; JSON 解析 (必须 v6, v7 API 不同)
    knolleary/PubSubClient@^2.8       ; MQTT 客户端

upload_speed = 115200
monitor_speed = 115200
```

> **来源**：`platformio.ini` 配置整体为 AI 生成。双 OTA 分区脚本 `eagle.flash.1m.ota.ld` 的选择参考了 [kldzj/esphome-chamber-sentinel](https://github.com/kldzj/esphome-chamber-sentinel) 的 ESP8266 OTA 配置。

### 框架内置库（无需 lib_deps）

- `ESP8266WiFi.h` / `WiFiClientSecure.h` — WiFi 与 TLS
- `ESP8266WebServer.h` — HTTP 服务器
- `Update.h` — OTA 固件写入
- `EEPROM.h` — 配置持久化

### ESP-01S 内存优化要点

- TLS 缓冲区设为 2048 字节（默认 8192，节省 ~6KB RAM）
- PubSubClient 缓冲区设为 1024 字节
- ArduinoJson 文档大小 1024 字节（MQTT 消息解析）/ 128-256 字节（HTTP 处理）
- 所有 HTML/CSS/JS 存 `PROGMEM`
- 避免 `String` 动态分配，优先 `char[]` + `snprintf`
- TLS 证书验证跳过 (`setInsecure()`) 以节省证书存储

> **来源**：以上内存优化策略为 AI 生成，综合参考 ESP8266 低内存最佳实践。

---

## 3. 项目文件结构

```
Bambu-ESP01S-Listener/
├── platformio.ini                   # PlatformIO 构建配置
├── Bambu-ESP01S-Listener.ino        # 主程序入口
├── config.h                         # 全局常量、结构体、枚举
├── storage.h                        # EEPROM 配置存储管理
├── mqtt_client.h                    # MQTT TLS 连接 + 消息解析 + 打印命令
├── web_server.h                     # HTTP 服务器、REST API、OTA 上传
├── html_page.h                      # Web UI (PROGMEM 存储)
└── doc/                             # 参考文档
    ├── mqtt.md                      # Bambu API 协议文档（OpenBambuAPI 本地缓存）
    ├── tls.md                       # TLS 证书相关说明
    └── video.md                     # 参考视频链接
```

所有头文件同时包含类声明和 `inline` 实现，无需额外 `.cpp` 文件。

> **来源**：文件结构为 AI 生成。

---

## 4. 架构与模块关系

```
  ┌──────────┐    ┌─────────────┐    ┌──────────────┐
  │EEPROM    │◄──►│  WiFi 管理   │◄──►│  WebServer   │
  │(storage) │    │  + AP 模式   │    │  (HTTP + API)│
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

主循环执行顺序：WiFi 检查 → AP 模式/WebServer（如 AP）→ ArduinoOTA（如启用）→ MQTT loop → HTTP 请求处理。

> **来源**：架构图为 AI 生成。

---

## 5. 配置结构体与枚举

### 5.1 `DeviceConfig` (`config.h`)

```c
struct DeviceConfig {
  char wifi_ssid[33] = "";           // WiFi SSID
  char wifi_password[65] = "";       // WiFi 密码
  char printer_ip[16] = "";          // 打印机局域网 IP
  char printer_device_id[33] = "";   // 打印机序列号 (DEVICE_ID)
  char printer_access_code[17] = ""; // LAN 访问码（MQTT 密码）
  char ota_url[129] = "";            // HTTP OTA 更新 URL（可选）
};
```

> **来源**：结构体定义为 AI 生成。各字段最大长度（33/65/16/33/17/129）分别对应 SSID 最大 32 字符、WPA2 密码最大 63 字符、IPv4 点分十进制最大 15 字符、Bambu Device ID 典型长度、16 位 Access Code、URL 通用上限。

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

> **来源**：枚举定义为 AI 生成。`STATE_OFFLINE` 为未来预留。

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

> **来源**：结构体定义为 AI 生成。

### 5.4 关键常量 (`#define`)

| 常量 | 值 | 说明 |
|---|---|---|
| `WIFI_CONNECT_TIMEOUT` | 20000UL | WiFi 连接超时 (ms) |
| `MQTT_RECONNECT_INTERVAL` | 5000UL | MQTT 重连间隔 (ms) |
| `STATUS_POLL_INTERVAL` | 60000UL | pushall 轮询间隔 (ms) |
| `HTTP_PORT` | 80 | Web 服务器端口 |
| `JSON_DOC_SIZE` | 1024 | MQTT 消息 JSON 文档大小 |
| `JSON_DOC_SIZE_SMALL` | 128 | 小型 HTTP 请求 |
| `JSON_DOC_SIZE_MEDIUM` | 256 | 中型 HTTP 响应 |
| `MQTT_TOPIC_REPORT` | `"device/%s/report"` | 打印机状态推送主题 |
| `MQTT_TOPIC_REQUEST` | `"device/%s/request"` | 命令发送主题 |
| `OTA_ENABLE` | false | ArduinoOTA（禁用，省内存） |
| `HTTP_OTA_ENABLE` | true | HTTP 上传固件（启用） |

> **来源**：常量值为 AI 生成。`MQTT_TOPIC_REPORT` / `MQTT_TOPIC_REQUEST` 的主题格式参考自 OpenBambuAPI 文档（见下节）。

---

## 6. MQTT 协议实现

> **来源**：本节全部协议细节（连接参数、主题格式、pushall 请求、命令 JSON、`gcode_state` 映射、字段提取表）参考自 [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)，本地缓存见 `doc/mqtt.md`。

### 6.1 连接参数

| 项目 | 值 |
|---|---|
| 协议 | TLS，端口 8883 |
| 用户名 | `bblp`（本地 broker 固定值） |
| 密码 | 打印机 LAN 访问码 |
| 客户端 ID | `esp-{MAC后4字节}` |
| TLS 配置 | `setInsecure()` 跳过证书验证，缓冲区 2048/2048 |

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

**注意**：P1P 上建议 ≥5 分钟间隔，过于频繁可能卡顿。本项目使用 60 秒（配置常量 `STATUS_POLL_INTERVAL`），可根据实际情况调整。

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

> **来源**：`sequence_id` 递增逻辑为 AI 生成，遵循 OpenBambuAPI 对 `sequence_id` 递增约束（每次命令 +1）。

---

## 7. REST API 参考

所有端点返回 JSON，统一启用 CORS (`Access-Control-Allow-Origin: *`)。

> **来源**：API 端点设计与路由风格为 AI 生成。

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
| POST | `/api/wifi` | `{"ssid":"XXX","pass":"YYY"}` | 保存 WiFi 后自动重启 |
| POST | `/api/printer` | `{"ip":"...","device_id":"...","access_code":"..."}` | 保存打印机连接参数 |
| POST | `/api/reset` | 无 | 恢复默认设置并重启 |

### 7.4 OTA 固件更新

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/update` | OTA 上传页面（HTML 表单 + XHR 进度） |
| POST | `/upload` | multipart/form-data 接收 `.bin` 文件，使用 ESP8266 `Update` 库写入 |

> **实现说明**：OTA 上传通过 `ESP8266WebServer::on()` 注册路由特定的 upload handler（4 参数版本），直接调用 `Update.begin()` / `Update.write()` / `Update.end()` 完成固件写入。不走 `ESP8266HTTPUpdateServer`（该库需要 mDNS，ESP-01S 内存占用太大）。

> **来源**：HTTP OTA 上传模式（路由特定 upload handler + 直写 Update 库）参考自 [kldzj/esphome-chamber-sentinel](https://github.com/kldzj/esphome-chamber-sentinel)。

---

## 8. EEPROM 存储布局

ESP-01S EEPROM 模拟大小 4KB。采用固定地址布局（非序列化），避免内存碎片。

> **来源**：EEPROM 固定地址布局设计为 AI 生成，兼顾可读性和 EEPROM 写入寿命控制。

### 魔数与版本

```c
#define CONFIG_MAGIC    0x4C534231UL  // "BSL1" = Bambu Speaker Listener v1
#define CONFIG_VERSION  1
```

### 字段偏移表

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

### 读写逻辑

- 读取时检查魔数，不匹配则返回 `DeviceConfig()` 默认值
- 所有写入通过 `EEPROM.write()` 逐字节写入后 `EEPROM.commit()`
- `setter` 方法立即 commit（EEPROM 写入寿命有限，调用方控制频率）

---

## 9. WiFi 管理与 AP 模式

- **AP SSID**：`BambuListener-Setup`
- **AP 密码**：`12345678`（至少 8 字符）
- 未配置 SSID 或连续 3 次连接失败 → 启动 AP 模式
- AP 模式下只运行 Web 服务器（用于 WiFi 配置），不启动 MQTT
- STA 模式下连接丢失每 10 秒尝试重连

> **来源**：WiFi/AP 模式设计参考自 [VR6syncro/Bambulab_Esp32_LEDController](https://github.com/vr6syncro/Bambulab_Esp32_LEDController) 的 WiFi 配置思路。

---

## 10. Web UI 规格 (`html_page.h`)

单页应用，所有 HTML/CSS/JS 存入 `PROGMEM`。深色主题，CSS 内联，JS 使用原生 `fetch` API 轮询 `/api/status`（3 秒间隔）。

> **规格说明**：本章节描述的是**功能规格**，AI 生成时应据此实现**功能等价**的 UI。HTML/CSS/JS 可以自由发挥，只要满足下述模块布局和交互逻辑即可。

> **来源**：Web UI 整体设计风格参考自 [VR6syncro/Bambulab_Esp32_LEDController](https://github.com/vr6syncro/Bambulab_Esp32_LEDController) 的 Dashboard 页面布局。

### 页面模块

1. **打印机状态卡片** — 连接状态 Badge、状态 Badge（空闲/打印中/已暂停/错误）、喷嘴温度（当前/目标，°C）、热床温度（当前/目标，°C）、打印进度百分比、剩余时间（自动格式化 h+m 或 m）、当前打印文件名
2. **打印控制卡片** — 暂停 / 继续 / 停止按钮（均有 confirm 弹窗）
3. **网络设置卡片** — WiFi SSID 输入 + 密码输入，保存后设备自动重启
4. **打印机设置卡片** — IP 地址 / Device ID（序列号） / LAN 访问码，保存后自动重连 MQTT

### 额外页面

- `/update` — OTA 固件上传页面（独立 HTML 表单 + XHR 上传进度条 + 成功/失败提示）

---

## 11. 主程序流程 (`Bambu-ESP01S-Listener.ino`)

> **来源**：主程序初始化顺序和主循环逻辑为 AI 生成。

```c
// 全局实例
StorageManager storage;
DeviceConfig config;
PrinterStatus printerStatus;
BambuMQTT bambuMQTT;
WebServer httpServer;

// setup() 顺序:
// 1. Serial.begin(115200)
// 2. storage.begin() → 加载 config（检查 EEPROM 魔数）
// 3. connectWiFi() → STA 模式，失败则启动 AP
// 4. bambuMQTT.begin(config, printerStatus) → 构建主题、配置 TLS
// 5. httpServer.setConfigRefs(...) + httpServer.begin()

// loop() 顺序:
// checkWiFi()
// if (apModeActive) { httpServer.loop(); storage.loop(); return; }
// #if OTA_ENABLE → ArduinoOTA.handle()
// bambuMQTT.loop()      // 自动重连 + pushall 轮询
// httpServer.loop()     // 所有 HTTP 请求（含 /api/* 和 /update）
// storage.loop()
```

---

## 12. 技术参考

- Bambu 打印机 API 协议文档：[Doridian/OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI)（本地缓存见 `doc/mqtt.md`）
- ESP8266 HTTP OTA 实现参考：[kldzj/esphome-chamber-sentinel](https://github.com/kldzj/esphome-chamber-sentinel)
- WiFi AP 配置模式 + Web Dashboard 风格参考：[VR6syncro/Bambulab_Esp32_LEDController](https://github.com/vr6syncro/Bambulab_Esp32_LEDController)

---

## 13. 来源声明

| 模块 / 章节 | 来源 | 说明 |
|---|---|---|
| **MQTT 协议细节**（连接参数、主题、pushall 请求、命令 JSON、gcode_state 映射、字段提取） | OpenBambuAPI / `doc/mqtt.md` | 协议规范源自官方 API 文档 |
| **双 OTA 分区脚本** `eagle.flash.1m.ota.ld` | 参考 [kldzj/esphome-chamber-sentinel](https://github.com/kldzj/esphome-chamber-sentinel) | 选型参考该项目的 ESP8266 OTA 配置 |
| **HTTP OTA 上传模式**（路由特定 upload handler） | 参考 [kldzj/esphome-chamber-sentinel](https://github.com/kldzj/esphome-chamber-sentinel) | 实现思路参考该项目的 OTA 上传方式 |
| **WiFi/AP 配置模式** | 参考 [VR6syncro/Bambulab_Esp32_LEDController](https://github.com/vr6syncro/Bambulab_Esp32_LEDController) | AP 模式 + Web 配置页面思路参考 |
| **Web UI 深色主题风格** | 参考 [VR6syncro/Bambulab_Esp32_LEDController](https://github.com/vr6syncro/Bambulab_Esp32_LEDController) | Dashboard 布局风格参考 |
| **ESP-01S 内存优化策略** | AI 生成 | 综合 ESP8266 低内存最佳实践 |
| **`config.h` 结构体与常量** | AI 生成 | 各字段长度由 SSID/密码/IP 等实际约束推导 |
| **`storage.h` EEPROM 固定地址布局** | AI 生成 | 非序列化方案，兼顾可读性与写入寿命 |
| **`mqtt_client.h` 类设计** | AI 生成 | PubSubClient + WiFiClientSecure 封装 |
| **`web_server.h` REST API 端点设计** | AI 生成 | 路由路径、请求体格式、响应格式 |
| **`html_page.h` Web UI 实现** | AI 生成 | PROGMEM 存储、fetch 轮询、3 秒刷新 |
| **主程序流程（setup/loop）** | AI 生成 | 初始化顺序、AP 模式条件分支 |

---

## 14. License

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

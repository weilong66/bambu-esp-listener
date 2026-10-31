# ESP-01S 拓竹打印机 Web 监视器 + 继电器控制 完整方案

> 状态：已实现（Arduino IDE 单文件版）。本文档是自包含的设计规格，基于项目 README.md 与 `ESP-01S_Relay` 子文件夹内容整理。

## 1. 目标

用一块 ESP-01S（1MB Flash）+ 5V 单路继电器模块，做成一体的局域网小网关：

- 通过 MQTT/TLS 连接拓竹打印机本地 broker，实时抓取全量打印状态（温度、进度、图层、速度、风扇、耗材、错误、WiFi 等）；
- 提供深色主题 Web 面板：纯状态监视 + 网络/打印机配置（不发送任何打印控制命令）；
- 额外提供一路继电器远程开关（用于电源通断、照明、风扇等），支持手动开关、条件自动、延时、手动接管、状态回读与上电安全态；
- 支持 HTTP OTA 固件上传，避免每次开盖插串口。

## 2. 硬件清单与接线

| 部件 | 说明 |
|---|---|
| ESP-01S | AI-Thinker WiFi 模块，1MB Flash |
| ESP-01S 继电器模块 | 5V 供电、GPIO0 控制、低电平触发、10A/250VAC 负载 |
| 5V/≥250mA 电源 | 继电器吸合电流较大，勿直接靠串口板供电 |
| 下载器（选配） | 首次烧录用（后续走 HTTP OTA） |

接线要点（继电器板为 2×4 排母，ESP-01S 直接插入）：

- `VCC/GND` → 5V 电源（GND 需与 ESP-01S 共地）
- `GPIO0` → 继电器控制（板上已接驱动三极管/光耦）
- 继电器输出端 `COM/NO/NC` 接被控负载（按需选择常开 NO 或常闭 NC，默认建议用 NO）

## 3. 关键硬件约束（务必注意）

1. GPIO0 = 启动/烧录复用脚：ESP8266 上电时 GPIO0 被 ROM bootloader 采样。GPIO0 在复位瞬间为低会进入烧录模式、不跑程序。继电器板内部需保证上电瞬间 GPIO0 不被拉低；初始化代码要尽早把 GPIO0 置为“继电器关闭”电平。
2. 继电器极性可配置：实测本模块为**低电平触发**（吸合=LOW），市售同型号板另有高电平版本，因此把触发有效电平抽成常量，便于按板子一键调整。
3. 上电安全态：`setup()` 最开头立即把继电器置为关闭，防止上电瞬间误动作。
4. GPIO2 注意事项：本方案不使用 GPIO2（ESP-01S 另一个可用脚），避免与 LED/启动挂钩冲突，保持最小改动。

## 4. 软件架构与文件结构

采用 **Arduino IDE sketch** 形式：`.ino` 为主程序，其余模块以头文件放在同一目录（Arduino IDE 会自动一起编译）：

```
BambuESP01SRelay/
├── BambuESP01SRelay.ino   # 主程序：全局对象、网络管理、setup/loop
├── config.h               # 常量 + 数据结构（配置集中点）
├── storage.h              # EEPROM 持久化
├── relay.h                # GPIO0 继电器控制
├── mqtt_client.h          # TLS 8883 客户端 + 状态解析 + 打印命令
├── html_page.h            # INDEX_HTML / OTA_PAGE（PROGMEM）
├── web_server.h           # REST 路由 + OTA 上传
├── README.md              # 使用说明
└── PLAN.md                # 本文档
```

依赖顺序（由 `#include` 决定，均为 `#pragma once`）：

```
config.h → storage.h / relay.h / mqtt_client.h
         → html_page.h → web_server.h → BambuESP01SRelay.ino
```

模块关系：

```
WebServer ──► Relay（GPIO0 开关）
WebServer ──► BambuMQTT ──► 打印机
Storage   ──► DeviceConfig 上电加载
```

## 5. Arduino IDE 编译配置

| 选项 | 值 | 说明 |
|---|---|---|
| Board | Generic ESP8266 Module | ESP-01S 用通用板型 |
| Flash Size | 1MB (FS:64KB OTA:~470KB) | 固件约 437KB，留 OTA 空间 |
| Flash Mode | DOUT | ESP-01S 兼容性最好 |
| CPU Frequency | 80 MHz | 默认 |
| Upload Speed | 115200 | 下载器不稳时可降到 57600 |

依赖库（库管理器安装）：

- `PubSubClient`（knolleary）2.8
- `ArduinoJson`（bblanchon）**6.x**（代码使用 `StaticJsonDocument`，容量编译期固定；7.x 不支持，`config.h` 内有 `#error` 拦截）

内存优化（严格）：TLS 缓冲 2048、PubSubClient 1024、ArduinoJson 文档 1280、HTML/CSS/JS 全部 PROGMEM、避免 String 动态分配、不启用 ArduinoOTA。

### 5.1 机型适配（编译期参数页）

`config.h` 通过 `PRINTER_MODEL` 宏选择机型，派生 5 个能力开关，随固件编译固化：

| 宏 | 含义 |
|---|---|
| `FEAT_CHAMBER_TEMP` | 腔体温度 |
| `FEAT_AUX_FAN` | 辅助风扇（`big_fan1`） |
| `FEAT_CHAMBER_FAN` | 腔体风扇（`big_fan2`） |
| `FEAT_BED_TYPE` | 热床类型 |
| `FEAT_NOZZLE_INFO` | 喷嘴类型/直径 |

另设 `HAS_AMS` 独立开关（A1 系列 AMS lite 为选配）。

下发方式：`/api/config` 返回 `model` 与各 `has_*` 布尔值，前端 `applyFeatures()` 隐藏对应行/整卡（AMS 卡、腔温行、热床类型行、辅助/腔体风扇行、喷嘴行）。这样 HTML 仍是一段 PROGMEM 常量，无需服务端模板，RAM 占用不变。

机型默认矩阵（X1C/X1E 全量；P1S 有辅助/腔体风扇、无腔温；P1P 无辅助/腔体风扇；A1 与 A1mini 仅部件风扇 + 热端风扇）。字段是否实际上报随固件版本变化，可在 `config.h` 直接改 `FEAT_*`。

## 6. 配置持久化

EEPROM 模拟，魔数 `CONFIG_MAGIC` + `CONFIG_VERSION`，偏移布局：

| 项 | 偏移 | 长度 |
|---|---|---|
| 魔数 | 0 | 4 |
| 版本 | 4 | 1 |
| WiFi SSID | 8 | 33 |
| WiFi 密码 | 41 | 65 |
| 打印机 IP | 106 | 16 |
| Device ID | 122 | 33 |
| 访问码 | 155 | 17 |
| OTA URL | 172 | 129 |

继电器运行态**不持久化**，每次上电强制关闭（最安全），避免 EEPROM 频繁写入。

## 7. 常量 / 结构体 / 枚举设计

```cpp
#define RELAY_PIN             0        // GPIO0 控制继电器
#define RELAY_ACTIVE_LEVEL    LOW      // 触发有效电平：本模块低电平吸合（HIGH=高电平吸合）
#define RELAY_POWER_ON_STATE  false    // 上电默认：false=关闭
```

继电器状态用布尔 `relayOn`，展示层（“ON”/“OFF”/“吸合/断开”）与逻辑层（GPIO 电平）分离。

## 8. MQTT 设计

- TLS 8883，用户名 `bblp`，密码 = 打印机 LAN 访问码；
- 订阅 `device/{DEVICE_ID}/report`，发布 `device/{DEVICE_ID}/request`；
- `pushall` 每 60s 请求完整状态；
- `gcode_state` 映射：IDLE→空闲、RUNNING→打印中、PAUSE/PAUSED→暂停、ERROR→错误、FINISH→空闲。

## 9. 继电器控制模块

```cpp
class Relay {
public:
  void begin();                       // pinMode + 立马上电置为安全态
  void set(bool on);                  // 写 GPIO，内部换算有效电平
  bool state() const;                 // 返回逻辑态
  void toggle();
private:
  bool _on = false;
};
```

关键实现约定：

- `set(true)` → `digitalWrite(RELAY_PIN, RELAY_ACTIVE_LEVEL)`；`set(false)` → `digitalWrite(RELAY_PIN, !RELAY_ACTIVE_LEVEL)`；
- `begin()` 内先 `pinMode(RELAY_PIN, OUTPUT)`，再 `set(RELAY_POWER_ON_STATE)`；
- 极性只在 `RELAY_ACTIVE_LEVEL` 一处配置。

### 9.1 规则引擎 `RelayAutomation`

| 能力 | 说明 |
|---|---|
| 模式 | 手动（仅网页开关）/ 自动（按规则） |
| 触发掩码 | 打印中 0x01、暂停 0x02、故障 0x04、温度阈值 0x08（任一满足即吸合） |
| 延时 | `delay_on` 吸合延时、`delay_off` 断开延时（秒） |
| 温度阈值 | 取喷嘴与热床较大值比较，0=禁用 |
| 手动接管 | 手动操作后 `override_min` 分钟内暂停自动逻辑 |
| 上电安全 | 上电强制断开，运行态不落盘 |

`update()` 每 1 秒调用一次：接管期内跳过；非自动模式跳过；满足条件时启动吸合计时，条件消失时启动断开计时。规则变更后调用 `resetTimers()` 立即按新规则计算。

## 10. REST API 设计

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/` | Web 面板 |
| GET | `/api/status` | 全量打印状态 + 设备信息 |
| GET | `/api/config` | 当前配置（含继电器规则） |
| POST | `/api/wifi` | `{"ssid":"...","pass":"..."}`，保存后重启 |
| POST | `/api/printer` | `{"ip":"...","device_id":"...","access_code":"..."}` |
| POST | `/api/reset` | 清除配置并重启 |
| GET | `/api/relay` | 状态 + 规则 + 运行时（`on/mode/want/override`） |
| POST | `/api/relay` | 手动 `{"on":true}`/`{"toggle":true}`；规则 `{"settings":{...}}` |
| GET | `/update` | OTA 上传页 |
| POST | `/upload` | 固件上传 |

所有 JSON 接口统一 CORS `*`。

## 11. Web UI 设计

8 个区域：打印任务、温度、速度与风扇、耗材/AMS、错误与系统、继电器控制、网络设置、打印机设置。

- 状态区覆盖 report 中可用字段：温度（喷嘴/热床/腔体）、进度/图层/剩余、速度档位与倍率、4 路风扇、喷嘴与热床类型、AMS 槽位/类型/湿度/温度、错误码/HMS/失败原因、打印机 WiFi 信号、ESP WiFi 信号、空闲内存、运行时间；
- 继电器卡：状态指示灯（红=闭合 / 灰=断开）、模式、自动目标、接管倒计时、开/关/切换按钮 + 规则表单（模式、触发条件多选、吸合/断开延时、温度阈值、接管分钟）；
- 原生 fetch + 3 秒轮询；
- 所有文案映射由前端维护，与后端布尔/掩码解耦。

## 12. OTA

- HTTP OTA：`/update` 页面 + `/upload` 接收 `.bin`，直写 `Update` 库；
- 不启用 ArduinoOTA（节约 RAM）。
- Arduino IDE 通过「项目 → 导出已编译的二进制文件」获得 `.bin`。

## 13. 主程序流程

`setup()`：

1. `Serial.begin(115200)`
2. `relay.begin()`（最先，确保安全态）
3. `storage.begin()` + 加载 config
4. `connectSTA()`（失败进 AP）
5. 有打印机 IP 则 `bambuMQTT.begin(...)`
6. `webServer.begin(...)`（注册所有路由，含 relay）

`loop()`：

```
checkNetwork() → webServer.loop() → (非 AP 模式) bambuMQTT.loop()
```

## 14. 装机步骤

1. Arduino IDE 打开 `BambuESP01SRelay.ino`，选好板型与分区，编译；
2. ESP-01S 拔离继电器板，GPIO0 接地后上电，烧录固件；
3. 串口确认启动日志，按 AP 配网流程填 WiFi；
4. 填打印机 IP / Device ID / Access Code，确认状态卡有数据；
5. 实测继电器极性，必要时翻转 `RELAY_ACTIVE_LEVEL`；
6. 验证上电默认关、OTA 升级可用。

## 15. 风险清单

- GPIO0 启动模式冲突（最高风险）：上电/复位瞬间 GPIO0 不可被拉低，否则进烧录模式；烧录时必须拔离继电器板。
- 继电器极性不确定：以 `RELAY_ACTIVE_LEVEL` 常量隔离，实测翻转一次即可。
- 供电不足：继电器吸合瞬间电流大，5V 电源须 ≥250mA，勿用 CH340 串口板供电。
- 1MB Flash 紧张：严格 PROGMEM + 缓冲调小，固件体积控制在 470KB 以内。
- 拓竹 P1 系列 pushall 频率：60s 间隔，过频可能卡顿。

## 16. 实现状态（已落地）

单文件固件已完成拆分，编译验证通过（ESP8266 Arduino Core 3.x）：

| 文件 | 内容 |
|---|---|
| `config.h` | 常量、机型选择与 `FEAT_*` 能力开关、`DeviceConfig`、`RelaySettings`、`PrinterStatus`、`PrinterState`、`stateToStr` |
| `storage.h` | `StorageManager`：EEPROM 魔数 + 版本(v2) + 偏移布局（含继电器规则） |
| `relay.h` | `Relay`（GPIO0）+ `RelayAutomation`（规则引擎、延时、手动接管） |
| `mqtt_client.h` | `BambuMQTT`：TLS 8883、pushall、全量状态解析（仅只读） |
| `html_page.h` | `INDEX_HTML` / `OTA_PAGE`（8 区域 UI + OTA 页，PROGMEM） |
| `web_server.h` | `WebServerManager`：状态/配置/继电器/OTA 接口（无打印控制） |
| `BambuESP01SRelay.ino` | 全局对象、STA/AP 网络管理、1s 规则调度、setup/loop |

编译结果：RAM 39.2%（32132/81920），Flash 59.0%（449171/761840，Arduino Core 3.x 对照构建）。

注：仓库 `.gitignore` 当前忽略除 README/LICENSE 外的所有文件，如需用 Git 跟踪源码需调整该文件。

#pragma once
#include <Arduino.h>
#include "config.h"

/* 继电器硬件抽象：只负责 GPIO 电平换算 */
class Relay {
public:
  void begin() {
    pinMode(RELAY_PIN, OUTPUT);
    set(RELAY_POWER_ON_STATE);
  }

  void set(bool on) {
    _on = on;
    digitalWrite(RELAY_PIN, on ? RELAY_ACTIVE_LEVEL : !RELAY_ACTIVE_LEVEL);
  }

  void toggle() { set(!_on); }

  bool state() const { return _on; }

private:
  bool _on = false;
};

/* 继电器规则引擎：手动 / 自动（按打印状态与温度），带延时与手动接管 */
class RelayAutomation {
public:
  void attach(Relay& relay, RelaySettings& settings) {
    _relay = &relay;
    _s = &settings;
    _manualUntil = 0;
    _onAt = 0;
    _offAt = 0;
    _want = false;
  }

  /* 周期调用（建议 1s） */
  void update(const PrinterStatus& st, unsigned long now) {
    if (_relay == nullptr || _s == nullptr) return;

    // 手动接管期内不执行自动逻辑
    if (overrideActive(now)) {
      _onAt = 0;
      _offAt = 0;
      return;
    }

    if (_s->mode != RELAY_MODE_AUTO) {
      _onAt = 0;
      _offAt = 0;
      _want = false;
      return;
    }

    _want = conditionMet(st);

    if (_want) {
      _offAt = 0;
      if (!_relay->state()) {
        if (_onAt == 0) _onAt = now + (unsigned long)_s->delay_on * 1000UL;
        if ((long)(now - _onAt) >= 0) {
          _relay->set(true);
          _onAt = 0;
        }
      } else {
        _onAt = 0;
      }
    } else {
      _onAt = 0;
      if (_relay->state()) {
        if (_offAt == 0) _offAt = now + (unsigned long)_s->delay_off * 1000UL;
        if ((long)(now - _offAt) >= 0) {
          _relay->set(false);
          _offAt = 0;
        }
      } else {
        _offAt = 0;
      }
    }
  }

  /* 手动开关：返回当前是否处于手动接管 */
  bool manualSet(bool on, unsigned long now) {
    if (_relay == nullptr) return false;
    _relay->set(on);
    _onAt = 0;
    _offAt = 0;
    if (_s != nullptr && _s->override_min > 0) {
      _manualUntil = now + (unsigned long)_s->override_min * 60000UL;
      return true;
    }
    _manualUntil = 0;
    return false;
  }

  bool manualToggle(unsigned long now) {
    if (_relay == nullptr) return false;
    return manualSet(!_relay->state(), now);
  }

  /* 规则变更后清掉延时计时，立即按新规则计算 */
  void resetTimers() {
    _onAt = 0;
    _offAt = 0;
  }

  bool overrideActive(unsigned long now) const {
    if (_manualUntil == 0) return false;
    return (long)(_manualUntil - now) > 0;
  }

  unsigned long overrideRemain(unsigned long now) const {
    if (!overrideActive(now)) return 0;
    return (_manualUntil - now) / 1000UL;
  }

  bool want() const { return _want; }
  bool isOn() const { return _relay && _relay->state(); }

private:
  Relay* _relay = nullptr;
  RelaySettings* _s = nullptr;
  unsigned long _manualUntil = 0;
  unsigned long _onAt = 0;
  unsigned long _offAt = 0;
  bool _want = false;

  /* 判断当前是否满足吸合条件 */
  bool conditionMet(const PrinterStatus& st) {
    uint8_t t = _s->trigger;

    if ((t & RELAY_TRIG_PRINTING) && st.state == STATE_PRINTING) return true;
    if ((t & RELAY_TRIG_PAUSED)   && st.state == STATE_PAUSE)    return true;
    if ((t & RELAY_TRIG_ERROR)    && st.state == STATE_ERROR)    return true;

    if (t & RELAY_TRIG_TEMP) {
      if (_s->temp_threshold <= 0) return false;
      float hi = st.nozzle_temp > st.bed_temp ? st.nozzle_temp : st.bed_temp;
      if (hi >= (float)_s->temp_threshold) return true;
    }
    return false;
  }
};

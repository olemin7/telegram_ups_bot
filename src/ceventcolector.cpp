/*
 * ceventcolector.cpp
 *
 *  Created on: Dec 24, 2022
 *      Author: oleksandr
 */

#include <Arduino.h>
#include <TimeLib.h>
#include <ceventcolector.h>

#include <sstream>
#include <vector>

class cevent_colector::implementation {
 private:
  struct event_t {
    ekind kind;
    uint32_t ts;
    bool is_ok;
  };
  std::vector<event_t> ev_list_;

 public:
  std::string event_name_str(const ekind kind) const;
  std::string ts_to_str(uint32_t ts) const {
    std::stringstream result;
    const auto diff = millis() - ts;
    auto passed = diff;
    passed /= 1000;
    if (60 >= passed) {
      result << passed << " секунд";
    } else {
      passed /= 60;
      if (60 >= passed) {
        result << passed << " хвилин";
      } else {
        passed /= 60;
        result << passed << " годин";
      }
    }
    //    if (timeNotSet != timeStatus()) {
    //      auto now = time(nullptr) - diff;
    //      auto tm = *localtime(&now);
    //    }
    return result.str();
  }

  void event(const ekind kind, bool is_ok) {
    ev_list_.push_back(event_t{kind, millis(), is_ok});
  }
  std::string get_status(const ekind kind, bool extent) const {
    bool no_data = true;
    std::stringstream result;
    event_t last;
    result << event_name_str(kind);
    for (const auto &el : ev_list_) {
      if (kind == el.kind) {
        no_data = false;
        last = el;
      }
    }
    if (no_data) {
      result << "(нема данних)";
    } else {
      const auto now = millis();
      result << " " << (last.is_ok ? "норма" : "відсутній");
      result <<" "<< ts_to_str(last.ts);
    }
    return result.str();
  }
  std::string get_status(bool extend) const {
    std::stringstream summary;
    for (auto ev = static_cast<uint8_t>(ekind::ev_start);
         ev < static_cast<uint8_t>(ekind::ev_last); ev++) {
      summary << get_status(static_cast<ekind>(ev), extend);
      summary << std::endl;
    }
    return summary.str();
  }
  };

std::string cevent_colector::implementation::event_name_str(
    const ekind kind) const {
  switch (kind) {
    case ekind::ev_start:
      return "запуск";
    case ekind::ev_tb_connectd:
      return "телеграм з'єднання";
    case ekind::ev_power:
      return "живлення";
    case ekind::ev_battery:
      return "батарея";
    default:;
  }
  return "нерозпізнано";
}

cevent_colector::cevent_colector()
    : impl_(std::make_unique<implementation>()) {}
cevent_colector::~cevent_colector() = default;

void cevent_colector::event(const ekind event, bool is_ok) {
  impl_->event(event, is_ok);
};
std::string cevent_colector::get_status(const ekind event, bool extent) const  {
  return impl_->get_status(event, extent);
}
std::string cevent_colector::get_status(bool extent) const {
  return impl_->get_status(extent);
}

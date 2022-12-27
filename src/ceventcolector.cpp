/*
 * ceventcolector.cpp
 *
 *  Created on: Dec 24, 2022
 *      Author: oleksandr
 */

#include <Arduino.h>
#include <TimeLib.h>
#include <ceventcolector.h>

#include <cstdio>
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
  // static constexpr auto EVENT_COUNT = 1000;

 public:
  std::string event_name_str(const ekind kind) const;
  std::string ts_to_str(uint32_t ts) const {
    const auto diff_sec = (millis() - ts) / 1000;
    const auto now = time(nullptr);
    if (diff_sec < 5) {
      return " щойно";
    } else if (diff_sec < 24 * 60 * 60 || (now < 24 * 60 * 60)) {
      auto passed = diff_sec;
      const int sec = passed % 60;
      passed /= 60;
      const int min = passed % 60;
      const int hours = passed / 60;
      char tt[20];
      std::snprintf(tt, sizeof(tt), " %d:%02d:%02d", hours, min, sec);
      return tt;
    }
    // time set
    auto event_at = now - diff_sec;
    auto tm = *localtime(&event_at);
    char tt[30];
    strftime(tt, sizeof(tt), " від %e %b %G %R", &tm);
    return tt;
  }

  void event(const ekind kind, bool is_ok) {
    ev_list_.push_back(event_t{kind, millis(), is_ok});
  }
  std::string get_status(const ekind kind) const {
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
      result << " " << (last.is_ok ? "норма" : "відсутній");
      result <<" "<< ts_to_str(last.ts);
    }
    return result.str();
  }

  std::string get_summary() const {
    std::stringstream summary;
    for (auto ev = static_cast<uint8_t>(ekind::ev_start);
         ev < static_cast<uint8_t>(ekind::ev_last); ev++) {
      summary << get_status(static_cast<ekind>(ev));
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

std::string cevent_colector::get_status(const ekind event) const  {
  return impl_->get_status(event);
}

std::string cevent_colector::get_status() const {
  std::stringstream status;
  for (auto ev = static_cast<uint8_t>(ekind::ev_start);
       ev < static_cast<uint8_t>(ekind::ev_last); ev++) {
    status << impl_->get_status(static_cast<ekind>(ev));
    status << std::endl;
  }
  return status.str();
}

std::string cevent_colector::get_summary() const {
  return impl_->get_summary();
}

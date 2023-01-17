/*
 * ceventcolector.cpp
 *
 *  Created on: Dec 24, 2022
 *      Author: oleksandr
 */

#include <Arduino.h>
#include <TimeLib.h>
#include <ceventcolector.h>
#include <logs.h>

#include <cstdio>
#include <sstream>
#include <vector>
  
struct event_t {
  cevent_colector::ekind kind;
  uint32_t ts = 0;
  bool is_ok;
  };
  
class cevent_colector::implementation {
 public:

  // static constexpr auto EVENT_COUNT = 1000;

 public:
  std::string event_name_str(const ekind kind) const;

  void event(const ekind kind, bool is_ok) {
    ev_list_.push_back(event_t{kind, millis(), is_ok});
  }

  std::string get_status(const ekind kind) const;
  std::string get_summary() const;

 private:
  std::vector<event_t> ev_list_;
  std::string duration_to_str(uint64_t duration_ms) const;
  std::string ts_to_str(uint32_t ts) const;
};

class cevent_calculate {
 public:
  uint32_t ts_beg_ = 0;
  uint64_t power_time_ = 0;
  uint64_t power_outages_count_ = 0;
  uint64_t power_outages_max_ = 0;
  event_t power_pre_state_;
  void event(const event_t &ev);
  void finalize();

 private:
};

void cevent_calculate::event(const event_t &ev) {
  if (!ts_beg_) {
    ts_beg_ = ev.ts;
  }
  switch (ev.kind) {
    case cevent_colector::ekind::ev_power:
      if (power_pre_state_.ts) {  // inited
        const auto duration = ev.ts - power_pre_state_.ts;
        if (ev.is_ok) {  // turned on
          if (duration > power_outages_max_) {
            power_outages_max_ = duration;
          }
        } else {
          power_outages_count_++;
          power_time_ += duration;
        }
      } else {
        if (!ev.is_ok) {
          power_outages_count_++;
        }
      }
      power_pre_state_ = ev;
      break;
  }
}

void cevent_calculate::finalize() {
  const auto now = millis();
  if (power_pre_state_.ts) {  // inited
    const auto duration = now - power_pre_state_.ts;
    if (!power_pre_state_.is_ok) {  // now is off
      if (duration > power_outages_max_) {
        power_outages_max_ = duration;
      }
    } else {
      power_time_ += duration;
    }
  }
}

class cevent_graph {
 public:
  void event(const event_t &ev);
  std::string finalize();

 private:
  event_t power_pre_state_;
  std::stringstream result;
  bool is_started = false;
  time_t pre_;
  time_t ts_beg_;  // include local time offset
  bool is_need_endl_ = false;
  bool is_ok_;
  static constexpr auto time_day = 24 * 60 * 60;
  static constexpr auto time_step_ = 60 * 60;
  void put_char(const time_t &tm, char ch);
};

void cevent_graph::put_char(const time_t &at, char ch) {
  while (at >= pre_) {
    const auto step = (pre_ - ts_beg_) % time_day;
    const auto tm = *localtime(&pre_);
    DBG_OUT << "step " << step;
    if (step == 0) {
      char tt[10];
      strftime(tt, sizeof(tt), "%D", &tm);
      if (is_need_endl_) {
        result << std::endl;
      }
      is_need_endl_ = true;
      result << "<pre>" << tt << "[";
    }
    switch (ch) {
      case 0:
        result << '-';
        break;
      case 1:
        result << (tm.tm_hour % 10);
        break;
      default:
        result << ch;
    }

    if (step == (time_day - time_step_)) {
      result << "]</pre>";
    }
    pre_ += time_step_;
  }
}

void cevent_graph::event(const event_t &ev) {
  DBG_FUNK();
  if (ev.kind == cevent_colector::ekind::ev_power) {
    const auto diff_sec = (millis() - ev.ts) / 1000;
    // time set
    const auto event_at = time(nullptr) - diff_sec;

    if (!is_started) {
      auto tm = *localtime(&event_at);
      auto tm_pre = tm;
      tm_pre.tm_hour = 0;
      tm_pre.tm_min = 0;
      tm_pre.tm_sec = 0;
      ts_beg_ = pre_ = mktime(&tm_pre);
      if (tm.tm_hour) {
        put_char(mktime(&tm) - time_step_, ' ');
      }
      is_started = true;
    }
    is_ok_ = ev.is_ok;
    put_char(event_at, ev.is_ok);
  }
}
std::string cevent_graph::finalize() {
  if (!is_started) {
    return {};
  }
  put_char(millis(), is_ok_);
  auto tm = *localtime(&pre_);
  tm.tm_hour = 23;
  tm.tm_min = 59;
  tm.tm_sec = 59;
  put_char(mktime(&tm), ' ');
  return result.str();
}

std::string cevent_colector::implementation::duration_to_str(
    uint64_t duration_ms) const {
  auto passed = duration_ms / 60000;
  const int min = passed % 60;
  const int hours = passed / 60;
  char tt[20];
  std::snprintf(tt, sizeof(tt), " %d:%02d", hours, min);
  return tt;
}

std::string cevent_colector::implementation::ts_to_str(uint32_t ts) const {
  const auto diff_sec = (millis() - ts) / 1000;
  const auto now = time(nullptr);
  if (diff_sec < 60) {
    return " щойно";
  } else if (diff_sec < 24 * 60 * 60 || (now < 24 * 60 * 60)) {
    return duration_to_str(diff_sec * 1000);
  }
  // time set
  auto event_at = now - diff_sec;
  auto tm = *localtime(&event_at);
  char tt[30];
  strftime(tt, sizeof(tt), " від %e %b %G %R", &tm);
  return tt;
}

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

std::string cevent_colector::implementation::get_status(
    const ekind kind) const {
  std::stringstream result;
  event_t pre;
  event_t last;
  result << event_name_str(kind);
  for (const auto &el : ev_list_) {
    if (kind == el.kind) {
      pre = last;
      last = el;
    }
  }
  if (last.ts == 0) {
    result << "(нема данних)";
  } else {
    result << " " << (last.is_ok ? "норма" : "відсутній");
    result << " " << ts_to_str(last.ts);
    if (pre.ts) {
      result << ", попередній стан тривав " << duration_to_str(last.ts - pre.ts)
             << "";
    }
  }
  return result.str();
}

std::string cevent_colector::implementation::get_summary() const {
  DBG_FUNK();

  cevent_calculate whole_report;
  const auto ts_cur = millis();
  cevent_graph event_graph;

  for (const auto &el : ev_list_) {
    event_graph.event(el);
    whole_report.event(el);
  }
  //clang-format off
  //  auto tt = time(nullptr);
  //  auto tm = *localtime(&tt);
  //  tm.tm_hour = 0;
  //  tm.tm_min = 0;
  //  tm.tm_sec = 0;
  //  const auto _day_2 =mktime(&tm)-2*24 * 60 * 60 * 1000;
  //
  //  event_graph.event(event_t{cevent_colector::ekind::ev_power, _day_2+0,
  //  true}); event_graph.event(event_t{cevent_colector::ekind::ev_power,
  //  _day_2+20 * 60 * 60 * 1000, false});
  //  event_graph.event(event_t{cevent_colector::ekind::ev_power, _day_2+(24+1)
  //  * 60 * 60 * 1000, true});
  // clang-format on
  whole_report.finalize();
  // to include last period

  std::stringstream summary;
  summary<<event_graph.finalize()<<std::endl;

  const auto work_time = ts_cur - whole_report.ts_beg_;
  summary << "час роботи " << duration_to_str(work_time) << std::endl;
  summary << "було живлення " << duration_to_str(whole_report.power_time_)
          << "(" << (whole_report.power_time_ * 100 / work_time) << "%)"
          << std::endl;
  summary << "кількість вимкнень " << whole_report.power_outages_count_;
  if (whole_report.power_outages_count_) {
    summary << ", найдовше тривало "
            << duration_to_str(whole_report.power_outages_max_);
  }
  summary  << std::endl;
  return summary.str();
}

cevent_colector::cevent_colector()
    : impl_(std::make_unique<implementation>()) {}
cevent_colector::~cevent_colector() = default;

void cevent_colector::event(const ekind event, bool is_ok) {
  impl_->event(event, is_ok);
};

std::string cevent_colector::get_status(const ekind event) const {
  return impl_->get_status(event);
}

std::string cevent_colector::get_status() const {
  DBG_FUNK();
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

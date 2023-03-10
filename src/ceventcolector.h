/*
 * ceventcolector.h
 *
 *  Created on: Dec 24, 2022
 *      Author: oleksandr
 */

#ifndef SRC_CEVENTCOLECTOR_H_
#define SRC_CEVENTCOLECTOR_H_
#include <memory>
#include <string>

class cevent_colector {
 public:
  cevent_colector();
  ~cevent_colector();
  enum class ekind : uint8_t {
    ev_start,
    ev_tb_connectd,
    ev_power,
    ev_battery,
    ev_last
  };
  void event(const ekind kind, bool is_ok = true);
  std::string get_status(const ekind kind) const;
  std::string get_status() const;
  std::string get_summary() const;

 private:
  class implementation;
  std::unique_ptr<implementation> impl_;
};

#endif /* SRC_CEVENTCOLECTOR_H_ */

/*
 * ctelegram.h
 *
 *  Created on: Dec 17, 2022
 *      Author: oleksandr
 */

#pragma once
#include <AsyncTelegram2.h>  // https://github.com/cotestatnt/AsyncTelegram2
#include <stdint.h>

#include <functional>
#include <memory>
#include <string>

class ctelegram {
 private:
  class implementation;
  std::unique_ptr<implementation> impl_;

 public:
  ctelegram();
  ~ctelegram() noexcept;
  using cmd_handler_t = std::function<std::string(const TBMessage &msg)>;

  void setup(const char *tolken, const uint16_t interval);
  void add_cmd(std::string &&cmd, std::string &&description,
               cmd_handler_t &&handler);
  void notify(const std::string &&notice);
  bool start();
  void loop();
};

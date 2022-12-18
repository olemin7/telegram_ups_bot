/*
 * ctelegram.h
 *
 *  Created on: Dec 17, 2022
 *      Author: oleksandr
 */

#pragma once
#include <stdint.h>
#include <functional>
#include <string>

void telegram_setup(const char *tolken, const uint16_t interval,
                    std ::function<std::string()> &&status_fun);
void telegram_notify(const std::string &&notice);
bool telegram_start();
bool telegram_restart();
void telegram_loop();

/*
 * ctelegram.cpp
 *
 *  Created on: Dec 17, 2022
 *      Author: oleksandr
 */
// https://core.telegram.org/bots/api

#include <ArduinoJson.h>
#include <AsyncTelegram2.h>  // https://github.com/cotestatnt/AsyncTelegram2
#include <LittleFS.h>
#include <cledstatus.h>
#include <ctelegram.h>
#include <logs.h>
#include <misk.h>

#include <algorithm>
#include <functional>
#include <set>
#include <sstream>
#include <tuple>
#include <vector>

BearSSL::WiFiClientSecure client;
BearSSL::Session session;
BearSSL::X509List certificate(telegram_cert);
AsyncTelegram2 myBot(client);
constexpr auto MYTZ = "EET-2EEST,M3.5.0/3,M10.5.0/4";
std ::function<std::string()> get_host_status = nullptr;
constexpr auto JSON_FILE_SUBSCRIBERS = "/www/config/subscribers.json";
bool isStarted = false;

class ccmd_list {
  std::vector<std::tuple<std::string, std::string,
                         std::function<void(TBMessage &&msg)>>>
      cmd_list_;

 public:
  void add(std::string &&cmd, std::string &&description,
           std::function<void(TBMessage &&msg)> &&handler) {
    cmd_list_.emplace_back(cmd, description, handler);
  }
  void handle(TBMessage &&msg) const {
    if (msg.messageType != MessageText || cmd_list_.empty()) {
      return;
    }
    auto it =
        std::find_if(cmd_list_.cbegin(), cmd_list_.cend(), [&msg](auto &el) {
          return msg.text.equalsIgnoreCase(std::get<0>(el).c_str());
        });

    if (it != cmd_list_.end()) {
      std::get<2> (*it)(std::move(msg));

    } else {
      std::get<2>(cmd_list_.back())(std::move(msg));
    }
  }

  std::string to_json() {
    std::stringstream payload;
    payload << "{\"commands\":[";
    bool first = true;
    for (const auto &el : cmd_list_) {
      if (!first) {
        payload << ",";
      } else {
        first = false;
      }
      payload << "{\"command\":\"" << std::get<0>(el) << "\",";
      payload << "\"description\":\"" << std::get<1>(el) << "\"}";
    }
    payload << "]}";
    DBG_OUT << payload.str() << std::endl;
    return payload.str();
  }
  std::string to_str() {
    std::stringstream stream;
    for (const auto &el : cmd_list_) {
      stream << std::get<0>(el) << " - " << std::get<1>(el) << std::endl;
    }
    return stream.str();
  }
};

class csubscribes {
 private:
  std::set<int64_t> ids;
  const std::string persist_file_;
  static constexpr auto CAPACITY_ = 16;
  static constexpr auto JZON_CAPACITY_ = JSON_ARRAY_SIZE(CAPACITY_);
  void persist() {
    StaticJsonDocument<JZON_CAPACITY_> doc;
    JsonArray array = doc.to<JsonArray>();
    for (const auto &id : ids) {
      array.add(id);
    }
    auto jsonFile = LittleFS.open(persist_file_.c_str(), "w");
    if (!jsonFile) {
      DBG_OUT << "Failed to create file" << std::endl;
      return;
    }
    if (serializeJson(array, jsonFile) == 0) {
      DBG_OUT << "Failed to write file" << std::endl;
    }
    jsonFile.close();
  }

 public:
  csubscribes(const std::string &&file) : persist_file_(std::move(file)){};
  auto size() const { return ids.size(); }
  void init() {
    StaticJsonDocument<JZON_CAPACITY_> doc;
    auto jsonFile = LittleFS.open(persist_file_.c_str(), "r");
    if (!jsonFile) {
      DBG_OUT << "Failed to open file" << std::endl;
      return;
    }
    auto error = deserializeJson(doc, jsonFile);
    if (error) {
      DBG_OUT << "deserializeJson error=" << error.c_str() << std::endl;
      return;
    }
    JsonArray array = doc.as<JsonArray>();
    for (JsonVariant v : array) {
      ids.emplace(v.as<int64_t>());
    }
    DBG_OUT << "subscribers:" << ids.size() << std::endl;
  }
  void add(int64_t id) {
    const auto sz = ids.size();
    if (CAPACITY_ > sz) {
      ids.emplace(id);
      if (sz != ids.size()) {
        myBot.sendTo(id, "Subscribed");
        persist();
      } else {
        myBot.sendTo(id, "already");
      }
    } else {
      myBot.sendTo(id, "нема місця");
    }
  }

  void remove(int64_t id) {
    if (ids.erase(id)) {
      myBot.sendTo(id, "unSubscribed");
      persist();
    } else {
      myBot.sendTo(id, "wasn`t");
    }
  }
  bool isInList(int64_t id) const { return ids.find(id) != ids.cend(); }
  void notify(const std::string &&notice) const {
    for (const auto id : ids) {
      myBot.sendTo(id, notice.c_str());
    }
  }
};

csubscribes subscribes(JSON_FILE_SUBSCRIBERS);
ccmd_list cmd_list;

void send_welcome(TBMessage &&msg) {
  DBG_FUNK();
  std::stringstream stream;

  stream << "BOT " << myBot.getBotName() << std::endl;
  if (subscribes.isInList(msg.sender.id)) {
    stream << "підписані" << std::endl;
  }
  stream << "доступні команди" << std::endl;
  stream << cmd_list.to_str();

  myBot.sendMessage(msg, stream.str().c_str());
}

void send_status(TBMessage &&msg) {
  std::stringstream stream;
  stream << get_host_status();
  myBot.sendMessage(msg, stream.str().c_str());
}

void telegram_setup(const char *tolken, const uint16_t interval,
                    std ::function<std::string()> &&status_fun) {
  isStarted = false;
  get_host_status = move(status_fun);
  subscribes.init();
  // Sync time with NTP, to check properly Telegram certificate
  configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  // Set certficate, session and some other base client properies
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);
  myBot.setUpdateTime(interval);
  myBot.setTelegramToken(tolken);
  cmd_list.add("/start", "поточаток",
               [](auto msg) { send_welcome(std::move(msg)); });
  cmd_list.add("/status", "поточний стан",
               [](auto msg) { send_status(std::move(msg)); });
  cmd_list.add("/stat", "статистика", [](auto msg) {});
  cmd_list.add("/subscribe", "підписатись на зміну стану",
               [](auto msg) { subscribes.add(msg.sender.id); });
  cmd_list.add("/unsubscribe", "відписатись",
               [](auto msg) { subscribes.remove(msg.sender.id); });
  cmd_list.add("/help", "допомога",
               [](auto msg) { send_welcome(std::move(msg)); });
}

bool telegram_start() {
  DBG_FUNK();
  DBG_OUT << "Test Telegram connection... ";

  if (!myBot.begin()) {
    DBG_OUT << "NOK" << std::endl;
    isStarted = false;
    return false;
  }
  isStarted = true;
  DBG_OUT << "OK" << std::endl;

  myBot.setMyCommands(cmd_list.to_json().c_str());

  std::stringstream stream;
  stream << "Активувався" << std::endl;
  stream << get_host_status();
  telegram_notify(stream.str());
  return true;
}

bool telegram_restart() {
  DBG_OUT << "TB restart connection" << std::endl;
  myBot.reset();
  return telegram_start();
}

void telegram_notify(const std::string &&notice) {
  if (isStarted) {
    DBG_OUT << "notice=" << notice << std::endl;
    subscribes.notify(std::move(notice));
  } else {
    DBG_OUT << "tb is not connected" << std::endl;
  }
}

void telegram_loop() {
  if (!isStarted) {
    return;
  }
  // local variable to store telegram message data
  TBMessage msg;

  // if there is an incoming message...
  if (!myBot.getNewMessage(msg)) {
    return;
  }
  switch (msg.messageType) {
    case MessageText:
      DBG_OUT << "Text message received:\"" << msg.text << "\"" << std::endl;
      cmd_list.handle(std::move(msg));
      break;
    default:
      send_welcome(std::move(msg));
  }
}

/*
 * ctelegram.cpp
 *
 *  Created on: Dec 17, 2022
 *      Author: oleksandr
 */
// https://core.telegram.org/bots/api

#include <ArduinoJson.h>
#include <LittleFS.h>
#include <cledstatus.h>
#include <ctelegram.h>
#include <logs.h>
#include <misk.h>

#include <algorithm>
#include <functional>
#include <set>
#include <sstream>
#include <vector>

class ccmd_list {
  struct cmd_t {
    std::string cmd;
    std::string description;
    ctelegram::cmd_handler_t handler;
    uint8_t mask;
  };
  std::vector<cmd_t> cmd_list_;

 public:
  void add(std::string &&cmd, std::string &&description,
           ctelegram::cmd_handler_t &&handler, uint8_t mask = 0) {
    cmd_list_.emplace_back(cmd_t{cmd, description, handler, mask});
  }
  std::string handle(TBMessage &msg, uint8_t mask = 0) const {
    if (msg.messageType != MessageText || cmd_list_.empty()) {
      return "";
    }
    auto it = std::find_if(
        cmd_list_.cbegin(), cmd_list_.cend(),
        [&msg](auto &el) { return msg.text.equalsIgnoreCase(el.cmd.c_str()); });

    if (it != cmd_list_.end()) {
      return it->handler(msg);
    }
    return cmd_list_.begin()->handler(msg);
  }

  std::string to_json(uint8_t mask = 0) {
    std::stringstream payload;
    payload << "{\"commands\":[";
    bool first = true;
    for (const auto &el : cmd_list_) {
      if (!mask || mask & el.mask) {
        if (!first) {
          payload << ",";
        } else {
          first = false;
        }
        payload << "{\"command\":\"" << el.cmd << "\",";
        payload << "\"description\":\"" << el.description << "\"}";
      }
    }
    payload << "]}";
    DBG_OUT << payload.str() << std::endl;
    return payload.str();
  }
  std::string to_str(uint8_t mask = 0) {
    std::stringstream stream;
    for (const auto &el : cmd_list_) {
      if (!mask || mask & el.mask) {
        stream << el.cmd << " - " << el.description << std::endl;
      }
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
  const auto &get_ids() const { return ids; }
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
  std::string add(int64_t id) {
    const auto sz = ids.size();
    std::string result;
    if (CAPACITY_ > sz) {
      ids.emplace(id);
      if (sz != ids.size()) {
          result= "Subscribed";
        persist();
      } else {
          result= "already";
      }
    } else {
        result= "нема місця";
    }
    return result;
  }

  std::string  remove(int64_t id) {
      std::string result;
    if (ids.erase(id)) {
        result="відписались";
      persist();
    } else {
        result= "небули підписані";
    }
    return result;
  }
  bool isInList(int64_t id) const { return ids.find(id) != ids.cend(); }
};



class ctelegram::implementation {
 private:
  bool is_started_ = false;
  ccmd_list cmd_list_;
  csubscribes subscribes_;

  BearSSL::WiFiClientSecure client_;
  BearSSL::Session session_;
  BearSSL::X509List certificate_;
  AsyncTelegram2 my_bot_;

 public:
  implementation()
      : subscribes_("/www/config/subscribers.json"),
        certificate_(telegram_cert),
        my_bot_(client_) {}
  void setup(const char *tolken, const uint16_t interval) {
    is_started_ = false;
    subscribes_.init();
    // Sync time with NTP, to check properly Telegram certificate
    // Set certficate, session and some other base client properies
    client_.setSession(&session_);
    client_.setTrustAnchors(&certificate_);
    client_.setBufferSizes(1024, 1024);
    my_bot_.setUpdateTime(interval);
    my_bot_.setTelegramToken(tolken);
    add_cmd(
        "/help", "допомога", [this](auto msg) { return send_welcome(msg); },
        SHOW_IN_MENU);
    add_cmd("/subscribe", "підписатись на зміну стану",
                 [this](auto msg) { return subscribes_.add(msg.sender.id); });
    add_cmd("/unsubscribe", "відписатись", [this](auto msg) {
      return subscribes_.remove(msg.sender.id);
    });
  }
  void add_cmd(std::string &&cmd, std::string &&description,
               cmd_handler_t &&handler, uint8_t mask = 0);

  void notify(const std::string &&notice) {
    DBG_FUNK();
    if (is_started_) {
      DBG_OUT << "notice=" << notice << std::endl;
      for (const auto id : subscribes_.get_ids()) {
        my_bot_.sendTo(id, notice.c_str());
      }
    } else {
      DBG_OUT << "tb is not connected" << std::endl;
    }
  }
  bool start() {
    DBG_FUNK();
    //myBot.reset();
    DBG_OUT << "Test Telegram connection... ";

    if (!my_bot_.begin()) {
      DBG_OUT << "NOK" << std::endl;
      is_started_ = false;
      return false;
    }
    is_started_ = true;
    DBG_OUT << "OK" << std::endl;

    my_bot_.setMyCommands(cmd_list_.to_json(SHOW_IN_MENU).c_str());

    std::stringstream stream;
    stream << "Активувався" << std::endl;
    notify(stream.str());
    return true;
  }
  void stop() {
      is_started_=false;
  }
  void loop() {
    if (!is_started_) {
      return;
    }
    // local variable to store telegram message data
    TBMessage msg;

    // if there is an incoming message...
    if (!my_bot_.getNewMessage(msg)) {
      return;
    }
    switch (msg.messageType) {
      case MessageText:
        DBG_OUT << "Text message received:\"" << msg.text << "\"" << std::endl;
        {
        auto answer = cmd_list_.handle(msg);
        if (!answer.empty()) {
          my_bot_.sendMessage(msg, answer.c_str());
        }
        }
        break;
      default:
        my_bot_.sendMessage(msg, "/help -для допомоги");
    }
  }

  std::string send_welcome(TBMessage &msg) {
    DBG_FUNK();
    std::stringstream stream;

    stream << "BOT " << my_bot_.getBotName() << std::endl;
    if (subscribes_.isInList(msg.sender.id)) {
      stream << "підписані" << std::endl;
    }
    stream << "доступні команди" << std::endl;
    stream << cmd_list_.to_str();
    return stream.str();
  }
};
void ctelegram::implementation::add_cmd(std::string &&cmd,
                                        std::string &&description,
                                        cmd_handler_t &&handler, uint8_t mask) {
  cmd_list_.add(std::move(cmd), std::move(description), std::move(handler),mask);
}

//-----------------------------------
ctelegram::ctelegram():impl_(std::make_unique<implementation>())  {}
ctelegram::~ctelegram() noexcept=default;
void ctelegram::setup(const char *tolken, const uint16_t interval) {
  impl_->setup(tolken, interval);
}
void ctelegram::add_cmd(std::string &&cmd, std::string &&description,
                        cmd_handler_t &&handler, uint8_t mask) {
  impl_->add_cmd(std::move(cmd), std::move(description), std::move(handler),mask);
}
void ctelegram::notify(const std::string &&notice) {
  impl_->notify(std::move(notice));
}
bool ctelegram::start(){
    return impl_->start();
}

void ctelegram::stop(){
    impl_->stop();
}
void ctelegram::loop(){
    impl_->loop();
}


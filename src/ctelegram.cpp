/*
 * ctelegram.cpp
 *
 *  Created on: Dec 17, 2022
 *      Author: oleksandr
 */

#include <AsyncTelegram2.h>
#include <cledstatus.h>
#include <ctelegram.h>
#include <logs.h>
#include <misk.h>

#include <set>
#include <sstream>

BearSSL::WiFiClientSecure client;
BearSSL::Session session;
BearSSL::X509List certificate(telegram_cert);
AsyncTelegram2 myBot(client);
constexpr auto MYTZ = "EET-2EEST,M3.5.0/3,M10.5.0/4";
std ::function<std::string()> get_host_status = nullptr;

class csubscribes {
  std::set<int64_t> ids;

 public:
  auto size() const { return ids.size(); }
  void add(int64_t id) {
    ids.emplace(id);
    myBot.sendTo(id, "Subscribed");
  }
  void remove(int64_t id) {
    ids.erase(id);
    myBot.sendTo(id, "unSubscribed");
  }
  bool isInList(int64_t id) const { return ids.find(id) != ids.cend(); }
  void notify(const std::string &&notice) const {
    for (const auto id : ids) {
      myBot.sendTo(id, notice.c_str());
    }
  }
};
csubscribes subscribes;

void telegram_setup(const char *tolken, const uint16_t interval,
                    std ::function<std::string()> &&status_fun) {
  get_host_status = move(status_fun);
  // Sync time with NTP, to check properly Telegram certificate
  configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  // Set certficate, session and some other base client properies
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);
  myBot.setUpdateTime(interval);
  myBot.setTelegramToken(tolken);
}

bool telegram_start() {
  DBG_OUT << "Test Telegram connection... ";

  if (!myBot.begin()) {
    DBG_OUT << "NOK" << std::endl;
    return false;
  }
  DBG_OUT << "OK" << std::endl;
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

void send_welcome(TBMessage &&msg) {
  std::stringstream stream;

  stream << "BOT " << myBot.getBotName() << std::endl;
  if (subscribes.isInList(msg.sender.id)) {
    stream << "підписані" << std::endl;
  }
  stream << "/subscribe - підписатись на зміну стану\n"
            "/status - поточний стан\n"
            "/unsubscribe - відписатись";
  myBot.sendMessage(msg, stream.str().c_str());
}

void send_status(TBMessage &&msg) {
  std::stringstream stream;
  stream << get_host_status();
  myBot.sendMessage(msg, stream.str().c_str());
}

void telegram_notify(const std::string &&notice) {
  if (myBot.checkConnection()){
      DBG_OUT << "notice=" << notice << std::endl;

  subscribes.notify(std::move(notice));
  }else{
    DBG_OUT << "tb is not connected" << std::endl;
  }
}

void telegram_loop() {
  if (!myBot.checkConnection()) {
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
      if (msg.text.equalsIgnoreCase("/subscribe")) {
        subscribes.add(msg.sender.id);
        break;
      } else if (msg.text.equalsIgnoreCase("/status")) {
        send_status(std::move(msg));
        break;
      } else if (msg.text.equalsIgnoreCase("/unsubscribe")) {
        subscribes.remove(msg.sender.id);
        break;
      }
    default:
      send_welcome(std::move(msg));
  }
}

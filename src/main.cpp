// clang-format off
#include <Arduino.h>
#include <stdint.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <LittleFS.h>
#include <time.h>
#include <pgmspace.h>

#include <eventloop.h>
#include <logs.h>
#include <misk.h>
#include <CConfig.h>
#include <wifiHandle.h>
#include <cledstatus.h>
#include <ctelegram.h>
#include <CSignal.h>
#include <sstream>
#include <ceventcolector.h>
// clang-format on

using namespace std;

constexpr auto SERIAL_BAUND = 115200;
constexpr auto SERVER_PORT_WEB = 80;

constexpr auto AP_MODE_TIMEOUT = 30 * 1000;  // switch to ap if no wifi
constexpr auto AUTO_REBOOT_AFTER_AP_MODE =
    5 * 60 * 1000;  // switch to ap if no wifi

constexpr auto V220_PIN = D6;
constexpr auto LOW_BAT_PIN = D5;

const char* pDeviceName = nullptr;
ESP8266WebServer serverWeb(SERVER_PORT_WEB);
ESP8266HTTPUpdateServer otaUpdater;
CWifiStateSignal wifiStateSignal;
cevent_loop event_loop;
cled_status led_status;
constexpr auto DEBOUNCE_SIGNAL_MS = 3000;
SignalDebounceLoop<bool> power(DEBOUNCE_SIGNAL_MS,
                               []() { return digitalRead(V220_PIN) == 0; });
SignalDebounceLoop<bool> state_bat(DEBOUNCE_SIGNAL_MS, []() {
  return digitalRead(LOW_BAT_PIN) != 0;
});
cevent_colector event_colector;
ctelegram telegram;

auto config = CConfig<512>();
const char* update_path = "/firmware";
constexpr auto DEF_AP_PWD = "12345678";

constexpr auto MYTZ = "EET-2EEST,M3.5.0/3,M10.5.0/4";

te_ret get_about(ostream& out) {
  const auto rst = system_get_rst_info()->reason;
  out << "{";
  out << "\"firmware\":\"bot " << __DATE__ << " " << __TIME__ << "\"";
  out << ",\"deviceName\":\"" << pDeviceName << "\"";
  out << ",\"resetInfo\": \"" << rst << "(" << rst_reason_to_string(rst)
      << ")\"";
  out << "}";
  return er_ok;
}

te_ret get_status(ostream& out) {
  out << event_colector.get_status();
  return er_ok;
}

void setup_WebPages() {
  DBG_FUNK();
  otaUpdater.setup(&serverWeb, update_path, config.getCSTR("OTA_USERNAME"),
                   config.getCSTR("OTA_PASSWORD"));

  serverWeb.on("/restart", []() {
    webRetResult(serverWeb, er_ok);
    delay(1000);
    ESP.restart();
  });

  serverWeb.on("/about",
               [] { wifiHandle_send_content_json(serverWeb, get_about); });

  serverWeb.on("/status",
               [] { wifiHandle_send_content_json(serverWeb, get_status); });

  serverWeb.on("/filesave", []() {
    DBG_FUNK();
    if (!serverWeb.hasArg("path") || !serverWeb.hasArg("payload")) {
      webRetResult(serverWeb, er_no_parameters);
      return;
    }
    const auto path = string("/www/") + serverWeb.arg("path").c_str();
    cout << path << endl;
    auto file = LittleFS.open(path.c_str(), "w");
    if (!file) {
      webRetResult(serverWeb, er_createFile);
      return;
    }
    if (!file.print(serverWeb.arg("payload"))) {
      webRetResult(serverWeb, er_FileIO);
      return;
    }
    file.close();
    webRetResult(serverWeb, er_ok);
  });

  serverWeb.on("/scanwifi", HTTP_ANY,
               [&]() { wifiHandle_sendlist(serverWeb); });
  serverWeb.on("/connectwifi", HTTP_ANY,
               [&]() { wifiHandle_connect(pDeviceName, serverWeb, true); });

  serverWeb.on("/getlogs", HTTP_ANY, [&]() {
    serverWeb.send(200, "text/plain", log_buffer.c_str());
    log_buffer = "";
  });

  serverWeb.serveStatic("/", LittleFS, "/www/");

  serverWeb.onNotFound([] {
    Serial.println("Error no handler");
    Serial.println(serverWeb.uri());
    webRetResult(serverWeb, er_fileNotFound);
  });
  serverWeb.begin();
}

void setup_WIFIConnect() {
  DBG_FUNK();
  static int16_t to_ap_mode_thread = 0;
  static int16_t to_telegram_restart_thread = 0;
  WiFi.persistent(false);
  WiFi.hostname(pDeviceName);
  WiFi.begin();
  wifiStateSignal.onChange([](const wl_status_t& status) {
    if (WL_CONNECTED == status) {
      // skip AP mode
      event_loop.remove(to_ap_mode_thread);
      event_loop.remove(to_telegram_restart_thread);  // kill if present

      to_telegram_restart_thread = event_loop.set_interval(
          []() {
            if (telegram.start()) {
              event_loop.remove(to_telegram_restart_thread);  // started
              event_colector.event(cevent_colector::ekind::ev_tb_connectd,
                                   true);
            } else {
              event_colector.event(cevent_colector::ekind::ev_tb_connectd,
                                   false);
            }
          },
          15000, true);  // retry connection
    }
    led_status.set(cled_status::Work);
    wifi_status(DBG_OUT);
  });

  to_ap_mode_thread = event_loop.set_timeout( []() {
    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(pDeviceName, DEF_AP_PWD);
    DBG_OUT << "AP mode, name:" << pDeviceName << " ,pwd: " << DEF_AP_PWD
            << ",ip:" << WiFi.softAPIP().toString() << std::endl;

    event_loop.set_timeout( []() {
      DBG_OUT << "Rebooting" << std::endl;
      ESP.restart();
    },AUTO_REBOOT_AFTER_AP_MODE);
  },AP_MODE_TIMEOUT);

  if (WIFI_STA == WiFi.getMode()) {
    DBG_OUT << "connecting <" << WiFi.SSID() << "> " << endl;
    return;
  }
}

void setup_config() {
  config.getConfig().clear();
  config.getConfig()["DEVICE_NAME"] = "bot";
  config.getConfig()["OTA_USERNAME"] = "";
  config.getConfig()["OTA_PASSWORD"] = "";
  config.getConfig()["TELEGRAM_TOLKEN"] ="";
  config.getConfig()["TELEGRAM_UPDATE_TIME"] = 2000;

  if (!config.load("/www/config/config.json")) {
    // write file
    config.write("/www/config/config.json");
  }
  pDeviceName = config.getCSTR("DEVICE_NAME");
}

void setup() {
  pinMode(LOW_BAT_PIN, INPUT_PULLUP);
  pinMode(V220_PIN, INPUT_PULLUP);
  Serial.begin(SERIAL_BAUND);

  logs_begin();
  DBG_FUNK();
  hw_info(DBG_OUT);
  led_status.setup();
  event_loop.set_interval([]() { led_status.loop(); }, led_status.period_ms);
  led_status.set(cled_status::Warning);
  LittleFS.begin();
  LittleFS_info(DBG_OUT);
  setup_config();
  setup_WebPages();
  // Sync time with NTP, to check properly Telegram certificate
  configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  event_loop.set_interval([]() { time(nullptr); }, 1000, true); //to time sync

  telegram.setup(config.getCSTR("TELEGRAM_TOLKEN"),
                 config.getInt("TELEGRAM_UPDATE_TIME"));
  telegram.add_cmd(
      "/status", "стан системи",
      [](auto) { return event_colector.get_status(); }, SHOW_IN_MENU);

  telegram.add_cmd("/get_summary", "сумарний звіт",
                   [](auto) { return event_colector.get_summary(); });

  telegram.add_cmd("/sys_info", "системна інформація", [](auto msg) {
    std::stringstream sys_info;
    get_about(sys_info);
    sys_info << endl;
    sys_info << "sender.id:" << msg.sender.id << endl;
    wifi_status(sys_info);
    return sys_info.str();
  });
  
  power.onChange([](auto val) {
    event_colector.event(cevent_colector::ekind::ev_power, val);
    telegram.notify(
        event_colector.get_status(cevent_colector::ekind::ev_power));
  });
  state_bat.onChange([](auto val) {
    event_colector.event(cevent_colector::ekind::ev_battery, val);
    telegram.notify(
        event_colector.get_status(cevent_colector::ekind::ev_battery));
  });

  setup_WIFIConnect();
  event_colector.event(cevent_colector::ekind::ev_start);
  DBG_OUT << "Setup done" << endl;
}

void loop() {
  wifiStateSignal.loop();
  serverWeb.handleClient();
  event_loop.loop();
  telegram.loop();
  power.loop();
  state_bat.loop();
}

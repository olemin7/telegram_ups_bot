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
constexpr auto DEBOUNCE_SIGNAL_MS = 300;
SignalDebounceLoop<bool> state_220(DEBOUNCE_SIGNAL_MS,
                                   []() { return digitalRead(V220_PIN) != 0; });
SignalDebounceLoop<bool> state_bat(DEBOUNCE_SIGNAL_MS, []() {
  return digitalRead(LOW_BAT_PIN) != 0;
});

auto config = CConfig<512>();
const char* update_path = "/firmware";
constexpr auto DEF_AP_PWD = "12345678";

te_ret get_about(ostream& out) {
  out << "{";
  out << "\"firmware\":\"bot " << __DATE__ << " " << __TIME__ << "\"";
  out << ",\"deviceName\":\"" << pDeviceName << "\"";
  out << ",\"resetInfo\":" << system_get_rst_info()->reason;
  out << "}";
  return er_ok;
}

string get_state_220() {
  stringstream ss;
  ss << "\"стан 220\":\"";
  ss << (state_220.getLastValue() ? "відсутній" : "увімкнений") << "\"";
  return ss.str();
}

string get_state_bat() {
  stringstream ss;
  ss << "\"заряд батареї\":\"";
  ss << (state_bat.getLastValue() ? "нормальний" : "низький") << "\"";
  return ss.str();
}

te_ret get_status(ostream& out) {
  out << get_state_220();
  out << endl;
  out << get_state_bat();
  return er_ok;
}
string get_status_str() {
  stringstream ss;
  get_status(ss);
  return ss.str();
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
  WiFi.persistent(false);
  WiFi.hostname(pDeviceName);
  WiFi.begin();
  wifiStateSignal.onChange([](const wl_status_t& status) {
    if (WL_CONNECTED == status) {
      // skip AP mode
      event_loop.remove(to_ap_mode_thread);

      if (!telegram_restart()) {
        event_loop.set_timeout([]() { telegram_restart(); },
                               15000);  // retry connection
      }
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
  telegram_setup(config.getCSTR("TELEGRAM_TOLKEN"),
                 config.getInt("TELEGRAM_UPDATE_TIME"), get_status_str);
  state_220.onChange([](auto) { telegram_notify(get_state_220()); });
  state_bat.onChange([](auto) { telegram_notify(get_state_bat()); });

  setup_WIFIConnect();
  DBG_OUT << "Setup done" << endl;
}

void loop() {
  wifiStateSignal.loop();
  serverWeb.handleClient();
  event_loop.loop();
  telegram_loop();
  state_220.loop();
  state_bat.loop();
}

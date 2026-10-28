#include "cloud.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_http_client.h>
#include <vector>
#include "app.h"
#include "config.h"
#include "net.h"
#include "relay_cert.h"

namespace web {
String stateJson();
}

namespace {
constexpr const char* RELAY_URL = "https://2.28.47.87:8443/v1/sync";

struct Ack {
  int id;
  String result;
};

Preferences prefs;
String key;
SemaphoreHandle_t mu = xSemaphoreCreateMutex();
std::vector<String> inbox;   // raw JSON commands waiting for the main loop
std::vector<Ack> acks;       // results waiting for the next poll
int pendingMsgAck = -1;      // note on screen: ack when it has scrolled by
volatile uint32_t lastOk = 0;
String lastMsg = "null";

struct Locked {
  Locked() { xSemaphoreTake(mu, portMAX_DELAY); }
  ~Locked() { xSemaphoreGive(mu); }
};

String body() {
  JsonDocument doc;
  {
    app::Lock lock;
    char frame[201];
    app::frameHex(frame);
    doc["frame"] = frame;
  }
  JsonDocument st;
  deserializeJson(st, web::stateJson());
  st.remove("frame");
  doc["state"] = st;
  doc["fw"] = FW_NAME;
  {
    Locked l;
    JsonArray a = doc["acks"].to<JsonArray>();
    for (auto& k : acks) {
      JsonObject o = a.add<JsonObject>();
      o["id"] = k.id;
      o["result"] = k.result;
    }
    // Long-poll only when we have nothing to report, so results arrive quickly.
    doc["wait"] = acks.empty() ? 8 : 0;
  }
  String s;
  serializeJson(doc, s);
  return s;
}

bool poll() {
  String req = body();
  size_t sentAcks;
  {
    Locked l;
    sentAcks = acks.size();
  }
  esp_http_client_config_t cfg = {};
  cfg.url = RELAY_URL;
  cfg.method = HTTP_METHOD_POST;
  cfg.cert_pem = RELAY_CERT_PEM;
  cfg.skip_cert_common_name_check = true;  // the certificate itself is pinned
  cfg.timeout_ms = 15000;
  cfg.buffer_size = 2048;
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  if (!c) return false;
  esp_http_client_set_header(c, "Content-Type", "application/json");
  esp_http_client_set_header(c, "X-Okame-Key", key.c_str());
  bool ok = false;
  if (esp_http_client_open(c, req.length()) == ESP_OK &&
      esp_http_client_write(c, req.c_str(), req.length()) == (int)req.length()) {
    esp_http_client_fetch_headers(c);
    if (esp_http_client_get_status_code(c) == 200) {
      String resp;
      char buf[512];
      int n;
      while ((n = esp_http_client_read(c, buf, sizeof buf)) > 0 && resp.length() < 12000) resp.concat(buf, n);
      JsonDocument doc;
      if (!deserializeJson(doc, resp)) {
        ok = true;
        Locked l;
        acks.erase(acks.begin(), acks.begin() + sentAcks);
        for (JsonObject cmd : doc["commands"].as<JsonArray>()) {
          String s;
          serializeJson(cmd, s);
          inbox.push_back(s);
        }
      }
    } else {
      log_w("relay: HTTP %d", esp_http_client_get_status_code(c));
    }
  }
  esp_http_client_close(c);
  esp_http_client_cleanup(c);
  return ok;
}

void task(void*) {
  uint32_t backoff = 2000;
  for (;;) {
    if (key.length() != 64 || !net::online()) {
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }
    if (poll()) {
      lastOk = millis();
      backoff = 2000;
    } else {
      vTaskDelay(pdMS_TO_TICKS(backoff));
      backoff = min<uint32_t>(backoff * 2, 60000);
    }
  }
}

void ack(int id, const char* result) {
  Locked l;
  for (auto& a : acks)
    if (a.id == id) return;
  acks.push_back({id, result});
}

bool unhex(const char* hex, uint8_t* out, size_t n) {
  if (strlen(hex) != n * 2) return false;
  for (size_t i = 0; i < n; i++) {
    char b[3] = {hex[i * 2], hex[i * 2 + 1], 0};
    out[i] = strtoul(b, nullptr, 16);
  }
  return true;
}

void apply(JsonDocument& cmd) {
  int id = cmd["id"] | 0;
  String type = cmd["type"] | "";
  uint32_t now = millis();
  app::Lock lock;
  if (type == "act") {
    String a = cmd["action"] | "";
    Action act;
    if (a == "seed") act = Action::Seed;
    else if (a == "millet") act = Action::Millet;
    else if (a == "bath") act = Action::Bath;
    else if (a == "vet") act = Action::Vet;
    else if (a == "light") act = Action::Light;
    else if (a == "scritch") act = Action::Scritch;
    else return ack(id, "unknown");
    Result r = app::pet.act(act, app::epoch());
    app::ui.remoteAction(act, r, now);
    if (r == Result::Ok) app::requestSave();
    const char* names[] = {"ok", "refused", "not-needed", "sleeping"};
    ack(id, names[int(r)]);
  } else if (type == "msg") {
    String lcd = cmd["lcd"] | "";
    app::ui.showMessage(cmd["from"] | "", lcd.c_str(), now);
    JsonDocument m;
    m["from"] = cmd["from"] | "";
    m["text"] = cmd["text"] | "";
    m["translation"] = cmd["translation"] | "";
    m["at"] = app::epoch();
    lastMsg = "";
    serializeJson(m, lastMsg);
    prefs.putString("msg", lastMsg);
    pendingMsgAck = id;
  } else if (type == "pic") {
    static uint8_t frames[8][100];
    int n = 0;
    for (const char* f : cmd["frames"].as<JsonArray>())
      if (n < 8 && f && unhex(f, frames[n], 100)) n++;
    if (n) app::ui.showPicture(&frames[0][0], n, cmd["ms"] | 300, cmd["secs"] | 8, now);
    ack(id, n ? "shown" : "bad");
  } else if (type == "tune") {
    String t = cmd["notes"] | "";
    uint8_t notes[rules::TUNE_MAX];
    int n = 0;
    for (char ch : t)
      if (ch >= '0' && ch <= '4' && n < rules::TUNE_MAX) notes[n++] = ch - '0';
    if (n) {
      app::pet.setTune(notes, n);
      app::playTune();
      app::requestSave();
    }
    ack(id, n ? "ok" : "bad");
  } else if (type == "pair") {
    String code = cmd["code"] | "0";
    app::ui.showCode(code.toInt(), "TELEGRAM", now, 3 * 60 * 1000);
    ack(id, "shown");
  } else if (type == "pairdone") {
    if (app::ui.screen() == Screen::Code) app::ui.showCode(0, "", now, 0);
    app::ui.forceAnim(Anim::GameWin, now);
    ack(id, "ok");
  } else {
    ack(id, "unknown");
  }
}
}  // namespace

namespace cloud {

void begin() {
  prefs.begin("cloud", false);
  key = prefs.getString("key", "");
  lastMsg = prefs.getString("msg", "null");
  xTaskCreatePinnedToCore(task, "cloud", 10240, nullptr, 1, nullptr, 0);
}

void loop() {
  std::vector<String> todo;
  {
    Locked l;
    todo.swap(inbox);
  }
  for (auto& s : todo) {
    JsonDocument cmd;
    if (!deserializeJson(cmd, s)) apply(cmd);
  }
  if (pendingMsgAck >= 0) {
    bool done;
    {
      app::Lock lock;
      done = app::ui.messageDone();
    }
    if (done) {
      ack(pendingMsgAck, "shown");
      pendingMsgAck = -1;
    }
  }
}

bool connected() { return lastOk && millis() - lastOk < 60000; }

void setKey(const String& hex) {
  key = hex;
  prefs.putString("key", hex);
}

String lastMessageJson() { return lastMsg; }

}  // namespace cloud

#include "cloud.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_http_client.h>
#include <vector>
#include "app.h"
#include "config.h"
#include "net.h"
#include "relay_cert.h"
#include <mbedtls/x509_crt.h>
#include <esp_log.h>

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
std::vector<String> events;  // things that happened here, for the relay (cloud::event)
std::vector<String> shows;   // notes and pictures waiting their turn on the screen
int pendingMsgAck = -1;      // note on screen: ack when it has scrolled by
String pendingMsg;           // ... and the command itself, to replay it if something interrupts it
int seen[32];                // recent command ids, so a resent command is not applied twice
int seenAt = 0;
volatile uint32_t lastOk = 0, attempts = 0;
char lastErr[64] = "none";
String lastMsg = "null";
String lastHug = "null";
String wxJson = "null";  // {"kind","temp","place"} for the page

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
    JsonArray ev = doc["events"].to<JsonArray>();
    for (auto& e : events) {
      JsonDocument one;
      if (!deserializeJson(one, e)) ev.add(one);
    }
    // Long-poll only when we have nothing to report, so results arrive quickly.
    doc["wait"] = acks.empty() && events.empty() ? 8 : 0;
  }
  String s;
  serializeJson(doc, s);
  return s;
}

bool poll() {
  String req = body();
  size_t sentAcks, sentEvents;
  {
    Locked l;
    sentAcks = acks.size();
    sentEvents = events.size();
  }
  esp_http_client_config_t cfg = {};
  cfg.url = RELAY_URL;
  cfg.method = HTTP_METHOD_POST;
  cfg.cert_pem = RELAY_CERT_PEM;
  cfg.skip_cert_common_name_check = true;  // the certificate itself is pinned
  cfg.timeout_ms = 15000;
  cfg.buffer_size = 2048;
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  attempts++;
  if (!c) {
    strlcpy(lastErr, "client init failed", sizeof lastErr);
    return false;
  }
  esp_http_client_set_header(c, "Content-Type", "application/json");
  esp_http_client_set_header(c, "X-Okame-Key", key.c_str());
  bool ok = false;
  esp_err_t oe = esp_http_client_open(c, req.length());
  if (oe != ESP_OK) snprintf(lastErr, sizeof lastErr, "open: %s", esp_err_to_name(oe));
  if (oe == ESP_OK &&
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
        events.erase(events.begin(), events.begin() + min(sentEvents, events.size()));  // the cap may have dropped some
        for (JsonObject cmd : doc["commands"].as<JsonArray>()) {
          String s;
          serializeJson(cmd, s);
          inbox.push_back(s);
        }
      }
    } else {
      snprintf(lastErr, sizeof lastErr, "HTTP %d", esp_http_client_get_status_code(c));
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

bool seenBefore(int id) {
  for (int v : seen)
    if (v == id) return true;
  seen[seenAt++ % 32] = id;
  return false;
}

void apply(JsonDocument& cmd, const String& raw) {
  int id = cmd["id"] | 0;
  String type = cmd["type"] | "";
  uint32_t now = millis();
  if (type == "msg" || type == "pic" || type == "hug") {  // one thing on the screen at a time, in order
    shows.push_back(raw);
    return;
  }
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
  } else if (type == "home") {  // Matthieu's return day, from /home; y = 0 clears it
    PetExtra& x = app::pet.extraMut();
    x.home = {uint16_t(cmd["y"] | 0), uint8_t(cmd["m"] | 0), uint8_t(cmd["d"] | 0)};
    if (!x.home.m || !x.home.d) x.home = {};
    app::requestSave();
    ack(id, "ok");
  } else if (type == "word" || type == "forget") {  // /teach and /forget
    String text = cmd["text"] | "";
    text.trim();
    bool done = type == "word" ? app::pet.learnWord(text.c_str(), app::epoch()) : app::pet.forgetWord(text.c_str());
    if (done) app::requestSave();
    ack(id, done ? "ok" : type == "word" ? "known" : "unknown-word");
  } else if (type == "wx") {  // the weather where Haruki is, refreshed by the relay
    String k = cmd["kind"] | "clear";
    Weather w = k == "rain" ? Weather::Rain : k == "snow" ? Weather::Snow : k == "hot" ? Weather::Hot
              : k == "cold" ? Weather::Cold : Weather::Clear;
    app::ui.setWeather(w, now);
    wxJson = "";
    JsonDocument j;
    j["kind"] = k;
    j["temp"] = cmd["temp"] | 0;
    j["place"] = cmd["place"] | "";
    serializeJson(j, wxJson);
    ack(id, "ok");
  } else if (type == "dates") {  // private dates (her birthday) live on the relay, never in the public code
    PetExtra& x = app::pet.extraMut();
    memset(x.dates, 0, sizeof x.dates);
    int i = 0;
    for (JsonObject d : cmd["list"].as<JsonArray>()) {
      if (i >= PetExtra::DATES) break;
      x.dates[i++] = {uint8_t(d["m"] | 0), uint8_t(d["d"] | 0), uint8_t(d["kind"] | 0)};
    }
    snprintf(x.who, sizeof x.who, "%s", (const char*)(cmd["who"] | ""));
    app::requestSave();
    ack(id, "ok");
  } else if (type == "rival") {  // Matthieu's best Feather Catch score, for Seed Rain
    app::pet.extraMut().rivalRain = cmd["score"] | 0;
    app::requestSave();
    ack(id, "ok");
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
// Put the next note or picture on the screen. Called with the app lock held, when the screen is free.
void show(const String& raw) {
  JsonDocument cmd;
  if (deserializeJson(cmd, raw)) return;
  int id = cmd["id"] | 0;
  uint32_t now = millis();
  if (String(cmd["type"] | "") == "msg") {
    String lcd = cmd["lcd"] | "";
    app::ui.showMessage(cmd["from"] | "", lcd.c_str(), now);
    app::ui.messageDone();  // clear a stale flag from an earlier note
    JsonDocument m;
    m["from"] = cmd["from"] | "";
    m["text"] = cmd["text"] | "";
    m["translation"] = cmd["translation"] | "";
    m["at"] = app::epoch();
    lastMsg = "";
    serializeJson(m, lastMsg);
    prefs.putString("msg", lastMsg);
    pendingMsgAck = id;
    pendingMsg = raw;
  } else if (String(cmd["type"] | "") == "hug") {
    String from = cmd["from"] | "";
    app::ui.showHug(from.c_str(), now);
    JsonDocument h;
    h["from"] = from;
    h["at"] = app::epoch();
    lastHug = "";
    serializeJson(h, lastHug);
    prefs.putString("hug", lastHug);
    ack(id, "shown");
  } else {
    static uint8_t frames[8][100];
    int n = 0;
    for (const char* f : cmd["frames"].as<JsonArray>())
      if (n < 8 && f && unhex(f, frames[n], 100)) n++;
    if (n) app::ui.showPicture(&frames[0][0], n, cmd["ms"] | 300, cmd["secs"] | 8, now);
    ack(id, n ? "shown" : "bad");
  }
}

// Free for a note: awake on the main screens, nothing else showing.
bool screenFree() {
  Screen s = app::ui.screen();
  return s == Screen::Main || s == Screen::Lights;
}
}  // namespace

namespace cloud {

void begin() {
  {  // Check the pinned certificate parses (diagnostics).
    mbedtls_x509_crt crt;
    mbedtls_x509_crt_init(&crt);
    int ret = mbedtls_x509_crt_parse(&crt, (const unsigned char*)RELAY_CERT_PEM, strlen(RELAY_CERT_PEM) + 1);
    snprintf(lastErr, sizeof lastErr, "cert parse %d, ca=%d", ret, crt.private_ca_istrue);
    mbedtls_x509_crt_free(&crt);
  }
  for (int& v : seen) v = -1;
  prefs.begin("cloud", false);
  key = prefs.getString("key", "");
  lastMsg = prefs.getString("msg", "null");
  lastHug = prefs.getString("hug", "null");
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
    if (!deserializeJson(cmd, s) && !seenBefore(cmd["id"] | 0)) apply(cmd, s);
  }
  app::Lock lock;
  if (pendingMsgAck >= 0 && app::ui.screen() != Screen::Message) {
    if (app::ui.messageDone()) {  // scrolled all the way through
      ack(pendingMsgAck, "shown");
      pendingMsgAck = -1;
      pendingMsg = "";
    } else {  // pushed off the screen (sleep, pairing, a game...): play it again later
      shows.insert(shows.begin(), pendingMsg);
      pendingMsg = "";
      pendingMsgAck = -1;
    }
  }
  if (pendingMsgAck < 0 && !shows.empty() && screenFree()) {
    String next = shows.front();
    shows.erase(shows.begin());
    show(next);
  }
}

bool connected() { return lastOk && millis() - lastOk < 60000; }

String status() {
  return String("key=") + key.length() + " online=" + net::online() + " attempts=" + attempts +
         " lastOk=" + (lastOk ? (millis() - lastOk) / 1000 : -1) + "s ago err=" + lastErr;
}

void setKey(const String& hex) {
  key = hex;
  prefs.putString("key", hex);
}

String lastMessageJson() { return lastMsg; }

String lastHugJson() { return lastHug; }

String weatherJson() { return wxJson; }

void event(const String& json) {
  Locked l;
  if (events.size() >= 16) events.erase(events.begin());
  events.push_back(json);
}

}  // namespace cloud

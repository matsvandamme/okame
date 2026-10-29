#include "web.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include "app.h"
#include "config.h"
#include "net.h"
#include "ota.h"
#include "web_assets.h"
#include <esp_random.h>
#include "cloud.h"

namespace {
AsyncWebServer server(80);
AsyncEventSource events("/api/events");
char lastFrame[201] = "";
uint32_t nextFrame = 0, nextState = 0;

const char* stageName(Stage s) {
  switch (s) {
    case Stage::Egg: return "egg";
    case Stage::Chick: return "chick";
    case Stage::Juvenile: return "juvenile";
    case Stage::Adult: return "adult";
    default: return "gone";
  }
}
const char* variantName(Variant v) {
  switch (v) {
    case Variant::Pearl: return "pearl";
    case Variant::Pied: return "pied";
    case Variant::Scruffy: return "scruffy";
    default: return "grey";
  }
}
const char* moodName(Mood m) {
  switch (m) {
    case Mood::Content: return "content";
    case Mood::Meh: return "meh";
    case Mood::Sad: return "sad";
    case Mood::Sick: return "sick";
    default: return "asleep";
  }
}
const char* resultName(Result r) {
  switch (r) {
    case Result::Ok: return "ok";
    case Result::Refused: return "refused";
    case Result::NotNeeded: return "not-needed";
    default: return "sleeping";
  }
}

String jsonString(const char* s) {
  String o = "\"";
  for (; *s; s++) {
    char c = *s;
    if (c == '"' || c == '\\') o += '\\', o += c;
    else if ((uint8_t)c < 0x20) o += ' ';
    else o += c;
  }
  return o + "\"";
}

// Game status for the phone's controls (called with the lock held).
String gameJson() {
  Ui& ui = app::ui;
  if (ui.screen() == Screen::Duet) {
    const Duet& d = ui.duet();
    const char* ph[] = {"showing", "listening", "won", "lost", "done"};
    // While the bird whistles, the page gets the notes so the phone can whistle along.
    String notes;
    if (d.phase() == Duet::Phase::Showing)
      for (int i = 0; i < d.length(); i++) notes += d.note(i) ? 'L' : 'S';
    return String("{\"type\":\"duet\",\"phase\":\"") + ph[int(d.phase())] + "\",\"round\":" + d.round() +
           ",\"rounds\":" + Duet::ROUNDS + ",\"wins\":" + d.wins() + ",\"length\":" + d.length() +
           ",\"entered\":" + d.entered() + ",\"showing\":" + d.showing() + ",\"notes\":\"" + notes + "\"}";
  }
  if (ui.screen() == Screen::Rain) {
    const SeedRain& r = ui.rain();
    return String("{\"type\":\"rain\",\"phase\":\"playing\",\"score\":") + r.score() + ",\"lives\":" + r.lives() +
           ",\"best\":" + app::pet.state().bestRain + "}";
  }
  if (ui.screen() == Screen::Janken) {
    const Janken& j = ui.janken();
    const char* ph[] = {"chant", "reveal", "done"};
    const char* hands[] = {"rock", "scissors", "paper"};
    String last = j.phase() == Janken::Phase::Reveal ? String(",\"you\":\"") + hands[j.you()] + "\",\"bird\":\"" +
                                                           hands[j.bird()] + "\",\"result\":" + j.last()
                                                     : "";
    return String("{\"type\":\"janken\",\"phase\":\"") + ph[int(j.phase())] + "\",\"wins\":" + j.wins() +
           ",\"losses\":" + j.losses() + last + "}";
  }
  if (ui.screen() == Screen::Dance) {
    const Dance& d = ui.dance();
    const char* ph[] = {"showing", "listening", "good", "fail", "done"};
    return String("{\"type\":\"dance\",\"phase\":\"") + ph[int(d.phase())] + "\",\"length\":" + d.length() +
           ",\"entered\":" + d.entered() + ",\"best\":" + app::pet.extra().bestDance + "}";
  }
  if (ui.screen() == Screen::Peek) {
    const Peekaboo& p = ui.peek();
    const char* ph[] = {"hidden", "peeking", "hit", "miss", "done"};
    return String("{\"type\":\"peek\",\"phase\":\"") + ph[int(p.phase())] + "\",\"round\":" + p.round() +
           ",\"rounds\":" + Peekaboo::ROUNDS + ",\"hits\":" + p.hits() + "}";
  }
  return "null";
}

}  // namespace

// Also used by the relay link (cloud.cpp).
namespace web {
String stateJson();
}
String web::stateJson() {
  app::Lock lock;
  uint32_t now = app::epoch();
  const PetState& s = app::pet.state();
  char frame[201];
  app::frameHex(frame);
  String j;
  j.reserve(900);
  j += "{\"name\":" + jsonString(s.name);
  j += ",\"stage\":\"" + String(stageName(s.stage)) + "\"";
  j += ",\"variant\":\"" + String(variantName(s.variant)) + "\"";
  j += ",\"hunger\":" + String(s.hunger) + ",\"happy\":" + String(s.happy);
  j += ",\"poop\":" + String(s.poop) + ",\"sick\":" + String(s.sick ? "true" : "false");
  j += ",\"lightsOff\":" + String(s.lightsOff ? "true" : "false");
  j += ",\"weight\":" + String(s.weight);
  j += ",\"ageDays\":" + String(app::pet.ageSeconds(now) / 86400);
  j += ",\"mood\":\"" + String(moodName(app::pet.mood(now))) + "\"";
  j += ",\"attention\":" + String(app::pet.needsAttention(now) ? "true" : "false");
  j += ",\"tune\":\"";
  for (int i = 0; i < s.tuneLen; i++) j += char('0' + s.tune[i]);
  j += "\",\"tuneLevel\":" + String(s.tuneLevel);
  j += ",\"night\":" + String(app::pet.isNight(now) ? "true" : "false");
  j += ",\"setup\":" + String(net::apActive() ? "true" : "false");
  j += ",\"host\":" + jsonString(net::hostname().c_str());
  j += ",\"fw\":\"" FW_NAME "\"";
  j += ",\"game\":" + gameJson();
  j += ",\"msg\":" + cloud::lastMessageJson();
  j += ",\"hug\":" + cloud::lastHugJson();
  j += ",\"homeDays\":" + String(app::ui.daysToHome(app::today()));
  j += ",\"wx\":" + cloud::weatherJson();
  {
    static const char* const OCC[] = {"", "hina", "sakura", "kodomo", "tanabata", "halloween", "sinterklaas",
                                      "christmas", "newyear", "birthday", "anniversary", "home", "yearend"};
    Date d = app::today();
    j += ",\"occasion\":\"" + String(OCC[int(occasion(d, app::pet.extra()))]) + "\"";
  }
  j += ",\"words\":" + String(app::pet.wordCount()) + ",\"wordList\":[";
  {
    bool firstWord = true;
    for (const auto& wd : app::pet.extra().words) {
      if (!wd.text[0]) continue;
      j += String(firstWord ? "" : ",") + "{\"t\":" + jsonString(wd.text) + ",\"learning\":" +
           (now < wd.at + rules::DAY ? "true" : "false") + "}";
      firstWord = false;
    }
  }
  j += "]";
  {
    const PetExtra& x = app::pet.extra();
    Date d = app::today();
    bool drawn = d.y && x.omikujiDay == civil(d);
    j += ",\"omikuji\":{\"today\":" + String(drawn ? x.omikujiGrade : -1) + ",\"seen\":" + String(x.omikujiSeen) +
         ",\"day\":" + String(d.y ? civil(d) : 0) + "}";
    j += ",\"bestDance\":" + String(x.bestDance) + ",\"jankenWins\":" + String(x.jankenWins);
    j += ",\"rivalRain\":" + String(x.rivalRain);
    j += ",\"focusLeft\":" + String(app::ui.focusLeftSec(millis())) + ",\"focusCount\":" + String(x.focusCount);
  }
  j += ",\"page\":\"" WEB_PAGE_HASH "\"";
  j += ",\"hat\":" + String(s.hat) + ",\"unlocked\":" + String(s.unlocked);
  j += ",\"giftReady\":" + String(app::pet.giftReady(now) ? "true" : "false");
  j += ",\"bestRain\":" + String(s.bestRain);
  j += ",\"frame\":\"" + String(frame) + "\"}";
  return j;
}

namespace {

bool viaSetupWifi(AsyncWebServerRequest* r) {
  return net::apActive() && r->client()->localIP() == WiFi.softAPIP();
}

// Settings need a pairing session, except over the setup Wi-Fi (its password came from the LCD).
bool authorized(AsyncWebServerRequest* r) {
  if (viaSetupWifi(r)) return true;
  const AsyncWebHeader* h = r->getHeader("X-Okame-Token");
  return h && app::sessionValid(h->value());
}

String param(AsyncWebServerRequest* r, const char* name) {
  const AsyncWebParameter* p = r->getParam(name, true);
  return p ? p->value() : String();
}

void sendJson(AsyncWebServerRequest* r, int code, const String& body) {
  AsyncWebServerResponse* res = r->beginResponse(code, "application/json", body);
  res->addHeader("Cache-Control", "no-store");
  r->send(res);
}

void denied(AsyncWebServerRequest* r) { sendJson(r, 401, "{\"error\":\"pair first\"}"); }

void handleAct(AsyncWebServerRequest* r) {
  String a = param(r, "a");
  Action act;
  if (a == "seed") act = Action::Seed;
  else if (a == "millet") act = Action::Millet;
  else if (a == "bath") act = Action::Bath;
  else if (a == "vet") act = Action::Vet;
  else if (a == "light") act = Action::Light;
  else if (a == "scritch") act = Action::Scritch;
  else return sendJson(r, 400, "{\"error\":\"unknown action\"}");
  Result res;
  {
    app::Lock lock;
    res = app::pet.act(act, app::epoch());
    app::ui.remoteAction(act, res, millis());
  }
  if (res == Result::Ok) app::requestSave();
  sendJson(r, 200, String("{\"result\":\"") + resultName(res) + "\"}");
}

void handleName(AsyncWebServerRequest* r) {
  String n = param(r, "name");
  n.trim();
  if (n.isEmpty() || n.length() > 16) return sendJson(r, 400, "{\"error\":\"1-16 characters\"}");
  for (char c : n)
    if ((uint8_t)c < 0x20 || c == '<' || c == '>') return sendJson(r, 400, "{\"error\":\"letters please\"}");
  {
    app::Lock lock;
    // The first name is free; renaming later is a setting.
    if (app::pet.state().name[0] && !authorized(r)) return denied(r);
    app::pet.setName(n.c_str());
  }
  app::requestSave();  // the page stays at okame.local (printed as a QR code in the booklet)
  sendJson(r, 200, "{\"host\":" + jsonString(net::hostname().c_str()) + "}");
}

void handleTune(AsyncWebServerRequest* r) {
  String t = param(r, "notes");
  if (t.isEmpty() || t.length() > rules::TUNE_MAX) return sendJson(r, 400, "{\"error\":\"1-8 notes\"}");
  uint8_t notes[rules::TUNE_MAX];
  for (size_t i = 0; i < t.length(); i++) {
    if (t[i] < '0' || t[i] > '4') return sendJson(r, 400, "{\"error\":\"bad note\"}");
    notes[i] = t[i] - '0';
  }
  {
    app::Lock lock;
    app::pet.setTune(notes, t.length());
  }
  app::requestSave();
  sendJson(r, 200, "{\"ok\":true}");
}

void handlePractice(AsyncWebServerRequest* r) {
  bool learned;
  uint8_t level;
  {
    app::Lock lock;
    learned = app::pet.practiceTune(app::epoch());
    level = app::pet.state().tuneLevel;
    if (learned) app::ui.whistle(millis());
  }
  if (learned) app::requestSave();
  sendJson(r, 200, String("{\"learned\":") + (learned ? "true" : "false") + ",\"level\":" + level + "}");
}

void handleGame(AsyncWebServerRequest* r) {
  String g = param(r, "g");
  Ui::Game game;
  if (g == "duet") game = Ui::Game::Duet;
  else if (g == "peek") game = Ui::Game::Peek;
  else if (g == "rain") game = Ui::Game::Rain;
  else if (g == "janken") game = Ui::Game::Janken;
  else if (g == "dance") game = Ui::Game::Dance;
  else return sendJson(r, 400, "{\"error\":\"unknown game\"}");
  bool ok;
  {
    app::Lock lock;
    ok = app::ui.startGame(game, millis());
  }
  sendJson(r, ok ? 200 : 409, ok ? "{\"ok\":true}" : "{\"error\":\"not now\"}");
}

void handleGameInput(AsyncWebServerRequest* r) {
  String p = param(r, "p");
  {
    app::Lock lock;
    if (!app::ui.inGame()) return sendJson(r, 409, "{\"error\":\"no game\"}");
    Press k = p == "long" ? Press::Long : p == "left" ? Press::Left : p == "right" ? Press::Right
            : p == "hop" ? Press::Up : p == "bow" ? Press::Down : p == "rock" ? Press::One
            : p == "scissors" ? Press::Two : p == "paper" ? Press::Three : Press::Short;
    app::ui.gameInput(k, millis());
  }
  sendJson(r, 200, "{\"game\":" + [] { app::Lock lock; return gameJson(); }() + "}");
}

void handleGift(AsyncWebServerRequest* r) {
  int got;
  {
    app::Lock lock;
    got = app::pet.openGift(app::epoch(), esp_random());
    if (got > 0) app::pet.wear(got);  // put it on straight away
    if (got >= -1) app::ui.showGift(got, millis());
  }
  if (got < -1) return sendJson(r, 409, "{\"error\":\"come back tomorrow\"}");
  app::requestSave();
  sendJson(r, 200, String("{\"gift\":") + got + "}");
}

// Today's omikuji: once per local day.
void handleOmikuji(AsyncWebServerRequest* r) {
  int g;
  {
    app::Lock lock;
    Date d = app::today();
    Screen s = app::ui.screen();
    if (!d.y) return sendJson(r, 409, "{\"error\":\"no clock yet\"}");
    if ((s != Screen::Main && s != Screen::Anim) || app::ui.onceAnim()) return sendJson(r, 409, "{\"error\":\"not now\"}");
    g = app::pet.drawOmikuji(civil(d), esp_random());
    if (g >= 0) app::ui.showOmikuji(g, millis());
  }
  if (g < 0) return sendJson(r, 409, "{\"error\":\"already drawn today\"}");
  app::requestSave();
  cloud::event(String("{\"type\":\"omikuji\",\"grade\":") + g + "}");
  sendJson(r, 200, String("{\"grade\":") + g + "}");
}

// Tap a word on the page and the bird says it now.
void handleSay(AsyncWebServerRequest* r) {
  String t = param(r, "w");
  {
    app::Lock lock;
    if (app::ui.screen() != Screen::Main) return sendJson(r, 409, "{\"error\":\"not now\"}");
    for (const auto& wd : app::pet.extra().words)
      if (wd.text[0] && t == wd.text) {
        app::ui.say(wd.text, app::epoch() < wd.at + rules::DAY, millis());
        return sendJson(r, 200, "{\"ok\":true}");
      }
  }
  sendJson(r, 404, "{\"error\":\"unknown word\"}");
}

// Focus buddy: min=25 or 50 starts a session, min=0 stops it.
void handleFocus(AsyncWebServerRequest* r) {
  int m = param(r, "min").toInt();
  bool ok = true;
  {
    app::Lock lock;
    if (m <= 0) app::ui.stopFocus(millis());
    else ok = app::ui.startFocus(m, millis());
  }
  sendJson(r, ok ? 200 : 409, ok ? "{\"ok\":true}" : "{\"error\":\"not now\"}");
}

// The 💛 button: hearts on the screen here, and Matthieu hears about it on Telegram.
void handleLove(AsyncWebServerRequest* r) {
  static uint32_t last = 0;
  if (last && millis() - last < 60000) return sendJson(r, 429, "{\"error\":\"wait a little\"}");  // as the relay
  last = millis();
  {
    app::Lock lock;
    if (!app::pet.asleep(app::epoch())) app::ui.love(millis());
  }
  cloud::event("{\"type\":\"love\"}");
  sendJson(r, 200, "{\"ok\":true}");
}

void handleWear(AsyncWebServerRequest* r) {
  bool ok;
  {
    app::Lock lock;
    ok = app::pet.wear(param(r, "hat").toInt());
  }
  if (ok) app::requestSave();
  sendJson(r, ok ? 200 : 400, ok ? "{\"ok\":true}" : "{\"error\":\"not unlocked yet\"}");
}

void handlePair(AsyncWebServerRequest* r) {
  int left = 0;
  String token = app::tryPair(param(r, "code"), left);
  if (token.isEmpty()) return sendJson(r, 403, String("{\"error\":\"wrong code\",\"triesLeft\":") + left + "}");
  sendJson(r, 200, "{\"token\":\"" + token + "\"}");
}

void handleSettings(AsyncWebServerRequest* r) {
  if (!authorized(r)) return denied(r);
  SavedNetwork nets[MAX_NETWORKS];
  int n = net::networks(nets);
  String j = "{\"networks\":[";
  for (int i = 0; i < n; i++) j += (i ? "," : "") + jsonString(nets[i].ssid);  // never the passwords
  j += "],\"fw\":\"" FW_NAME "\",\"ota\":" + jsonString(ota::status());
  j += ",\"ap\":" + jsonString(net::apSsid()) + "}";
  sendJson(r, 200, j);
}

void sendIndex(AsyncWebServerRequest* r) {
  AsyncWebServerResponse* res = r->beginResponse(200, "text/html", WEB_INDEX_GZ, sizeof WEB_INDEX_GZ);
  res->addHeader("Content-Encoding", "gzip");
  res->addHeader("Cache-Control", "no-cache");
  res->addHeader("Content-Security-Policy",
                 "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; img-src 'self' data: blob:; media-src 'self' data:");
  r->send(res);
}
}  // namespace

namespace web {

void begin() {
  server.on("/", HTTP_GET, sendIndex);
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest* r) { sendJson(r, 200, web::stateJson()); });
  server.on("/api/act", HTTP_POST, handleAct);
  server.on("/api/name", HTTP_POST, handleName);
  server.on("/api/tune", HTTP_POST, handleTune);
  server.on("/api/practice", HTTP_POST, handlePractice);
  server.on("/api/pair", HTTP_POST, handlePair);
  server.on("/api/pair-request", HTTP_POST, [](AsyncWebServerRequest* r) {
    bool ok = app::requestPair();
    sendJson(r, ok ? 200 : 429, ok ? "{\"ok\":true}" : "{\"error\":\"wait a few seconds\"}");
  });
  server.on("/api/game-start", HTTP_POST, handleGame);
  server.on("/api/game-input", HTTP_POST, handleGameInput);
  server.on("/api/gift", HTTP_POST, handleGift);
  server.on("/api/wear", HTTP_POST, handleWear);
  server.on("/api/love", HTTP_POST, handleLove);
  server.on("/api/omikuji", HTTP_POST, handleOmikuji);
  server.on("/api/say", HTTP_POST, handleSay);
  server.on("/api/focus", HTTP_POST, handleFocus);
  server.on("/api/settings", HTTP_GET, handleSettings);
  server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* r) {
    if (!authorized(r)) return denied(r);
    sendJson(r, 200, net::scanJson());
  });
  server.on("/api/networks/add", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authorized(r)) return denied(r);
    bool ok = net::addNetwork(param(r, "ssid").c_str(), param(r, "pass").c_str());
    sendJson(r, ok ? 200 : 400, ok ? "{\"ok\":true}" : "{\"error\":\"could not save (max 5)\"}");
  });
  server.on("/api/networks/remove", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authorized(r)) return denied(r);
    sendJson(r, 200, net::removeNetwork(param(r, "ssid").c_str()) ? "{\"ok\":true}" : "{\"error\":\"not found\"}");
  });
  server.on("/api/new-egg", HTTP_POST, [](AsyncWebServerRequest* r) {
    {
      app::Lock lock;
      // Starting over after the bird flew away needs no code; otherwise it's a setting.
      if (app::pet.state().stage != Stage::Gone && !authorized(r)) return denied(r);
      app::pet.reset(app::epoch());
    }
    app::requestSave();
    sendJson(r, 200, "{\"ok\":true}");
  });
  server.on("/api/ota/check", HTTP_POST, [](AsyncWebServerRequest* r) {
    if (!authorized(r)) return denied(r);
    ota::checkNow();
    sendJson(r, 200, "{\"ok\":true}");
  });
  server.addHandler(&events);
  // Captive portal: phones probing for internet on the setup Wi-Fi land on the page.
  server.onNotFound([](AsyncWebServerRequest* r) {
    if (viaSetupWifi(r)) return r->redirect("http://" + WiFi.softAPIP().toString() + "/");
    r->send(404, "text/plain", "not found");
  });
  server.begin();
}

// Push the screen to open pages whenever it changes, and the stats every few seconds.
void loop() {
  uint32_t now = millis();
  if (events.count() == 0 || now < nextFrame) return;
  nextFrame = now + 150;
  char frame[201];
  {
    app::Lock lock;
    app::frameHex(frame);
  }
  if (strcmp(frame, lastFrame)) {
    strcpy(lastFrame, frame);
    events.send(frame, "frame");
  }
  if (app::takeTuneEvent()) {
    String notes;
    {
      app::Lock lock;
      const PetState& s = app::pet.state();
      for (int i = 0; i < s.tuneLen; i++) notes += char('0' + s.tune[i]);
    }
    events.send(notes.c_str(), "tune");
  }
  if (now >= nextState) {
    nextState = now + 3000;
    events.send(web::stateJson().c_str(), "state");
  }
}

}  // namespace web

// Okame: a cockatiel pocket pet for the T-Dongle-S3 clone.
#include <Arduino.h>
#include <esp_random.h>
#include <esp_timer.h>
#include <time.h>
#include "app.h"
#include "config.h"
#include "display.h"
#include "net.h"
#include "ota.h"
#include "render.h"
#include "storage.h"
#include "web.h"
#include "cloud.h"

static int localHour(uint32_t epoch) {
  time_t t = epoch;
  struct tm tm;
  localtime_r(&t, &tm);
  return tm.tm_hour;
}

static Date localDate(uint32_t epoch) {
  time_t t = epoch;
  struct tm tm;
  localtime_r(&t, &tm);
  return {uint16_t(tm.tm_year + 1900), uint8_t(tm.tm_mon + 1), uint8_t(tm.tm_mday)};
}

namespace app {
Pet pet(localHour);
Ui ui(pet, esp_random());
SemaphoreHandle_t mutex = xSemaphoreCreateRecursiveMutex();
}  // namespace app

namespace {
Display lcd;
uint16_t pixels[PX_W * PX_H];
uint8_t lastPacked[LCD_W * LCD_H / 8];
Env env;

uint32_t bootEpoch = 0;  // last known time at boot, used until the internet clock arrives
bool bootReal = false;   // bootEpoch came from a real clock (saved earlier), not a placeholder
bool clockTrusted() { return bootReal || net::timeValid(); }
volatile bool saveWanted = false;
uint32_t lastSave = 0, lastTick = 0, nextWhistle = 0;

uint16_t pairCode = 0;
uint32_t pairUntil = 0, pairLockUntil = 0;
int pairTries = 0;

bool before(uint32_t deadline) { return (int32_t)(millis() - deadline) < 0; }  // wrap-safe
String sessionToken;
uint32_t sessionUntil = 0;
bool tuneEvent = false;

void backlight(uint8_t level) { ledcWrite(PIN_LCD_BL, level); }

void save() {
  app::Lock lock;
  storage::savePet(app::pet.state());
  storage::saveExtra(app::pet.extra());
  if (clockTrusted()) storage::setLastEpoch(app::epoch());
  lastSave = millis();
  saveWanted = false;
}

void serialCommands() {
  static String line;
  while (Serial.available()) {
    char c = Serial.read();
    if (c != '\n') {
      if (line.length() < 80) line += c;
      continue;
    }
    line.trim();
    if (line == "screen") {
      char hex[201];
      {
        app::Lock lock;
        uint8_t packed[LCD_W * LCD_H / 8];
        app::ui.frame().pack(packed);  // unmasked: the cable is physical access
        for (int i = 0; i < 100; i++) sprintf(hex + i * 2, "%02x", packed[i]);
      }
      Serial.printf("FRAME %s\n", hex);
    } else if (line.startsWith("cloudkey ")) {
      String k = line.substring(9);
      k.trim();
      if (k.length() == 64) {
        cloud::setKey(k);
        Serial.println("CLOUDKEY ok");
      } else {
        Serial.println("CLOUDKEY bad length");
      }
    } else if (line == "cloud") {
      Serial.printf("CLOUD %s\n", cloud::status().c_str());
    } else if (line == "state") {
      app::Lock lock;
      const PetState& s = app::pet.state();
      Serial.printf("STATE stage=%d hunger=%d happy=%d poop=%d sick=%d fw=%s screen=%d\n", int(s.stage), s.hunger,
                    s.happy, s.poop, s.sick, FW_NAME, int(app::ui.screen()));
    }
    line = "";
  }
}
}  // namespace

namespace app {
uint32_t epoch() {
  if (net::timeValid()) return time(nullptr);
  return bootEpoch + (uint32_t)(esp_timer_get_time() / 1000000);
}

void requestSave() { saveWanted = true; }

Date today() { return clockTrusted() ? localDate(epoch()) : Date{}; }

void frameHex(char out[201]) {
  uint8_t packed[LCD_W * LCD_H / 8];
  Screen s = ui.screen();
  // The page mirror must never show the settings code or the setup Wi-Fi password: those prove physical presence.
  if (s == Screen::Pair || s == Screen::Setup) memset(packed, 0, sizeof packed);
  else ui.frame().pack(packed);
  for (int i = 0; i < 100; i++) sprintf(out + i * 2, "%02x", packed[i]);
}

bool pairActive() { return pairCode && before(pairUntil) && pairTries < PAIR_MAX_TRIES; }

bool requestPair() {
  Lock lock;
  static uint32_t lastAsk = 0;
  uint32_t now = millis();
  if (pairActive() || (pairLockUntil && before(pairLockUntil))) return false;
  if (lastAsk && now - lastAsk < PAIR_REQUEST_GAP_MS) return false;
  lastAsk = now;
  pairCode = 1000 + esp_random() % 9000;
  pairUntil = now + PAIR_VALID_MS;
  pairTries = 0;
  return true;
}

String tryPair(const String& code, int& triesLeft) {
  Lock lock;
  if (!pairActive()) {
    triesLeft = 0;
    return "";
  }
  if (code.toInt() != pairCode || code.length() != 4) {
    if (++pairTries >= PAIR_MAX_TRIES) pairLockUntil = millis() + 5 * 60 * 1000;
    triesLeft = PAIR_MAX_TRIES - pairTries;
    return "";
  }
  char tok[33];
  for (int i = 0; i < 4; i++) sprintf(tok + i * 8, "%08lx", (unsigned long)esp_random());
  sessionToken = tok;
  sessionUntil = millis() + SESSION_VALID_MS;
  pairCode = 0;  // one use
  triesLeft = PAIR_MAX_TRIES;
  return sessionToken;
}

bool sessionValid(const String& token) {
  Lock lock;
  return sessionToken.length() == 32 && token == sessionToken && before(sessionUntil);
}

void playTune() {
  ui.whistle(millis());
  tuneEvent = true;
}

bool takeTuneEvent() {
  Lock lock;
  bool t = tuneEvent;
  tuneEvent = false;
  return t;
}
}  // namespace app

// Keep the new image on trial until it has run for a minute (see ota::healthy).
extern "C" bool verifyRollbackLater() { return true; }

void setup() {
  Serial.begin(115200);
  ledcAttach(PIN_LCD_BL, 5000, 8);
  backlight(0);
  lcd.init();
  lcd.setRotation(1);
  lcd.fillScreen(rgb565(LcdPalette().bg));
  backlight(255);

  setenv("TZ", OKAME_TZ, 1);
  tzset();
  storage::begin();
  bootEpoch = storage::lastEpoch();
  bootReal = bootEpoch >= 1700000000;
  if (!bootReal) bootEpoch = 1700000000;
  PetState st;
  if (storage::loadPet(st)) app::pet.load(st);
  else if (bootReal) app::pet.reset(app::epoch());
  // else: keep the constructor's egg; it starts its clock once real time is known
  storage::loadExtra(app::pet);

  net::begin();
  web::begin();
  ota::begin();
  cloud::begin();
  log_i("Okame %s (v%d) up", FW_NAME, FW_VERSION);
}

void loop() {
  uint32_t now = millis();
  serialCommands();
  net::loop();

  {
    app::Lock lock;
    uint32_t ep = app::epoch();
    if (!bootReal && net::timeValid()) {
      bootReal = true;
      if (app::pet.state().eggStart < 1700000000) app::pet.reset(ep);  // first real time: the egg starts now
    }
    // Away from Wi-Fi the bird naps: time stands still, and on return at most 12 h is caught up (Pet::tick).
    if (clockTrusted() && net::online() && now - lastTick >= 1000) {
      lastTick = now;
      app::pet.tick(ep);
    }
    if (pairCode && !app::pairActive()) pairCode = 0;

    env.nowMs = now;
    env.epoch = ep;
    env.buttonDown = false;  // the BOOT button sits on the underside, inside the egg: all input comes from the page
    env.online = net::online();
    env.apMode = net::apActive();
    env.apSsid = net::apSsid();
    env.apPass = net::apPass();
    env.pairActive = app::pairActive();
    env.pairCode = pairCode;
    env.otaProgress = ota::progress();
    env.today = app::today();
    if (net::takeVisitor() && app::ui.screen() == Screen::Main) app::ui.visitor(now);
    app::ui.update(env);

    if (app::ui.takeDirty()) saveWanted = true;
    if (int sc = app::ui.takeRainRecord()) cloud::event(String("{\"type\":\"rain\",\"score\":") + sc + "}");
    if (int m = app::ui.takeFocusDone()) cloud::event(String("{\"type\":\"focus\",\"minutes\":") + m + "}");

    // A learned tune gets whistled now and then while the bird is awake and content.
    const PetState& s = app::pet.state();
    if (s.tuneLevel >= rules::TUNE_LEARNED && app::ui.screen() == Screen::Main &&
        app::pet.mood(ep) == Mood::Content) {
      if (!nextWhistle) nextWhistle = now + 20 * 60 * 1000 + esp_random() % (40 * 60 * 1000);
      if (now >= nextWhistle) {
        nextWhistle = 0;
        app::playTune();
      }
    }

    uint8_t packed[LCD_W * LCD_H / 8];
    app::ui.frame().pack(packed);
    if (memcmp(packed, lastPacked, sizeof packed)) {
      memcpy(lastPacked, packed, sizeof packed);
      renderRgb565(app::ui.frame(), pixels);
      lcd.pushImage(0, 0, PX_W, PX_H, (const lgfx::rgb565_t*)pixels);
    }
    Screen sc = app::ui.screen();
    backlight(sc == Screen::Sleep || sc == Screen::Lights ? 60 : 255);
  }

  if (saveWanted && now - lastSave > SAVE_MIN_GAP_MS) save();
  if (now - lastSave > SAVE_EVERY_MS) save();
  if (now > MARK_VALID_AFTER_MS) ota::healthy();
  if (ota::rebootDue()) {
    save();
    delay(200);
    esp_restart();
  }
  {
    Screen sc = app::ui.screen();
    ota::loop(net::online(), sc == Screen::Main || sc == Screen::Sleep || sc == Screen::Lights, app::epoch());
  }
  web::loop();
  cloud::loop();
  delay(15);
}

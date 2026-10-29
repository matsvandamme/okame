#pragma once
#include <stdint.h>
#include "frame.h"
#include "games.h"
#include "pet.h"
#include "rng.h"

// What the platform tells the UI each frame.
struct Env {
  uint32_t nowMs = 0;      // monotonic milliseconds
  uint32_t epoch = 0;      // game clock (seconds)
  bool buttonDown = false;
  bool online = false;     // joined one of the saved networks
  bool apMode = false;     // own setup Wi-Fi is up
  const char* apSsid = "";
  const char* apPass = "";
  bool pairActive = false;
  uint16_t pairCode = 0;
  int otaProgress = -1;    // 0..100 while updating
  Date today{};            // local calendar date; y == 0 until the clock is known
};

enum class Screen : uint8_t {
  Sleep, Setup, Main, Menu, Stats, Anim, Duet, Peek, Pair, Gone, Ota, Lights, Message, Picture, Code, Rain, Say, Janken, Dance, Focus
};

enum class Anim : uint8_t {
  Hatch, Seed, Millet, Bath, Vet, Refused, NotNeeded, Scritch, Greet, Visitor, Whistle,
  GameWin, GameLose, Sleeping, Gift, Love, Hug, Countdown, HomeDay, Omikuji, Occasion, FocusDone
};

// Weather where Haruki is, from the relay.
enum class Weather : uint8_t { Clear, Rain, Snow, Hot, Cold };

enum class MenuItem : uint8_t { Seed, Millet, Bath, Vet, Light, Game, Stats, Count };

class Ui {
 public:
  explicit Ui(Pet& pet, uint32_t seed = 1) : pet_(pet), rng_(seed) {}

  void update(const Env& env);
  const Frame& frame() const { return frame_; }
  Screen screen() const { return screen_; }

  // Platform events.
  void visitor(uint32_t nowMs) { startAnim(Anim::Visitor, nowMs); }
  void whistle(uint32_t nowMs) { startAnim(Anim::Whistle, nowMs); }
  // An action done from the phone page: play its animation.
  void remoteAction(Action a, Result r, uint32_t nowMs);

  // From Telegram (via the relay).
  void showMessage(const char* from, const char* text, uint32_t nowMs);
  void showPicture(const uint8_t* frames, int count, uint16_t frameMs, uint16_t secs, uint32_t nowMs);
  void showCode(uint16_t code, const char* title, uint32_t nowMs, uint32_t forMs);
  bool busyShowing() const { return screen_ == Screen::Message || screen_ == Screen::Picture || screen_ == Screen::Code; }
  bool messageDone() { bool d = msgDone_; msgDone_ = false; return d; }
  // A hug sent from Telegram: "HUG!" with the sender's name, then the bird holds a big heart.
  void showHug(const char* from, uint32_t nowMs);
  // The 💛 button on the phone page: hearts float up from the bird.
  void love(uint32_t nowMs) {
    // Never over a once-only moment: hatching, the daily cross-off, the home-day party, a holiday greeting.
    bool once = anim_ == Anim::Hatch || anim_ == Anim::Countdown || anim_ == Anim::HomeDay || anim_ == Anim::Occasion;
    if (screen_ == Screen::Main || (screen_ == Screen::Anim && !once)) startAnim(Anim::Love, nowMs);
  }
  // Days until Matthieu is home (0 = today), or -1 when no date is set or it has passed.
  int daysToHome(const Date& today) const;
  // The bird says one of the words Matthieu taught it, in a speech bubble. A word learned less than a day ago
  // comes out half-finished ("I LO..?"), like a real cockatiel still practising.
  void say(const char* word, bool learning, uint32_t nowMs);
  // Focus buddy: the bird sits by a tiny laptop and works along for `minutes`. Notes and hugs wait until the end.
  bool startFocus(int minutes, uint32_t nowMs);
  void stopFocus(uint32_t nowMs);
  int takeRainRecord() { int s = rainRecord_; rainRecord_ = 0; return s; }  // a new Seed Rain best, once
  int takeFocusDone() { int m = focusDoneMin_; focusDoneMin_ = 0; return m; }  // minutes of a session just finished
  int focusLeftSec(uint32_t nowMs) const {
    return screen_ == Screen::Focus ? (int)((int32_t)(focusEnd_ - nowMs) / 1000) : -1;
  }
  // Weather outside, from the relay; it fades back to Clear if not refreshed within 4 hours.
  void setWeather(Weather w, uint32_t nowMs) { wx_ = w; wxAt_ = nowMs; }
  Weather weather(uint32_t nowMs) const { return nowMs - wxAt_ < 4 * 3600 * 1000u ? wx_ : Weather::Clear; }

  // Games are started and played from the phone page.
  enum class Game : uint8_t { Duet, Peek, Rain, Janken, Dance };
  bool startGame(Game g, uint32_t nowMs);
  bool startGame(bool duet, uint32_t nowMs) { return startGame(duet ? Game::Duet : Game::Peek, nowMs); }
  void showGift(int unlocked, uint32_t nowMs) { giftItem_ = unlocked; startAnim(Anim::Gift, nowMs); }
  void gameInput(Press p, uint32_t nowMs);
  bool inGame() const {
    return screen_ == Screen::Duet || screen_ == Screen::Peek || screen_ == Screen::Rain || screen_ == Screen::Janken ||
           screen_ == Screen::Dance;
  }
  // Today's omikuji: the bird shakes the shrine box, a stick drops, and the fortune appears in big kanji.
  void showOmikuji(int grade, uint32_t nowMs) { omikuji_ = grade; startAnim(Anim::Omikuji, nowMs); }

  // Set when the user held the button long enough to ask for a pairing code.
  bool takePairRequest() {
    bool r = pairRequest_;
    pairRequest_ = false;
    return r;
  }
  // True for a moment after a care action, so the platform can save.
  bool takeDirty() {
    bool r = dirty_;
    dirty_ = false;
    return r;
  }

  // Test/sim hooks.
  void forceScreen(Screen s, uint32_t nowMs) { screen_ = s; t0_ = lastInput_ = nowMs; }
  void forceAnim(Anim a, uint32_t nowMs) { startAnim(a, nowMs); }
  void setMenu(MenuItem m) { menu_ = m; }
  void setStatsPage(int p) { statsPage_ = p; }
  Duet& duet() { return duet_; }
  SeedRain& rain() { return rain_; }
  Peekaboo& peek() { return peek_; }
  Janken& janken() { return janken_; }
  Dance& dance() { return dance_; }

 private:
  enum class Pose : uint8_t { Idle, Blink, Up, Flat, Peck, Sleep, Happy };

  void handleButton(const Env& env);
  void onShort(const Env& env);
  void onLong(const Env& env);
  void onDouble(const Env& env);
  void select(MenuItem m, const Env& env);
  void startAnim(Anim a, uint32_t nowMs);
  Screen baseScreen(const Env& env) const;

  void draw(const Env& env);
  void drawBird(Pose pose, int x, int y, bool flip);
  void drawMain(const Env& env);
  void drawSleep(const Env& env);
  void drawSetup(const Env& env);
  void drawMenu(const Env& env);
  void drawStats(const Env& env);
  void drawAnim(const Env& env);
  void drawDuet(const Env& env);
  void drawPeek(const Env& env);
  void drawPair(const Env& env);
  void drawGone(const Env& env);
  void drawOta(const Env& env);
  void drawLights(const Env& env);
  void drawMessage(const Env& env);
  void drawPicture(const Env& env);
  void drawCode(const Env& env);
  void drawRain(const Env& env);
  void drawJanken(const Env& env);
  void drawDance(const Env& env);
  void drawOmikuji(uint32_t el);
  void drawOccasionAnim(uint32_t el);
  void drawDecor(const Env& env, int bx, int by, bool flip);  // season and weather touches on the main screen
  void drawFocus(const Env& env);
  uint32_t focusEnd_ = 0, focusStart_ = 0;
  int focusDoneMin_ = 0, rainRecord_ = 0;
  Occasion occ_ = Occasion::None;
  char newYear_[6] = "";  // the year being greeted, e.g. "2027"
  Weather wx_ = Weather::Clear;
  uint32_t wxAt_ = 0;
  void finishGame(bool won, const Env& env);
  void drawHat(Pose pose, int x, int y, bool flip);
  void drawPoop(uint32_t nowMs);
  void drawHearts(int n, int y);
  int birdW() const;

  Pet& pet_;
  Rng rng_;
  Frame frame_;
  Screen screen_ = Screen::Main;
  Anim anim_ = Anim::Greet;
  MenuItem menu_ = MenuItem::Seed;
  Duet duet_;
  Peekaboo peek_;
  SeedRain rain_;
  Janken janken_;
  Dance dance_;
  int omikuji_ = 0;
  int giftItem_ = -1;
  int statsPage_ = 0;
  uint32_t t0_ = 0, lastInput_ = 0;

  // Button tracking.
  bool wasDown_ = false, holding_ = false, pairFired_ = false, pendingShort_ = false;
  uint32_t downAt_ = 0, upAt_ = 0;

  // Wandering.
  int birdX_ = 12;
  bool facingLeft_ = false, hop_ = false;
  uint32_t nextMove_ = 0, blinkUntil_ = 0;

  // Telegram content.
  static constexpr int MAX_PIC_FRAMES = 8;
  char msgFrom_[17] = "", msgText_[128] = "", codeTitle_[12] = "", hugFrom_[13] = "";
  uint8_t pic_[MAX_PIC_FRAMES][LCD_W * LCD_H / 8];
  int picCount_ = 0;
  uint16_t picMs_ = 300, picSecs_ = 8, code_ = 0;
  uint32_t showUntil_ = 0;
  bool msgDone_ = false;

  // Daily greeting and the countdown card.
  void daily(const Env& env);
  uint32_t nextCard_ = 0;
  bool crossOff_ = false;  // the first card of the day crosses yesterday off
  void maybeSay(const Env& env, bool afterScritch);
  void drawSay(const Env& env);
  uint32_t sayPassMs() const;  // one pass of a long word: lead-in, scroll to its end, hold
  char sayText_[PetExtra::WORD_LEN + 4] = "";
  uint32_t nextSay_ = 0;
  int cardDays_ = 0;

  Stage lastStage_ = Stage::Egg;
  bool lastOnline_ = false, pairRequest_ = false, dirty_ = false, started_ = false;
};

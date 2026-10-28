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
};

enum class Screen : uint8_t {
  Sleep, Setup, Main, Menu, Stats, Anim, Duet, Peek, Pair, Gone, Ota, Lights, Message, Picture, Code
};

enum class Anim : uint8_t {
  Hatch, Seed, Millet, Bath, Vet, Refused, NotNeeded, Scritch, Greet, Visitor, Whistle,
  GameWin, GameLose, Sleeping
};

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

  // Games are started and played from the phone page.
  bool startGame(bool duet, uint32_t nowMs);
  void gameInput(Press p, uint32_t nowMs);
  bool inGame() const { return screen_ == Screen::Duet || screen_ == Screen::Peek; }

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
  Peekaboo& peek() { return peek_; }

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
  char msgFrom_[17] = "", msgText_[128] = "", codeTitle_[12] = "";
  uint8_t pic_[MAX_PIC_FRAMES][LCD_W * LCD_H / 8];
  int picCount_ = 0;
  uint16_t picMs_ = 300, picSecs_ = 8, code_ = 0;
  uint32_t showUntil_ = 0;
  bool msgDone_ = false;

  Stage lastStage_ = Stage::Egg;
  bool lastOnline_ = false, pairRequest_ = false, dirty_ = false, started_ = false;
};

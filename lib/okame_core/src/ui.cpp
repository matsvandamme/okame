#include "ui.h"
#include <stdio.h>
#include "sprites.h"

namespace {
constexpr uint32_t SHORT_MAX_MS = 500;    // shorter = short press
constexpr uint32_t DOUBLE_GAP_MS = 350;   // second press within this = double press
constexpr uint32_t SCRITCH_HOLD_MS = 800;
constexpr uint32_t PAIR_HOLD_MS = 5000;
constexpr uint32_t MENU_IDLE_MS = 6000;
constexpr uint32_t STATS_IDLE_MS = 8000;
constexpr uint32_t ANIM_MS = 2400;
constexpr int STATS_PAGES = 5;

const Sprite* const MENU_ICONS[] = {&spr::ico_seed, &spr::ico_millet, &spr::ico_bath, &spr::ico_vet,
                                    &spr::ico_light, &spr::ico_game, &spr::ico_stats};
const char* const MENU_LABELS[] = {"SEED", "MILLET", "BATH", "VET", "LIGHT", "PLAY", "STATS"};

bool blink(uint32_t nowMs, uint32_t period = 500) { return (nowMs / period) % 2 == 0; }
}  // namespace

int Ui::birdW() const {
  switch (pet_.state().stage) {
    case Stage::Egg: return spr::egg.w;
    case Stage::Chick: return spr::chick_idle.w;
    case Stage::Juvenile: return spr::juv_idle.w;
    default: return spr::adult_idle.w;
  }
}

void Ui::drawBird(Pose pose, int x, int y, bool flip) {
  const PetState& s = pet_.state();
  const Sprite* sp = &spr::adult_idle;
  Pattern pat = Pattern::Solid;
  switch (s.stage) {
    case Stage::Egg:
      sp = &spr::egg;
      break;
    case Stage::Chick:
      switch (pose) {
        case Pose::Blink: sp = &spr::chick_blink; break;
        case Pose::Peck: sp = &spr::chick_peck; break;
        case Pose::Flat: sp = &spr::chick_sleep; break;
        case Pose::Sleep: sp = &spr::chick_sleep; break;
        case Pose::Happy: sp = &spr::chick_happy; break;
        default: sp = &spr::chick_idle; break;
      }
      break;
    case Stage::Juvenile:
      switch (pose) {
        case Pose::Blink: sp = &spr::juv_blink; break;
        case Pose::Peck: sp = &spr::juv_peck; break;
        case Pose::Flat: sp = &spr::juv_flat; break;
        case Pose::Sleep: sp = &spr::juv_sleep; break;
        case Pose::Happy: sp = &spr::juv_happy; break;
        default: sp = &spr::juv_idle; break;
      }
      break;
    default:
      switch (pose) {
        case Pose::Blink: sp = &spr::adult_blink; break;
        case Pose::Up: sp = &spr::adult_up; break;
        case Pose::Flat: sp = &spr::adult_flat; break;
        case Pose::Peck: sp = &spr::adult_peck; break;
        case Pose::Sleep: sp = &spr::adult_sleep; break;
        case Pose::Happy: sp = &spr::adult_happy; break;
        default: sp = &spr::adult_idle; break;
      }
      switch (s.variant) {
        case Variant::Pearl: pat = Pattern::Pearl; break;
        case Variant::Pied: pat = Pattern::Pied; break;
        case Variant::Scruffy: pat = Pattern::Scruffy; break;
        default: pat = Pattern::Solid; break;
      }
  }
  // Bottom-align so feet stay on the floor whatever the sprite height.
  frame_.sprite(*sp, x, y - sp->h, flip, pat);
  drawHat(pose, x, y, flip);
}

// Accessories sit on fixed spots of each life stage's sprite (facing right): bow tie, scarf and medal at the neck,
// sunglasses over the eye, a sakura flower tucked into the feathers behind the eye. Skipped while pecking.
void Ui::drawHat(Pose pose, int x, int y, bool flip) {
  const PetState& s = pet_.state();
  if (!s.hat || s.hat >= HAT_COUNT || s.stage == Stage::Egg || pose == Pose::Peck) return;
  const Sprite* items[] = {nullptr, &spr::hat_bow, &spr::hat_crown, &spr::hat_beret, &spr::hat_party, &spr::hat_sakura};
  const Sprite& a = *items[s.hat];
  // Top-left of each accessory in the facing-right sprite (index = Accessory); the chick wears the bow as a hair bow.
  static const int8_t OFF[3][6][2] = {
      /*chick*/ {{0, 0}, {0, 0}, {3, 3}, {2, 5}, {7, 6}, {0, 0}},
      /*juv  */ {{0, 0}, {11, 9}, {7, 4}, {6, 7}, {12, 8}, {3, 4}},
      /*adult*/ {{0, 0}, {13, 11}, {10, 5}, {9, 8}, {15, 10}, {6, 4}},
  };
  int st = s.stage == Stage::Chick ? 0 : s.stage == Stage::Juvenile ? 1 : 2;
  const Sprite* body = st == 0 ? &spr::chick_idle : st == 1 ? &spr::juv_idle : &spr::adult_idle;
  // The flower clashes with a flattened crest.
  if (s.hat == HAT_SAKURA && st == 2 && (pose == Pose::Flat || pose == Pose::Sleep)) return;
  int ax = OFF[st][s.hat][0], ay = OFF[st][s.hat][1];
  if (flip) ax = body->w - ax - a.w;
  frame_.sprite(a, x + ax, y - body->h + ay, flip);
}

void Ui::startAnim(Anim a, uint32_t nowMs) {
  anim_ = a;
  screen_ = Screen::Anim;
  t0_ = nowMs;
}

void Ui::remoteAction(Action a, Result r, uint32_t nowMs) {
  if (r == Result::Refused) return startAnim(Anim::Refused, nowMs);
  if (r == Result::NotNeeded) return startAnim(Anim::NotNeeded, nowMs);
  if (r == Result::Sleeping) return startAnim(Anim::Sleeping, nowMs);
  switch (a) {
    case Action::Seed: startAnim(Anim::Seed, nowMs); break;
    case Action::Millet: startAnim(Anim::Millet, nowMs); break;
    case Action::Bath: startAnim(Anim::Bath, nowMs); break;
    case Action::Vet: startAnim(Anim::Vet, nowMs); break;
    case Action::Scritch: startAnim(Anim::Scritch, nowMs); break;
    default: screen_ = Screen::Main; break;
  }
}

Screen Ui::baseScreen(const Env& env) const {
  if (env.otaProgress >= 0) return Screen::Ota;
  if (env.pairActive) return Screen::Pair;
  if (pet_.state().stage == Stage::Gone) return Screen::Gone;
  if (!env.online) return env.apMode ? Screen::Setup : Screen::Sleep;
  if (pet_.state().lightsOff) return Screen::Lights;
  return Screen::Main;
}

void Ui::update(const Env& env) {
  uint32_t now = env.nowMs;
  const PetState& s = pet_.state();

  if (!started_) {
    started_ = true;
    lastStage_ = s.stage;
    lastOnline_ = env.online;
    screen_ = baseScreen(env);
    nextMove_ = now + 800;
  }

  handleButton(env);

  // Events from state changes.
  if (lastStage_ == Stage::Egg && s.stage == Stage::Chick) startAnim(Anim::Hatch, now);
  lastStage_ = s.stage;
  if (env.online && !lastOnline_ && s.stage != Stage::Egg && s.stage != Stage::Gone)
    startAnim(Anim::Greet, now);
  lastOnline_ = env.online;

  // Screens that override everything else.
  Screen forced = baseScreen(env);
  if (forced == Screen::Ota || forced == Screen::Pair || forced == Screen::Gone ||
      forced == Screen::Setup || forced == Screen::Sleep) {
    if (screen_ != forced && !(screen_ == Screen::Anim && anim_ == Anim::Hatch)) {
      screen_ = forced;
      t0_ = now;
    }
  } else if (screen_ == Screen::Ota || screen_ == Screen::Pair || screen_ == Screen::Gone ||
             screen_ == Screen::Setup || screen_ == Screen::Sleep) {
    screen_ = forced;
    t0_ = now;
  }

  // Timeouts back to the base screen.
  switch (screen_) {
    case Screen::Menu:
      if (now - lastInput_ > MENU_IDLE_MS) screen_ = forced;
      break;
    case Screen::Stats:
      if (now - lastInput_ > STATS_IDLE_MS) screen_ = forced;
      break;
    case Screen::Anim:
      if (anim_ == Anim::Scritch && holding_) break;
      if (now - t0_ > (anim_ == Anim::Hatch ? 3600u : anim_ == Anim::Gift ? 3400u : ANIM_MS)) screen_ = forced;
      break;
    case Screen::Duet:
      duet_.update(now);
      if (duet_.phase() == Duet::Phase::Done) {
        bool won = duet_.won();
        pet_.recordGame(won, 0);
        pet_.act(won ? Action::GameWin : Action::GameLose, env.epoch);
        dirty_ = true;
        startAnim(won ? Anim::GameWin : Anim::GameLose, now);
      }
      break;
    case Screen::Peek:
      peek_.update(now);
      if (peek_.phase() == Peekaboo::Phase::Done) {
        bool won = peek_.won();
        pet_.recordGame(won, 0);
        pet_.act(won ? Action::GameWin : Action::GameLose, env.epoch);
        dirty_ = true;
        startAnim(won ? Anim::GameWin : Anim::GameLose, now);
      }
      break;
    case Screen::Rain:
      rain_.update(now);
      if (rain_.done()) {
        bool won = rain_.score() >= 10;
        pet_.recordGame(won, rain_.score());
        pet_.act(won ? Action::GameWin : Action::GameLose, env.epoch);
        dirty_ = true;
        startAnim(won ? Anim::GameWin : Anim::GameLose, now);
      }
      break;
    case Screen::Main:
    case Screen::Lights:
      screen_ = forced;
      break;
    case Screen::Message: {
      // Envelope 1.5 s, then the text scrolls past twice.
      int w = Frame::textWidth(msgText_, 2);
      uint32_t scrollMs = (uint32_t)(LCD_W + w) * 55;
      if (now - t0_ > 1500 + 2 * scrollMs) {
        msgDone_ = true;
        screen_ = forced;
      }
      break;
    }
    case Screen::Picture:
      if (now - t0_ > picSecs_ * 1000u) screen_ = forced;
      break;
    case Screen::Code:
      if ((int32_t)(now - showUntil_) > 0) screen_ = forced;
      break;
    default:
      break;
  }

  // Wander on the main screen.
  if (now >= nextMove_) {
    nextMove_ = now + 700;
    hop_ = false;
    Mood m = pet_.mood(env.epoch);
    if (m == Mood::Content || m == Mood::Meh) {
      uint32_t r = rng_.range(0, 99);
      int maxX = LCD_W - birdW() - 1 - 5 * pet_.state().poop;
      if (r < 15) {
        facingLeft_ = !facingLeft_;
      } else if (r < 50) {
        birdX_ += facingLeft_ ? -1 : 1;
        if (birdX_ < 1 || birdX_ > maxX) {
          facingLeft_ = !facingLeft_;
          birdX_ = birdX_ < 1 ? 1 : maxX;
        }
      } else if (r < 62 && m == Mood::Content) {
        hop_ = true;
      }
      if (rng_.range(0, 99) < 12) blinkUntil_ = now + 180;
    }
  }

  draw(env);
}

void Ui::handleButton(const Env& env) {
  uint32_t now = env.nowMs;
  bool down = env.buttonDown;
  if (down && !wasDown_) {
    downAt_ = now;
    pairFired_ = false;
  }
  if (down) {
    uint32_t held = now - downAt_;
    if (!pairFired_ && held >= PAIR_HOLD_MS && screen_ != Screen::Duet && screen_ != Screen::Peek) {
      pairFired_ = true;
      holding_ = false;
      pairRequest_ = true;
      screen_ = Screen::Main;
    } else if (!holding_ && !pairFired_ && held >= SCRITCH_HOLD_MS && screen_ == Screen::Main &&
               pet_.state().stage != Stage::Egg) {
      holding_ = true;
      pendingShort_ = false;
      pet_.act(Action::Scritch, env.epoch);
      dirty_ = true;
      startAnim(Anim::Scritch, now);
    }
  }
  if (!down && wasDown_) {
    uint32_t held = now - downAt_;
    lastInput_ = now;
    if (holding_) {
      holding_ = false;
      t0_ = now;  // let the heart float up after release
    } else if (!pairFired_) {
      if (held < SHORT_MAX_MS) {
        if (screen_ == Screen::Main && pendingShort_ && now - upAt_ <= DOUBLE_GAP_MS) {
          pendingShort_ = false;
          onDouble(env);
        } else if (screen_ == Screen::Main) {
          pendingShort_ = true;
        } else {
          onShort(env);
        }
      } else {
        onLong(env);
      }
    }
    upAt_ = now;
  }
  if (pendingShort_ && !down && now - upAt_ > DOUBLE_GAP_MS) {
    pendingShort_ = false;
    onShort(env);
  }
  wasDown_ = down;
}

void Ui::onShort(const Env& env) {
  uint32_t now = env.nowMs;
  switch (screen_) {
    case Screen::Main:
      if (pet_.state().stage == Stage::Egg) return;
      screen_ = Screen::Menu;
      menu_ = MenuItem::Seed;
      break;
    case Screen::Lights:
      screen_ = Screen::Menu;
      menu_ = MenuItem::Light;
      break;
    case Screen::Menu:
      menu_ = MenuItem((uint8_t(menu_) + 1) % uint8_t(MenuItem::Count));
      break;
    case Screen::Stats:
      if (++statsPage_ >= STATS_PAGES) screen_ = baseScreen(env);
      break;
    case Screen::Duet:
      duet_.input(Press::Short, now);
      break;
    case Screen::Peek:
      peek_.input(now);
      break;
    case Screen::Anim:
      if (anim_ != Anim::Hatch) screen_ = baseScreen(env);
      break;
    default:
      break;
  }
  t0_ = screen_ == Screen::Anim ? t0_ : now;
}

void Ui::onLong(const Env& env) {
  switch (screen_) {
    case Screen::Menu:
      select(menu_, env);
      break;
    case Screen::Stats:
      screen_ = baseScreen(env);
      break;
    case Screen::Duet:
      duet_.input(Press::Long, env.nowMs);
      break;
    case Screen::Peek:
      peek_.input(env.nowMs);
      break;
    case Screen::Gone:
      pet_.reset(env.epoch);
      dirty_ = true;
      screen_ = baseScreen(env);
      break;
    default:
      break;
  }
}

void Ui::onDouble(const Env& env) {
  if (pet_.state().stage == Stage::Egg) return;
  screen_ = Screen::Stats;
  statsPage_ = 0;
  t0_ = env.nowMs;
}

void Ui::select(MenuItem m, const Env& env) {
  uint32_t now = env.nowMs;
  Action a;
  switch (m) {
    case MenuItem::Seed: a = Action::Seed; break;
    case MenuItem::Millet: a = Action::Millet; break;
    case MenuItem::Bath: a = Action::Bath; break;
    case MenuItem::Vet: a = Action::Vet; break;
    case MenuItem::Light:
      pet_.act(Action::Light, env.epoch);
      dirty_ = true;
      screen_ = baseScreen(env);
      return;
    case MenuItem::Game:
      if (pet_.state().lightsOff) return startAnim(Anim::Sleeping, now);
      // Alternate games so both get played.
      if (rng_.next() & 1) {
        duet_.start(rng_.next(), now);
        screen_ = Screen::Duet;
      } else {
        peek_.start(rng_.next(), now);
        screen_ = Screen::Peek;
      }
      t0_ = now;
      return;
    case MenuItem::Stats:
      screen_ = Screen::Stats;
      statsPage_ = 0;
      t0_ = now;
      return;
    default:
      return;
  }
  Result r = pet_.act(a, env.epoch);
  dirty_ = true;
  remoteAction(a, r, now);
}

void Ui::showMessage(const char* from, const char* text, uint32_t nowMs) {
  snprintf(msgFrom_, sizeof msgFrom_, "%s", from);
  for (char* c = msgFrom_; *c; c++)
    if (*c >= 'a' && *c <= 'z') *c -= 32;
  snprintf(msgText_, sizeof msgText_, "%s", text);
  screen_ = Screen::Message;
  t0_ = nowMs;
}

void Ui::showPicture(const uint8_t* frames, int count, uint16_t frameMs, uint16_t secs, uint32_t nowMs) {
  picCount_ = count < MAX_PIC_FRAMES ? count : MAX_PIC_FRAMES;
  memcpy(pic_, frames, picCount_ * sizeof pic_[0]);
  picMs_ = frameMs < 80 ? 80 : frameMs;
  picSecs_ = secs < 2 ? 2 : secs > 30 ? 30 : secs;
  screen_ = Screen::Picture;
  t0_ = nowMs;
}

void Ui::showCode(uint16_t code, const char* title, uint32_t nowMs, uint32_t forMs) {
  code_ = code;
  snprintf(codeTitle_, sizeof codeTitle_, "%s", title);
  screen_ = Screen::Code;
  t0_ = nowMs;
  showUntil_ = nowMs + forMs;
}

bool Ui::startGame(Game g, uint32_t nowMs) {
  const PetState& s = pet_.state();
  if (s.stage == Stage::Egg || s.stage == Stage::Gone || s.lightsOff) return false;
  if (screen_ != Screen::Main && screen_ != Screen::Anim && !inGame()) return false;
  switch (g) {
    case Game::Duet: duet_.start(rng_.next(), nowMs); screen_ = Screen::Duet; break;
    case Game::Peek: peek_.start(rng_.next(), nowMs); screen_ = Screen::Peek; break;
    case Game::Rain: rain_.start(rng_.next(), nowMs, birdW()); screen_ = Screen::Rain; break;
  }
  t0_ = nowMs;
  return true;
}

void Ui::gameInput(Press p, uint32_t nowMs) {
  if (screen_ == Screen::Rain) rain_.input(p);
  else if (screen_ == Screen::Duet) duet_.input(p, nowMs);
  else if (screen_ == Screen::Peek) peek_.input(nowMs);
}

// ---------------------------------------------------------------- drawing

void Ui::draw(const Env& env) {
  frame_.clear();
  switch (screen_) {
    case Screen::Sleep: drawSleep(env); break;
    case Screen::Setup: drawSetup(env); break;
    case Screen::Main: drawMain(env); break;
    case Screen::Menu: drawMenu(env); break;
    case Screen::Stats: drawStats(env); break;
    case Screen::Anim: drawAnim(env); break;
    case Screen::Duet: drawDuet(env); break;
    case Screen::Peek: drawPeek(env); break;
    case Screen::Pair: drawPair(env); break;
    case Screen::Gone: drawGone(env); break;
    case Screen::Ota: drawOta(env); break;
    case Screen::Lights: drawLights(env); break;
    case Screen::Message: drawMessage(env); break;
    case Screen::Picture: drawPicture(env); break;
    case Screen::Code: drawCode(env); break;
    case Screen::Rain: drawRain(env); break;
  }
}

void Ui::drawPoop(uint32_t nowMs) {
  int n = pet_.state().poop;
  for (int i = 0; i < n; i++) {
    int x = LCD_W - 5 - i * 5, y = LCD_H - 4;
    frame_.sprite(spr::poop, x, y);
    if (blink(nowMs, 600) == (i % 2 == 0)) frame_.sprite(spr::stink, x + 3, y - 4);
  }
}

void Ui::drawMain(const Env& env) {
  uint32_t now = env.nowMs;
  const PetState& s = pet_.state();
  Mood m = pet_.mood(env.epoch);

  if (s.stage == Stage::Egg) {
    int wobble = (now / 400) % 4 == 1 ? 1 : ((now / 400) % 4 == 3 ? -1 : 0);
    frame_.sprite(spr::egg, (LCD_W - spr::egg.w) / 2 + wobble, LCD_H - spr::egg.h);
    return;
  }

  Pose pose = Pose::Idle;
  if (m == Mood::Asleep) pose = Pose::Sleep;
  else if (m == Mood::Sad || m == Mood::Sick) pose = Pose::Flat;
  if (now < blinkUntil_ && pose == Pose::Idle) pose = Pose::Blink;
  int y = LCD_H - (hop_ ? 1 : 0);
  drawBird(pose, birdX_, y, facingLeft_);
  drawPoop(now);

  // Status icons in the top-left corner.
  if (m == Mood::Sick && blink(now, 500)) frame_.sprite(spr::ico_vet, 0, 0);
  else if (m == Mood::Sad) frame_.sprite(spr::cloud, 0, 0);
  else if (m == Mood::Asleep && blink(now, 700)) frame_.sprite(spr::zz, 1, 1);
  if (pet_.needsAttention(env.epoch) && blink(now, 400)) frame_.sprite(spr::exclaim, LCD_W - 2, 0);
}

void Ui::drawSleep(const Env& env) {
  uint32_t now = env.nowMs;
  if (pet_.state().stage == Stage::Egg) {
    frame_.sprite(spr::egg, 8, LCD_H - spr::egg.h);
  } else {
    drawBird(Pose::Sleep, 4, LCD_H, false);
  }
  if (blink(now, 800)) frame_.sprite(spr::wifi, 1, 1);
  // Three z's drifting up and to the right.
  int phase = (now / 450) % 6;
  for (int i = 0; i < 3; i++) {
    int p = (phase + i * 2) % 6;
    frame_.sprite(spr::zz, 25 + p * 2, 12 - p * 2);
  }
}

void Ui::drawSetup(const Env& env) {
  // Page through: the network name, then the password.
  int page = (env.nowMs / 3000) % 2;
  if (page == 0) {
    frame_.sprite(spr::wifi, 1, 1);
    frame_.text("JOIN", 10, 1);
    frame_.textCentered(env.apSsid, 8);
    frame_.textCentered("TO SET UP", 15);
  } else {
    frame_.textCentered("PASSWORD", 1);
    frame_.textCentered(env.apPass, 9);
  }
}

void Ui::drawMenu(const Env& env) {
  int idx = int(menu_);
  frame_.sprite(*MENU_ICONS[idx], (LCD_W - 10) / 2, 1, false, Pattern::Solid, 2);
  frame_.textCentered(MENU_LABELS[idx], 12);
  int n = int(MenuItem::Count);
  int x0 = (LCD_W - (n * 2 - 1)) / 2;
  for (int i = 0; i < n; i++) frame_.set(x0 + i * 2, 19, true);
  frame_.set(x0 + idx * 2, 18, true);
  // Arrows hint that a short press moves on.
  if (blink(env.nowMs, 600)) {
    frame_.set(LCD_W - 3, 5, true);
    frame_.set(LCD_W - 2, 6, true);
    frame_.set(LCD_W - 3, 7, true);
  }
}

void Ui::drawHearts(int n, int y) {
  for (int i = 0; i < rules::MAX_STAT; i++)
    frame_.sprite(i < n ? spr::heart : spr::heart_empty, 7 + i * 7, y);
}

void Ui::drawStats(const Env& env) {
  const PetState& s = pet_.state();
  char buf[24];
  switch (statsPage_) {
    case 0:
      frame_.textCentered("FOOD", 1);
      drawHearts(s.hunger, 9);
      break;
    case 1:
      frame_.textCentered("JOY", 1);
      drawHearts(s.happy, 9);
      break;
    case 2: {
      uint32_t days = pet_.ageSeconds(env.epoch) / rules::DAY;
      frame_.textCentered("AGE", 1);
      snprintf(buf, sizeof buf, "%luD", (unsigned long)days);
      frame_.textCentered(buf, 7, 2);
      break;
    }
    case 3:
      frame_.textCentered("WEIGHT", 1);
      snprintf(buf, sizeof buf, "%uG", s.weight);
      frame_.textCentered(buf, 7, 2);
      break;
    default: {
      frame_.textCentered("TUNE", 1);
      if (!s.tuneLen) {
        frame_.textCentered("NONE", 9);
      } else {
        // One note per learned day, a dot for days still to practise.
        for (int i = 0; i < rules::TUNE_LEARNED; i++) {
          if (i < s.tuneLevel) frame_.sprite(spr::note, 10 + i * 8, 8);
          else frame_.set(12 + i * 8, 12, true);
        }
      }
      break;
    }
  }
  // Page dots along the bottom, current page marked above.
  int x0 = (LCD_W - (STATS_PAGES * 2 - 1)) / 2;
  for (int i = 0; i < STATS_PAGES; i++) frame_.set(x0 + i * 2, 19, true);
  frame_.set(x0 + statsPage_ * 2, 18, true);
}

void Ui::drawAnim(const Env& env) {
  uint32_t now = env.nowMs, el = now - t0_;
  int cx = (LCD_W - birdW()) / 2;
  bool flip = false;
  bool alt = (el / 300) % 2;

  switch (anim_) {
    case Anim::Hatch:
      if (el < 1600) {
        int wob = alt ? 1 : -1;
        frame_.sprite(spr::egg, (LCD_W - spr::egg.w) / 2 + wob, LCD_H - spr::egg.h);
      } else if (el < 2400) {
        frame_.sprite(spr::egg_crack, (LCD_W - spr::egg.w) / 2, LCD_H - spr::egg.h);
      } else {
        drawBird(Pose::Happy, cx, LCD_H, false);
        if (alt) frame_.sprite(spr::sparkle, cx - 6, 2);
        else frame_.sprite(spr::sparkle, cx + birdW() + 1, 3);
      }
      return;
    case Anim::Seed:
      drawBird(alt ? Pose::Peck : Pose::Idle, cx - 2, LCD_H, flip);
      if (el < ANIM_MS - 500) frame_.sprite(spr::seeds, cx + birdW() - 2, LCD_H - 2);
      break;
    case Anim::Millet:
      drawBird(alt ? Pose::Peck : Pose::Idle, cx - 3, LCD_H, flip);
      frame_.sprite(spr::millet_spray, cx + birdW() - 1, 2 + (el > 1500 ? 2 : 0));
      break;
    case Anim::Bath:
      drawBird(Pose::Happy, cx, LCD_H, flip);
      for (int i = 0; i < 4; i++) {
        int x = 3 + i * 10 + (i % 2) * 2;
        int y = int((el / 60 + i * 5) % 20) - 3;
        frame_.sprite(spr::drops, x, y);
      }
      break;
    case Anim::Vet:
      drawBird(alt ? Pose::Blink : Pose::Idle, cx, LCD_H, flip);
      frame_.sprite(spr::sparkle, cx + birdW() + 1, alt ? 2 : 6);
      frame_.sprite(spr::ico_vet, 1, 1);
      break;
    case Anim::Refused:
      drawBird(Pose::Flat, cx, LCD_H, true);
      if (alt) frame_.sprite(spr::sick, LCD_W - 6, 1);
      break;
    case Anim::NotNeeded:
      drawBird(Pose::Idle, cx, LCD_H, false);
      frame_.sprite(spr::question, cx + birdW() + 1, 2);
      break;
    case Anim::Sleeping:
      drawBird(Pose::Sleep, cx, LCD_H, false);
      frame_.sprite(spr::zz, cx + birdW() + 1, 2);
      break;
    case Anim::Scritch: {
      drawBird(Pose::Blink, cx, LCD_H, false);
      // A heart rises while held, then floats off after release.
      int y = holding_ ? 3 : 3 - int(el / 120);
      if (y > -6) frame_.sprite(spr::heart, cx + birdW() + 1, y);
      break;
    }
    case Anim::Greet: {
      bool up = (el / 250) % 2;
      drawBird(Pose::Up, cx, LCD_H - (up ? 1 : 0), false);
      if (blink(now, 250)) frame_.sprite(spr::exclaim, cx + birdW() + 1, 1);
      frame_.sprite(spr::sparkle, (el / 500) % 2 ? 1 : LCD_W - 6, 2);
      break;
    }
    case Anim::Visitor:
      drawBird(Pose::Up, cx, LCD_H, (el / 600) % 2);
      frame_.sprite(spr::question, (el / 600) % 2 ? cx - 4 : cx + birdW() + 1, 1);
      break;
    case Anim::Whistle: {
      drawBird(alt ? Pose::Happy : Pose::Idle, cx - 4, LCD_H, false);
      for (int i = 0; i < 3; i++) {
        int y = 12 - int((el / 90 + i * 6) % 16);
        frame_.sprite(spr::note, cx + birdW() - 2 + i * 4, y);
      }
      break;
    }
    case Anim::GameWin:
      drawBird(Pose::Happy, cx, LCD_H - (alt ? 1 : 0), false);
      frame_.sprite(spr::heart, 2, 2);
      frame_.sprite(spr::heart, LCD_W - 7, 2);
      break;
    case Anim::GameLose:
      drawBird(Pose::Flat, cx, LCD_H, false);
      frame_.sprite(spr::cloud, 1, 0);
      break;
    case Anim::Gift:
      if (el < 1400) {  // the present wobbles, then pops open
        frame_.sprite(spr::gift, (LCD_W - 10) / 2 + (alt ? 1 : -1), 4, false, Pattern::Solid, 2);
      } else {
        drawBird(Pose::Happy, cx, LCD_H, false);
        const Sprite* items[] = {nullptr, &spr::hat_bow, &spr::hat_crown, &spr::hat_beret, &spr::hat_party, &spr::hat_sakura};
        if (giftItem_ > 0 && giftItem_ < HAT_COUNT && el < 2200)
          frame_.sprite(*items[giftItem_], LCD_W - 9, 2, false, Pattern::Solid, 2);  // what was inside
        else if (giftItem_ <= 0)
          frame_.sprite(spr::seeds, LCD_W - 7, 4, false, Pattern::Solid, 1);
        frame_.sprite(spr::sparkle, alt ? 1 : LCD_W - 6, 8);
      }
      break;
  }
  drawPoop(now);
}

void Ui::drawDuet(const Env& env) {
  const Duet& d = duet_;
  int len = d.length();
  // Pattern row: dot for a short note, dash for a long one. Filled while it plays.
  int w = 0;
  for (int i = 0; i < len; i++) w += (d.note(i) ? 4 : 2) + 1;
  int x = LCD_W - w + 1;
  bool showing = d.phase() == Duet::Phase::Showing;
  for (int i = 0; i < len; i++) {
    int nw = d.note(i) ? 4 : 2;
    bool reveal = showing ? i <= d.showing() || d.showing() < 0 : true;
    if (reveal) {
      if (showing && i == d.showing()) frame_.fillRect(x, 1, nw, 2);
      else frame_.rect(x, 1, nw, 2);
    }
    // Player's answers underneath.
    if (i < d.entered()) frame_.fillRect(x, 5, d.enteredNote(i) ? 4 : 2, 2);
    x += nw + 1;
  }
  Pose pose = showing && d.showing() >= 0 ? Pose::Happy : Pose::Idle;
  if (d.phase() == Duet::Phase::RoundLost) pose = Pose::Flat;
  int cx = 0;
  drawBird(pose, cx, LCD_H, false);
  if (showing && d.showing() >= 0) frame_.sprite(spr::note, cx + birdW() + 1, 9);
  if (d.phase() == Duet::Phase::Listening && blink(env.nowMs, 400)) frame_.sprite(spr::question, LCD_W - 5, 11);
  if (d.phase() == Duet::Phase::RoundWon) frame_.sprite(spr::heart, LCD_W - 7, 11);
  // Round dots bottom-right.
  for (int i = 0; i < Duet::ROUNDS; i++) frame_.set(LCD_W - 1 - (Duet::ROUNDS - 1 - i) * 2, 19, true);
  frame_.set(LCD_W - 1 - (Duet::ROUNDS - 1 - d.round()) * 2, 18, true);
}

void Ui::drawPeek(const Env& env) {
  const Peekaboo& p = peek_;
  // A tree trunk with bark in the middle; the bird hides behind it.
  int tx = LCD_W / 2 - 3;
  auto trunk = [&] {
    frame_.fillRect(tx, 0, 6, LCD_H, false);
    frame_.vline(tx, 0, LCD_H);
    frame_.vline(tx + 5, 0, LCD_H);
    for (int y = 1; y < LCD_H; y += 3) frame_.set(tx + 2 + (y / 3) % 2, y, true);
  };
  if (p.phase() == Peekaboo::Phase::Peeking || p.phase() == Peekaboo::Phase::Hit) {
    Pose pose = p.phase() == Peekaboo::Phase::Hit ? Pose::Happy : Pose::Up;
    int show = birdW() / 2 + 1;
    int bx = p.peekRight() ? tx + 6 - birdW() + show : tx - show;
    drawBird(pose, bx, LCD_H, !p.peekRight());
    frame_.fillRect(p.peekRight() ? 0 : tx + 6, 0, tx, LCD_H, false);  // hide the far side
    if (p.phase() == Peekaboo::Phase::Hit) frame_.sprite(spr::heart, p.peekRight() ? LCD_W - 6 : 1, 2);
  } else if (p.phase() == Peekaboo::Phase::Miss) {
    frame_.sprite(spr::question, 2, 2);
  }
  trunk();
  // While hidden, the tail tip pokes out on the far side.
  if (p.phase() == Peekaboo::Phase::Hidden) frame_.hline(p.peekRight() ? tx - 4 : tx + 6, LCD_H - 1, 4);
  // Score: one slot per round, hits marked above.
  for (int i = 0; i < Peekaboo::ROUNDS; i++) {
    frame_.set(1 + i * 2, LCD_H - 1, true);
    if (i < p.hits()) frame_.set(1 + i * 2, LCD_H - 2, true);
  }
}

void Ui::drawPair(const Env& env) {
  char buf[8];
  frame_.textCentered("CODE", 1);
  snprintf(buf, sizeof buf, "%04u", env.pairCode % 10000);
  frame_.textCentered(buf, 9, 2);
}

void Ui::drawGone(const Env& env) {
  uint32_t el = env.nowMs - t0_;
  int y = int((el / 250) % 26) - 7;
  int x = 28 + ((el / 500) % 2 ? 1 : -1);
  frame_.sprite(spr::feather, x, y);
  frame_.text("BYE", 1, 1);
  if ((el / 1500) % 2) frame_.text("SEE APP", 1, 14);
}

void Ui::drawOta(const Env& env) {
  frame_.textCentered("UPDATE", 3);
  frame_.rect(4, 11, 32, 5);
  frame_.fillRect(5, 12, (30 * env.otaProgress) / 100, 3);
}

void Ui::drawMessage(const Env& env) {
  uint32_t el = env.nowMs - t0_;
  if (el < 1500) {
    int bounce = (el / 200) % 2;
    frame_.sprite(spr::mail, (LCD_W - spr::mail.w) / 2, 7 - bounce, false, Pattern::Solid, 1);
    if ((el / 300) % 2) frame_.sprite(spr::sparkle, 6, 3);
    else frame_.sprite(spr::sparkle, LCD_W - 11, 4);
    return;
  }
  char head[24];
  snprintf(head, 9, "%s", msgFrom_[0] ? msgFrom_ : "MAIL");  // 8 characters fit next to the envelope
  frame_.sprite(spr::mail_s, 0, 1);
  frame_.text(head, 7, 1);
  int w = Frame::textWidth(msgText_, 2);
  uint32_t pass = (uint32_t)(LCD_W + w) * 55;
  int x = LCD_W - (int)(((el - 1500) % pass) / 55);
  frame_.text(msgText_, x, 9, 2);
}

void Ui::drawPicture(const Env& env) {
  if (!picCount_) return;
  int i = ((env.nowMs - t0_) / picMs_) % picCount_;
  frame_.unpack(pic_[i]);
}

void Ui::drawCode(const Env& env) {
  char buf[8];
  frame_.textCentered(codeTitle_, 1);
  snprintf(buf, sizeof buf, "%04u", code_ % 10000);
  frame_.textCentered(buf, 9, 2);
}

void Ui::drawRain(const Env& env) {
  const SeedRain& r = rain_;
  for (int i = 0; i < SeedRain::MAX_SEEDS; i++) {
    const SeedRain::Seed& sd = r.seed(i);
    if (!sd.live) continue;
    int y = sd.y16 / 16;
    if (sd.golden) frame_.sprite(spr::sparkle, sd.x - 2, y - 2);
    else frame_.fillRect(sd.x, y, 2, 2);
  }
  drawBird(Pose::Up, r.birdX(), LCD_H, false);
  char buf[8];
  snprintf(buf, sizeof buf, "%d", r.score());
  frame_.text(buf, 1, 1);
  for (int i = 0; i < r.lives(); i++) frame_.sprite(spr::heart_s, LCD_W - 4 - i * 4, 0);
}

void Ui::drawLights(const Env& env) {
  frame_.fill();
  int phase = (env.nowMs / 600) % 3;
  frame_.spriteInverse(spr::zz, 18 + phase * 3, 8 - phase * 2);
}

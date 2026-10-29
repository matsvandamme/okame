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
constexpr uint32_t CARD_EVERY_MS = 15 * 60 * 1000;  // the countdown card, now and then on the main screen
constexpr uint32_t SAY_MIN_MS = 30 * 60 * 1000, SAY_MAX_MS = 90 * 60 * 1000;  // between spoken words
constexpr uint32_t SAY_SCROLL_MS = 100, SAY_LEAD_MS = 700, SAY_HOLD_MS = 1200, SAY_STATIC_MS = 4000;
constexpr int SAY_FIT = LCD_W - 4;  // text width that fits the bubble without scrolling
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
  const Sprite* items[] = {nullptr, &spr::hat_bow, &spr::hat_crown, &spr::hat_beret, &spr::hat_party, &spr::hat_sakura,
                            &spr::hat_omamori, &spr::hat_hachimaki};
  const Sprite& a = *items[s.hat];
  // Top-left of each accessory in the facing-right sprite (index = Accessory); the chick wears the bow as a hair bow.
  // Omamori hangs on the chest; the hachimaki ties round the forehead, its ends trailing behind.
  static const int8_t OFF[3][HAT_COUNT][2] = {
      /*chick*/ {{0, 0}, {0, 0}, {3, 3}, {2, 5}, {7, 6}, {0, 0}, {5, 6}, {2, 3}},
      /*juv  */ {{0, 0}, {11, 9}, {7, 4}, {6, 7}, {12, 8}, {3, 4}, {9, 8}, {6, 4}},
      /*adult*/ {{0, 0}, {13, 11}, {10, 5}, {9, 8}, {15, 10}, {6, 4}, {11, 10}, {9, 5}},
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
    nextCard_ = now + 60000;
    nextSay_ = now + 5 * 60 * 1000;
  }

  handleButton(env);

  // Events from state changes.
  if (lastStage_ == Stage::Egg && s.stage == Stage::Chick) startAnim(Anim::Hatch, now);
  lastStage_ = s.stage;
  if (env.online && !lastOnline_ && s.stage != Stage::Egg && s.stage != Stage::Gone && screen_ != Screen::Focus &&
      !inGame())
    startAnim(Anim::Greet, now);
  lastOnline_ = env.online;

  // Screens that override everything else.
  Screen forced = baseScreen(env);
  // A Wi-Fi blip doesn't end a focus session or a game: they run on the dongle's own clock.
  bool ride = forced == Screen::Sleep &&
              (screen_ == Screen::Focus || inGame() || (screen_ == Screen::Anim && anim_ == Anim::FocusDone));
  if (!ride && (forced == Screen::Ota || forced == Screen::Pair || forced == Screen::Gone ||
                forced == Screen::Setup || forced == Screen::Sleep)) {
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
    case Screen::Say:
      if (now - t0_ > (Frame::textWidth(sayText_) <= SAY_FIT ? SAY_STATIC_MS : 2 * sayPassMs())) screen_ = forced;
      break;
    case Screen::Anim:
      if (anim_ == Anim::Scritch && holding_) break;
      if (anim_ == Anim::Scritch && now - t0_ > ANIM_MS) {  // a happy bird sometimes says a word afterwards
        screen_ = forced;
        maybeSay(env, true);
        break;
      }
      if (now - t0_ > (anim_ == Anim::Hatch ? 3600u : anim_ == Anim::Gift ? 3400u : anim_ == Anim::Omikuji || anim_ == Anim::Occasion || anim_ == Anim::FocusDone ? 6000u :
                       anim_ == Anim::Hug || anim_ == Anim::HomeDay ? 5200u : anim_ == Anim::Countdown ? (crossOff_ ? 5600u : 4500u) : ANIM_MS))
        screen_ = forced;
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
        if (rain_.score() > pet_.state().bestRain) rainRecord_ = rain_.score();
        pet_.recordGame(won, rain_.score());
        pet_.act(won ? Action::GameWin : Action::GameLose, env.epoch);
        dirty_ = true;
        startAnim(won ? Anim::GameWin : Anim::GameLose, now);
      }
      break;
    case Screen::Janken:
      janken_.update(now);
      if (janken_.phase() == Janken::Phase::Done) {
        if (janken_.timedOut()) {
          screen_ = forced;
        } else {
          pet_.recordJanken(janken_.won());
          finishGame(janken_.won(), env);
        }
      }
      break;
    case Screen::Dance:
      dance_.update(now);
      if (dance_.phase() == Dance::Phase::Done) {
        pet_.recordDance(dance_.score());
        finishGame(dance_.score() >= 5, env);
      }
      break;
    case Screen::Focus:
      if ((int32_t)(now - focusEnd_) >= 0) {  // done: a little celebration and a scritch break
        pet_.recordFocus();
        focusDoneMin_ = (int)((focusEnd_ - focusStart_) / 60000);
        dirty_ = true;
        startAnim(Anim::FocusDone, now);
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

  daily(env);
  maybeSay(env, false);

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

void Ui::showHug(const char* from, uint32_t nowMs) {
  snprintf(hugFrom_, sizeof hugFrom_, "%s", from);
  for (char* c = hugFrom_; *c; c++)
    if (*c >= 'a' && *c <= 'z') *c -= 32;
  startAnim(Anim::Hug, nowMs);
}

int Ui::daysToHome(const Date& today) const {
  const Date& h = pet_.extra().home;
  if (!h.y || !today.y) return -1;
  int32_t d = civil(h) - civil(today);
  return d >= 0 ? (int)d : -1;
}

// Once a day, the first time the bird is awake on its main screen: a greeting. In between, every 15 minutes, the
// countdown card. Only from the main screen, so a note waiting in the relay queue never waits long.
void Ui::daily(const Env& env) {
  const PetState& s = pet_.state();
  if (screen_ != Screen::Main || !env.today.y || s.stage == Stage::Egg || s.stage == Stage::Gone ||
      pet_.asleep(env.epoch))
    return;
  uint32_t now = env.nowMs;
  int32_t today = civil(env.today);
  int days = daysToHome(env.today);
  bool first = pet_.extra().greetedDay != today;
  if (first) {
    pet_.extraMut().greetedDay = today;
    dirty_ = true;
  }
  occ_ = occasion(env.today, pet_.extra());
  snprintf(newYear_, sizeof newYear_, "%u", env.today.y);
  if (first && occ_ != Occasion::None) {  // today's greeting; the countdown card follows a little later
    nextCard_ = now + 20000;
    startAnim(Anim::Occasion, now);
    return;
  }
  if (days < 0 || (!first && (int32_t)(now - nextCard_) < 0)) return;
  nextCard_ = now + CARD_EVERY_MS;
  crossOff_ = first && days > 0 && pet_.extra().lastCardDays == days + 1;
  cardDays_ = days;
  if (pet_.extra().lastCardDays != days) {
    pet_.extraMut().lastCardDays = days;
    dirty_ = true;
  }
  startAnim(days == 0 ? Anim::HomeDay : Anim::Countdown, now);
}

void Ui::say(const char* word, bool learning, uint32_t nowMs) {
  if (learning) {  // the first half, then a puzzled "..?"
    int n = (int)strlen(word), keep = n > 2 ? (n + 1) / 2 : 1;
    snprintf(sayText_, sizeof sayText_, "%.*s..?", keep, word);
  } else {
    snprintf(sayText_, sizeof sayText_, "%s", word);
  }
  screen_ = Screen::Say;
  t0_ = nowMs;
}

void Ui::maybeSay(const Env& env, bool afterScritch) {
  const PetState& s = pet_.state();
  if (screen_ != Screen::Main || !pet_.wordCount() || s.stage == Stage::Egg || s.stage == Stage::Gone ||
      pet_.mood(env.epoch) != Mood::Content)
    return;
  uint32_t now = env.nowMs;
  if (afterScritch ? rng_.range(0, 99) >= 35 : (int32_t)(now - nextSay_) < 0) return;
  nextSay_ = now + rng_.range(SAY_MIN_MS, SAY_MAX_MS);
  int pick = rng_.range(0, pet_.wordCount() - 1);
  for (const auto& w : pet_.extra().words) {
    if (!w.text[0] || pick--) continue;
    say(w.text, env.epoch < w.at + rules::DAY, now);
    return;
  }
}

uint32_t Ui::sayPassMs() const {
  int over = Frame::textWidth(sayText_) - SAY_FIT;
  return SAY_LEAD_MS + (uint32_t)(over > 0 ? over : 0) * SAY_SCROLL_MS + SAY_HOLD_MS;
}

void Ui::drawSay(const Env& env) {
  uint32_t el = env.nowMs - t0_;
  // A full-width bubble across the top; the bird's head peeks up from below, talking.
  const int bh = 11;
  int w = Frame::textWidth(sayText_), x;
  if (w <= SAY_FIT) {
    x = (LCD_W - w + 1) / 2;
  } else {  // scroll slowly to the end, hold it there, then once more from the start
    uint32_t p = el % sayPassMs();
    int off = p < SAY_LEAD_MS ? 0 : (int)((p - SAY_LEAD_MS) / SAY_SCROLL_MS);
    x = 2 - (off < w - SAY_FIT ? off : w - SAY_FIT);
  }
  // The bird sits behind the bubble with its face showing underneath, beak just below the tail.
  int sh = pet_.state().stage == Stage::Chick ? spr::chick_idle.h : pet_.state().stage == Stage::Juvenile ? spr::juv_idle.h
                                                                                                         : spr::adult_idle.h;
  bool talk = (el / 180) % 2;
  drawBird(talk ? Pose::Happy : Pose::Idle, 4, bh - 4 + sh, false);
  frame_.fillRect(0, 0, LCD_W, bh, false);
  frame_.text(sayText_, x, 3);
  frame_.fillRect(0, 0, 2, bh, false);  // clip to one dot inside the border
  frame_.fillRect(LCD_W - 2, 0, 2, bh, false);
  frame_.rect(0, 0, LCD_W, bh);
  frame_.set(0, 0, false);  // rounded corners
  frame_.set(LCD_W - 1, 0, false);
  frame_.set(0, bh - 1, false);
  frame_.set(LCD_W - 1, bh - 1, false);
  // The bubble's tail points down at the beak.
  int tx = 4 + birdW() - 4;
  frame_.set(tx, bh, true);
  frame_.set(tx - 1, bh, true);
  frame_.set(tx - 1, bh - 1, false);
  frame_.set(tx, bh - 1, false);
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
    case Game::Janken: janken_.start(rng_.next(), nowMs); screen_ = Screen::Janken; break;
    case Game::Dance: dance_.start(rng_.next(), nowMs); screen_ = Screen::Dance; break;
  }
  t0_ = nowMs;
  return true;
}

void Ui::finishGame(bool won, const Env& env) {
  pet_.act(won ? Action::GameWin : Action::GameLose, env.epoch);
  dirty_ = true;
  startAnim(won ? Anim::GameWin : Anim::GameLose, env.nowMs);
}

void Ui::gameInput(Press p, uint32_t nowMs) {
  if (screen_ == Screen::Janken) {
    if (p >= Press::One) janken_.input(Janken::Hand(uint8_t(p) - uint8_t(Press::One)), nowMs);
    return;
  }
  if (screen_ == Screen::Dance) {
    Dance::Move m = p == Press::Left ? Dance::Left : p == Press::Right ? Dance::Right : p == Press::Up ? Dance::Hop
                                                                                                    : Dance::Bow;
    if (p == Press::Left || p == Press::Right || p == Press::Up || p == Press::Down) dance_.input(m, nowMs);
    return;
  }
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
    case Screen::Say: drawSay(env); break;
    case Screen::Janken: drawJanken(env); break;
    case Screen::Dance: drawDance(env); break;
    case Screen::Focus: drawFocus(env); break;
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
  Weather w = weather(now);
  if (w == Weather::Rain && pose != Pose::Sleep) {  // the umbrella goes behind the bird, over its head
    int hx = facingLeft_ ? birdX_ + birdW() - (birdW() * 2) / 3 - 5 : birdX_ + (birdW() * 2) / 3 - 5;
    frame_.sprite(spr::umbrella, hx, y - (s.stage == Stage::Adult ? 20 : s.stage == Stage::Juvenile ? 17 : 14));
  }
  drawBird(pose, birdX_, y, facingLeft_);
  drawDecor(env, birdX_, y, facingLeft_);
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
        const Sprite* items[] = {nullptr, &spr::hat_bow, &spr::hat_crown, &spr::hat_beret, &spr::hat_party, &spr::hat_sakura,
                            &spr::hat_omamori, &spr::hat_hachimaki};
        if (giftItem_ > 0 && giftItem_ < HAT_COUNT && el < 2200)
          frame_.sprite(*items[giftItem_], LCD_W - 9, 2, false, Pattern::Solid, 2);  // what was inside
        else if (giftItem_ <= 0)
          frame_.sprite(spr::seeds, LCD_W - 7, 4, false, Pattern::Solid, 1);
        frame_.sprite(spr::sparkle, alt ? 1 : LCD_W - 6, 8);
      }
      break;
    case Anim::Love: {
      drawBird(Pose::Happy, cx - 6, LCD_H - (alt ? 1 : 0), false);
      // Three hearts drift up and to the right, one after another.
      for (int i = 0; i < 3; i++) {
        int t = int(el) - i * 450;
        if (t < 0) continue;
        int y = 12 - t / 90;
        if (y > -4) frame_.sprite(spr::heart_s, cx + birdW() - 4 + i * 4 + (y % 4 < 2 ? 0 : 1), y);
      }
      break;
    }
    case Anim::Countdown: {
      // The ship rocks on the waves; the number of days to go next to it.
      int bob = (el / 500) % 2;
      frame_.sprite(spr::ship, 2, 10 + bob);
      for (int x = 0; x < 17; x++) frame_.set(x, 17 + ((x + el / 250) % 4 < 2 ? 0 : 1), true);
      // First card of the day: yesterday's number, a thick X drawn across it, a pause, then today's pops in.
      char buf[8];
      bool old = crossOff_ && el < 2600;
      snprintf(buf, sizeof buf, "%d", old ? cardDays_ + 1 : cardDays_);
      int w = Frame::textWidth(buf, 2), x = 29 - w / 2;
      int pop = crossOff_ && el >= 2600 && el < 2800 ? 1 : 0;
      frame_.text(buf, x, 2 - pop, 2);
      if (old && el >= 600) {
        int len = el >= 1200 ? 10 : (int)(el - 600) * 10 / 600;
        // Each stroke is two dots of ink with a cleared dot either side, so it stands out from the digits.
        for (int pass = 0; pass < 2; pass++)
          for (int i = 0; i < len; i++) {
            int a = x - 2 + i * (w + 3) / 10, b = 2 * x + w - 4 - a;  // the two diagonals
            if (pass == 0) {
              frame_.fillRect(a - 1, 2 + i, 4, 1, false);
              frame_.fillRect(b - 1, 2 + i, 4, 1, false);
            } else {
              frame_.fillRect(a, 2 + i, 2, 1);
              frame_.fillRect(b, 2 + i, 2, 1);
            }
          }
      }
      const char* unit = cardDays_ == 1 ? "DAY" : "DAYS";
      frame_.text(unit, 29 - Frame::textWidth(unit) / 2, 14);
      return;
    }
    case Anim::HomeDay:
      if (el < 2000) {
        frame_.textCentered("HOME", 2, 2);
        frame_.textCentered("TODAY!", 14);
      } else {
        frame_.sprite(spr::house, 1, LCD_H - spr::house.h);
        drawBird(Pose::Happy, 12, LCD_H - (alt ? 1 : 0), false);
        frame_.sprite(spr::heart, LCD_W - 7, alt ? 2 : 4);
        if (alt) frame_.sprite(spr::sparkle, 31, 10);
      }
      return;
    case Anim::Omikuji:
      drawOmikuji(el);
      return;
    case Anim::Occasion:
      drawOccasionAnim(el);
      return;
    case Anim::FocusDone:
      if (el < 2500) {
        frame_.textCentered("DONE!", 2, 2);
        frame_.textCentered("BREAK TIME", 14);
      } else {  // asks for a scritch
        drawBird(Pose::Happy, cx - 6, LCD_H - (alt ? 1 : 0), false);
        frame_.sprite(spr::heart, cx + birdW() - 4, 2);
        frame_.sprite(spr::question, cx + birdW() + 3, 2);
      }
      return;
    case Anim::Hug:
      if (el < 1800) {  // who it is from
        frame_.textCentered("HUG!", 2, 2);
        frame_.textCentered(hugFrom_[0] ? hugFrom_ : "FOR YOU", 14);
        if (alt) frame_.sprite(spr::heart_s, 0, 3);
        else frame_.sprite(spr::heart_s, LCD_W - 5, 3);
      } else {  // the bird holds a big heart that beats
        bool beat = (el / 400) % 2;
        int bx = cx - 7, hx = bx + birdW() + 1, hs = beat ? 2 : 1;  // the heart sits just past the beak
        drawBird(Pose::Happy, bx, LCD_H, false);
        frame_.sprite(spr::heart, hx + (beat ? 0 : 2), beat ? 5 : 8, false, Pattern::Solid, hs);
        if (beat) frame_.sprite(spr::sparkle, LCD_W - 6, 1);
      }
      return;
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
  char buf[12];
  snprintf(buf, sizeof buf, "%d", r.score());
  frame_.text(buf, 1, 1);
  int rival = pet_.extra().rivalRain;
  if (rival && env.nowMs - t0_ < 1600) {  // the first moments: Matthieu's Feather Catch score to beat
    snprintf(buf, sizeof buf, "BEAT %d", rival);
    frame_.fillRect(5, 0, 30, 7, false);
    frame_.textCentered(buf, 1);
  }
  if (!(rival && env.nowMs - t0_ < 1600))  // lives, once the intro has gone
    for (int i = 0; i < r.lives(); i++) frame_.sprite(spr::heart_s, LCD_W - 4 - i * 4, 0);
}

void Ui::drawLights(const Env& env) {
  frame_.fill();
  int phase = (env.nowMs / 600) % 3;
  frame_.spriteInverse(spr::zz, 18 + phase * 3, 8 - phase * 2);
}

// ---------------------------------------------------------------- 0.4 games

namespace {
const Sprite* const HANDS[] = {&spr::hand_rock, &spr::hand_scissors, &spr::hand_paper};
}

void Ui::drawJanken(const Env& env) {
  const Janken& j = janken_;
  uint32_t el = j.phaseMs(env.nowMs);
  if (j.phase() == Janken::Phase::Reveal) {
    // Her hand on the left, the bird's on the right, the result underneath.
    frame_.sprite(*HANDS[j.you()], 3, 0, false, Pattern::Solid, 2);
    if (el > 250) frame_.sprite(*HANDS[j.bird()], LCD_W - 17, 0, true, Pattern::Solid, 2);
    frame_.text("VS", 17, 4);
    if (el > 600) frame_.textCentered(j.last() > 0 ? "YOU WIN" : j.last() < 0 ? "OKAME!" : "AIKO!", 15);
  } else {
    // "JAN... KEN... PON!" in big letters while she picks; the three hands bounce underneath, one after another.
    static const char* const CHANT[] = {"JAN", "KEN", "PON"};
    int k = (el / 600) % 3;
    frame_.textCentered(CHANT[k], 1, 2);
    for (int i = 0; i < 3; i++) frame_.sprite(*HANDS[i], 5 + i * 11, 12 - (i == k ? 1 : 0));
  }
  // Score in the top corners: her wins on the left, the bird's on the right.
  if (j.phase() != Janken::Phase::Reveal) {
    for (int i = 0; i < j.wins(); i++) frame_.sprite(spr::heart_s, 1 + i * 4, 1);
    for (int i = 0; i < j.losses(); i++) frame_.sprite(spr::heart_s, LCD_W - 4 - i * 4, 1);
  }
}

void Ui::drawDance(const Env& env) {
  const Dance& d = dance_;
  uint32_t now = env.nowMs;
  int cx = (LCD_W - birdW()) / 2;
  int move = d.phase() == Dance::Phase::Showing ? d.showing() >= 0 ? d.move(d.showing()) : -1 : d.echo(now);
  int x = cx, y = LCD_H;
  bool flip = false;
  Pose pose = Pose::Idle;
  switch (move) {
    case Dance::Left: x -= 6; flip = true; break;
    case Dance::Right: x += 6; break;
    case Dance::Hop: y -= 3; pose = Pose::Happy; break;
    case Dance::Bow: pose = Pose::Peck; break;
    default: break;
  }
  if (d.phase() == Dance::Phase::Fail) pose = Pose::Flat;
  drawBird(pose, x, y, flip);
  // Move counter top-left: one dot per move in this round, filled as they're danced or copied.
  for (int i = 0; i < d.length(); i++) {
    bool on = d.phase() == Dance::Phase::Showing ? i <= d.showing() : i < d.entered();
    frame_.rect(1 + i * 3, 1, 2, 2, true);
    if (!on) frame_.set(1 + i * 3, 1, false);
  }
  if (d.phase() == Dance::Phase::Listening && d.echo(now) < 0 && blink(now, 400))
    frame_.sprite(spr::question, LCD_W - 5, 2);
  if (d.phase() == Dance::Phase::Good) frame_.sprite(spr::heart, LCD_W - 7, 2);
  if (d.phase() == Dance::Phase::Fail) frame_.sprite(spr::cloud, LCD_W - 8, 1);
}

void Ui::drawOmikuji(uint32_t el) {
  int cx = (LCD_W - birdW()) / 2;
  if (el < 1500) {  // the bird shakes the six-sided box
    int shake = (el / 120) % 2 ? 1 : -1;
    drawBird(Pose::Happy, cx - 8, LCD_H, false);
    frame_.sprite(spr::mikuji_box, cx + birdW() - 4 + shake, 6 - (shake > 0));
    return;
  }
  if (el < 2300) {  // a stick slides out
    drawBird(Pose::Up, cx - 8, LCD_H, false);
    int bx = cx + birdW() - 4;
    frame_.sprite(spr::mikuji_box, bx, 6);
    int out = (int)(el - 1500) / 120;
    frame_.vline(bx + 2, 6 - out, out < 6 ? out : 6);
    return;
  }
  // The fortune, two kanji at double size: 大吉, 中吉, 小吉, 吉, 末吉, 凶.
  static const Sprite* const FIRST[] = {&spr::kj_dai, &spr::kj_chu, &spr::kj_sho, nullptr, &spr::kj_sue, nullptr};
  int g = omikuji_ < 0 || omikuji_ > 5 ? 3 : omikuji_;
  const Sprite* second = g == 5 ? &spr::kj_kyo : &spr::kj_kichi;
  if (FIRST[g]) {
    frame_.sprite(*FIRST[g], 1, 1, false, Pattern::Solid, 2);
    frame_.sprite(*second, 21, 1, false, Pattern::Solid, 2);
  } else {
    frame_.sprite(*second, 11, 1, false, Pattern::Solid, 2);
  }
  bool alt = (el / 300) % 2;
  if (g == 0) {  // 大吉: sparkles
    frame_.sprite(spr::sparkle, alt ? 0 : LCD_W - 5, alt ? 14 : 0);
  } else if (g == 5 && el > 4000) {  // 凶: the bird comforts her
    frame_.clear();
    drawBird(Pose::Happy, cx - 4, LCD_H, false);
    frame_.sprite(spr::heart, cx + birdW() - 2, alt ? 2 : 3);
  }
}

// ---------------------------------------------------------------- seasons and weather

void Ui::drawDecor(const Env& env, int bx, int by, bool flip) {
  uint32_t now = env.nowMs;
  bool alt = (now / 400) % 2;
  int top = by - (pet_.state().stage == Stage::Adult ? 17 : pet_.state().stage == Stage::Juvenile ? 14 : 11);
  int headX = flip ? bx + 1 : bx + birdW() - 6;
  switch (weather(now)) {
    case Weather::Rain:
      for (int i = 0; i < 5; i++) {  // drops falling past the umbrella
        int x = 2 + i * 8 + (i % 2) * 3, yy = int((now / 70 + i * 7) % 24) - 3;
        frame_.vline(x, yy, 2);
      }
      break;
    case Weather::Snow:
      for (int i = 0; i < 4; i++) {
        int x = 3 + i * 10 + ((now / 600 + i) % 2), yy = int((now / 180 + i * 6) % 24) - 4;
        frame_.set(x, yy, true);
        frame_.set(x + 1, yy + 1, true);
      }
      // fall through: snow is cold too
    case Weather::Cold:
      if (alt) {
        frame_.sprite(spr::shiver, flip ? bx + birdW() + 1 : bx - 3, by - 7);
        frame_.sprite(spr::shiver, flip ? bx - 3 : bx + birdW() + 1, by - 7);
      }
      break;
    case Weather::Hot:
      frame_.sprite(spr::sun, LCD_W - 10, 1);
      frame_.sprite(spr::sweat, flip ? headX - 3 : headX + 6, top + 4 + (alt ? 1 : 0));
      break;
    default:
      break;
  }
  // A small seasonal touch in the corner (top right, clear of the attention mark).
  switch (occ_) {
    case Occasion::Sakura:
      for (int i = 0; i < 3; i++) {
        int x = 6 + i * 13 + ((now / 500 + i) % 3), yy = int((now / 220 + i * 7) % 24) - 3;
        frame_.sprite(spr::petal, x, yy);
      }
      break;
    case Occasion::Birthday: frame_.sprite(spr::cake, LCD_W - 12, LCD_H - 7); break;
    case Occasion::Christmas: frame_.sprite(spr::tree, LCD_W - 10, LCD_H - 9); break;
    case Occasion::Halloween: frame_.sprite(spr::pumpkin, LCD_W - 10, LCD_H - 6); break;
    case Occasion::NewYear:
    case Occasion::YearEnd: frame_.sprite(spr::kadomatsu, LCD_W - 10, LCD_H - 9); break;
    case Occasion::Kodomo: frame_.sprite(spr::koi, LCD_W - 14 + (alt ? 1 : 0), 1); break;
    case Occasion::Tanabata: frame_.sprite(spr::star, LCD_W - 9, alt ? 1 : 2); break;
    case Occasion::Sinterklaas: frame_.sprite(spr::mitre, LCD_W - 9, 1); break;
    case Occasion::Hina: frame_.sprite(spr::hina, LCD_W - 12, 1); break;
    case Occasion::Anniversary: if (alt) frame_.sprite(spr::heart, LCD_W - 8, 1); break;
    default: break;
  }
}

// The morning greeting on a special day: a word or two and a scene.
void Ui::drawOccasionAnim(uint32_t el) {
  bool alt = (el / 300) % 2;
  int cx = (LCD_W - birdW()) / 2;
  const char* a = "";
  const char* b = "";
  const Sprite* icon = nullptr;
  switch (occ_) {
    case Occasion::Birthday: a = "HAPPY"; b = "B-DAY!"; icon = &spr::cake; break;
    case Occasion::Anniversary: a = "OUR"; b = "DAY \x01"; icon = &spr::heart; break;
    case Occasion::Hina: a = "HINA"; b = "MATSURI"; icon = &spr::hina; break;
    case Occasion::Sakura: a = "SAKURA"; b = "SEASON"; icon = &spr::hat_sakura; break;
    case Occasion::Kodomo: a = "KODOMO"; b = "NO HI"; icon = &spr::koi; break;
    case Occasion::Tanabata: a = "TANA"; b = "BATA"; icon = &spr::star; break;
    case Occasion::Halloween: a = "BOO!"; b = "HALLOWEEN"; icon = &spr::pumpkin; break;
    case Occasion::Sinterklaas: a = "DAG"; b = "SINT!"; icon = &spr::mitre; break;
    case Occasion::Christmas: a = "MERRY"; b = "XMAS!"; icon = &spr::tree; break;
    case Occasion::NewYear: a = "AKEOME!"; b = newYear_; icon = &spr::kadomatsu; break;
    case Occasion::YearEnd: a = "YOI"; b = "OTOSHI O!"; icon = &spr::kadomatsu; break;
    default: break;
  }
  if (occ_ == Occasion::Tanabata && el >= 2600) {
    // Orihime and Hikoboshi, two stars kept apart by the Milky Way, cross it to meet once a year.
    for (int x = 0; x < LCD_W; x += 2) frame_.set(x + ((x / 2) % 2), 9 + ((x * 7) % 3), true);
    int t = (int)(el - 2600) / 120, d = t < 14 ? 14 - t : 0;
    frame_.sprite(spr::star, 4 + 14 - d, 2);
    frame_.sprite(spr::star, LCD_W - 9 - 14 + d, 13);
    if (!d) frame_.sprite(spr::heart_s, 18, 7);
    return;
  }
  if (el < 2600) {  // the words, the second line in the special case of a birthday name
    frame_.textCentered(a, 1, Frame::textWidth(a, 2) <= LCD_W ? 2 : 1);
    const char* who = pet_.extra().who;
    char name[16];
    snprintf(name, sizeof name, "%s!", who);
    frame_.textCentered(occ_ == Occasion::Birthday && el > 1300 && who[0] ? name : b, 13);
    return;
  }
  // Then the bird celebrates next to the day's picture.
  drawBird(Pose::Happy, cx - 8, LCD_H - (alt ? 1 : 0), false);
  int k = icon && icon->w <= 9 ? 2 : 1;  // wide pictures (the carp) stay small so they clear the bird
  if (icon) frame_.sprite(*icon, LCD_W - 2 - icon->w * k, k == 2 ? LCD_H - icon->h * 2 : 2, false, Pattern::Solid, k);
  if (alt) frame_.sprite(spr::sparkle, LCD_W - 6, 0);
}

// ---------------------------------------------------------------- focus buddy

bool Ui::startFocus(int minutes, uint32_t nowMs) {
  const PetState& s = pet_.state();
  if (s.stage == Stage::Egg || s.stage == Stage::Gone || s.lightsOff || minutes < 1 || minutes > 90) return false;
  if (screen_ != Screen::Main && screen_ != Screen::Anim && screen_ != Screen::Say) return false;
  focusStart_ = nowMs;
  focusEnd_ = nowMs + (uint32_t)minutes * 60000u;
  screen_ = Screen::Focus;
  t0_ = nowMs;
  return true;
}

void Ui::stopFocus(uint32_t nowMs) {
  if (screen_ == Screen::Focus) {
    screen_ = Screen::Main;
    t0_ = nowMs;
  }
}

void Ui::drawFocus(const Env& env) {
  uint32_t now = env.nowMs;
  // The bird sits by its laptop; now and then it taps a key. Minutes left in big digits.
  bool tap = (now / 350) % 6 == 0;
  drawBird(tap ? Pose::Peck : Pose::Idle, 0, LCD_H, false);
  frame_.sprite(spr::laptop, birdW() - 1, LCD_H - spr::laptop.h);
  if ((now / 500) % 2) frame_.hline(birdW() + 2, LCD_H - 3, 2);  // a cursor blinking on the screen
  int left = (int)((int32_t)(focusEnd_ - now) / 1000);
  char buf[8];
  if (left >= 60) snprintf(buf, sizeof buf, "%d", (left + 59) / 60);
  else snprintf(buf, sizeof buf, "%d", left < 0 ? 0 : left);
  int w = Frame::textWidth(buf, 2);
  frame_.text(buf, LCD_W - w - 1, 1, 2);
  const char* unit = left >= 60 ? "MIN" : "SEC";
  frame_.text(unit, LCD_W - Frame::textWidth(unit) - 1, 12);
}

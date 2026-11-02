// The bird's screen: state machine, events from the page and the relay, wandering. Drawing lives in ui_*.cpp.
#include "ui_internal.h"

using namespace uidetail;

int Ui::birdW() const {
  const Look& l = lookOf(pet_.species());
  Stage st = pet_.state().stage;
  return st == Stage::Egg ? l.egg->w : l.stage[stageIndex(st)][0]->w;
}

int Ui::birdH() const {
  const Look& l = lookOf(pet_.species());
  Stage st = pet_.state().stage;
  return st == Stage::Egg ? l.egg->h : l.stage[stageIndex(st)][0]->h;
}

int Ui::headX(int bx, bool flip) const {
  if (!lookOf(pet_.species()).sideways) return bx + birdW() / 2 - 3;
  return flip ? bx + 1 : bx + birdW() - 6;
}

void Ui::drawBird(Pose pose, int x, int y, bool flip) {
  const PetState& s = pet_.state();
  const Look& l = lookOf(pet_.species());
  const Sprite* sp = l.egg;
  Pattern pat = Pattern::Solid;
  if (s.stage != Stage::Egg) {
    sp = l.stage[stageIndex(s.stage)][uint8_t(pose)];
    if (pet_.golden() && s.stage == Stage::Adult) {
      pat = Pattern::Clear;  // lutino: a pale bird, no grey
    } else if (s.stage != Stage::Chick && s.stage != Stage::Juvenile) {
      switch (s.variant) {
        case Variant::Pearl: pat = Pattern::Pearl; break;
        case Variant::Pied: pat = Pattern::Pied; break;
        case Variant::Scruffy: pat = Pattern::Scruffy; break;
        default: pat = Pattern::Solid; break;
      }
    }
  }
  // Bottom-align so feet stay on the floor whatever the sprite height.
  frame_.sprite(*sp, x, y - sp->h, flip, pat);
  drawHat(pose, x, y, flip);
  drawCane(pose, x, y, flip);
}

// A wise old bird leans on a little walking stick, just in front of it.
void Ui::drawCane(Pose pose, int x, int y, bool flip) {
  if (!pet_.senior() || pet_.state().stage != Stage::Adult) return;
  const Sprite& c = spr::cane;  // drawn with its hook towards the bird, a dot clear of the body
  int w = birdW();
  int cx = flip ? x - c.w - 1 : x + w + 1;
  if (!lookOf(pet_.species()).sideways) cx = x + w;  // the owl holds it at its side
  frame_.sprite(c, cx, y - c.h, !flip);
}

// Accessories sit on fixed spots of each life stage's sprite (facing right): bow tie, scarf and medal at the neck,
// sunglasses over the eye, a sakura flower tucked into the feathers behind the eye. Skipped while pecking.
void Ui::drawHat(Pose pose, int x, int y, bool flip) {
  const PetState& s = pet_.state();
  if (!s.hat || s.hat >= HAT_COUNT || s.stage == Stage::Egg || pose == Pose::Peck) return;
  const Sprite* items[] = {nullptr, &spr::hat_bow, &spr::hat_crown, &spr::hat_beret, &spr::hat_party, &spr::hat_sakura,
                            &spr::hat_omamori, &spr::hat_hachimaki};
  const Sprite& a = *items[s.hat];
  int sp = uint8_t(pet_.species()), st = stageIndex(s.stage);
  const Sprite* body = lookOf(pet_.species()).stage[st][0];
  // The flower clashes with a flattened crest.
  if (s.hat == HAT_SAKURA && sp == 0 && st == 2 && (pose == Pose::Flat || pose == Pose::Sleep)) return;
  int ax = OFF[sp][st][s.hat][0], ay = OFF[sp][st][s.hat][1];
  bool front = !lookOf(pet_.species()).sideways;
  // Seen from the front, the owl wears its sunglasses over both eyes.
  if (s.hat == HAT_CROWN && front) frame_.sprite(a, x + body->w - ax - a.w, y - body->h + ay, !flip);
  if (flip) ax = body->w - ax - a.w;
  // A charm on the owl's barred chest gets a clear rim, or it vanishes into the feathers.
  if (front && (s.hat == HAT_PARTY || s.hat == HAT_OMAMORI)) frame_.fillRect(x + ax - 1, y - body->h + ay, a.w + 2, a.h + 1, false);
  frame_.sprite(a, x + ax, y - body->h + ay, flip);
}

void Ui::startAnim(Anim a, uint32_t nowMs) {
  anim_ = a;
  screen_ = Screen::Anim;
  t0_ = nowMs;
}

void Ui::remoteAction(Action a, Result r, uint32_t nowMs) {
  if (r == Result::Away) return;  // nobody home: the cage just shows "OUT"
  if (!canReact()) return;        // the care still counts; a game, a focus session or a once-only moment plays on
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
    nextFriend_ = now + rng_.range(20, 40) * 60 * 1000;
    nextTrick_ = now + rng_.range(30, 60) * 60 * 1000;
    nextToy_ = now + rng_.range(20, 50) * 60 * 1000;
    nextFamily_ = now + rng_.range(10, 30) * 60 * 1000;
  }
  epoch_ = env.epoch;

  handleButton(env);

  // Events from state changes.
  if (lastStage_ == Stage::Egg && s.stage == Stage::Chick) startAnim(Anim::Hatch, now);
  lastStage_ = s.stage;
  if (env.online && !lastOnline_ && s.stage != Stage::Egg && s.stage != Stage::Gone && screen_ != Screen::Focus &&
      !inGame() && !pet_.away(env.epoch))
    startAnim(Anim::Greet, now);
  lastOnline_ = env.online;

  // Screens that override everything else.
  Screen forced = baseScreen(env);
  // A Wi-Fi blip doesn't end a focus session or a game: they run on the dongle's own clock.
  bool ride = forced == Screen::Sleep &&
              (screen_ == Screen::Focus || inGame() ||
               (screen_ == Screen::Anim && (anim_ == Anim::FocusDone || anim_ == Anim::Home)));
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
      if (now - t0_ > animMs()) {
        screen_ = forced;
        if (anim_ == Anim::Calm && pendingTrick_ >= 0) {  // settled down: now the trick it just learned
          int k = pendingTrick_;
          pendingTrick_ = -1;
          if (screen_ == Screen::Main) {
            showTrick(k, now);
            newTrick_ = true;
          }
        }
      }
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
    case Screen::LeftRight:
      lr_.update(now);
      if (lr_.phase() == LeftRight::Phase::Done) {
        if (lr_.timedOut()) {
          screen_ = forced;
        } else {
          pet_.recordLeftRight(lr_.hits());
          finishGame(lr_.won(), env);
        }
      }
      break;
    case Screen::Clock:
      if (now - t0_ > 8000) screen_ = forced;
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
        if (env.hour >= 19) owlAt_ = now | 1;  // an evening of work: the owl comes to sit with the bird after
        dirty_ = true;
        startAnim(Anim::FocusDone, now);
      }
      break;
    case Screen::Main:
    case Screen::Lights:
      screen_ = forced;
      break;
    case Screen::Message: {
      // The pigeon drops the envelope, it bounces a moment, then the text scrolls past twice.
      int w = Frame::textWidth(msgText_, 2);
      uint32_t scrollMs = (uint32_t)(LCD_W + w) * 55;
      if (now - t0_ > MSG_INTRO_MS + 2 * scrollMs) {
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

  checkAlarm(env);
  maybeLife(env);
  daily(env);
  maybeSay(env, false);
  maybeFriend(env);

  // Wander on the main screen.
  if ((int32_t)(now - nextMove_) >= 0) {
    nextMove_ = now + 700;
    hop_ = false;
    Mood m = pet_.mood(env.epoch);
    if (m == Mood::Content || m == Mood::Meh) {
      uint32_t r = rng_.range(0, 99);
      int maxX = LCD_W - birdW() - 1 - 5 * pet_.state().poop;
      int minX = pet_.home().toy ? toyW() + 1 : 1;  // the toy stands at the left
      if (r < 15) {
        facingLeft_ = !facingLeft_;
      } else if (r < 50) {
        birdX_ += facingLeft_ ? -1 : 1;
        if (birdX_ < minX || birdX_ > maxX) {
          facingLeft_ = !facingLeft_;
          birdX_ = birdX_ < minX ? minX : maxX;
        }
      } else if (r < 62 && m == Mood::Content) {
        hop_ = true;
      }
      if (rng_.range(0, 99) < 12) blinkUntil_ = now + 180;
      if (pet_.senior() && m == Mood::Content && rng_.range(0, 999) < 4) dozeUntil_ = now + 5000;  // a little nap
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
  meet(FRIEND_PIGEON);
}

void Ui::showHug(const char* from, uint32_t nowMs) {
  snprintf(hugFrom_, sizeof hugFrom_, "%s", from);
  for (char* c = hugFrom_; *c; c++)
    if (*c >= 'a' && *c <= 'z') *c -= 32;
  startAnim(Anim::Hug, nowMs);
  meet(FRIEND_PIGEON);
}

void Ui::meet(Friend f) {
  if (pet_.recordVisit(f)) newFriend_ = f;
  dirty_ = true;
}

void Ui::friendVisit(Friend f, uint32_t nowMs) {
  friend_ = f < FRIEND_COUNT ? f : FRIEND_BUDGIE;
  meet(friend_);
  // The bird walks over to the left edge to make room, and stays there afterwards.
  fromX_ = birdX_;
  birdX_ = 2;
  facingLeft_ = false;
  startAnim(Anim::Friend, nowMs);
}

// Now and then a friend drops in: at most three a day, a couple of hours apart, only to a bird that is awake,
// well and on its main screen. Who can come depends on the season, the weather and the time of day.
void Ui::maybeFriend(const Env& env) {
  const PetState& s = pet_.state();
  if (screen_ != Screen::Main || !env.today.y || env.hour < 0 || s.stage == Stage::Egg || s.stage == Stage::Gone ||
      pet_.asleep(env.epoch) || pet_.away(env.epoch))
    return;
  Mood m = pet_.mood(env.epoch);
  if (m != Mood::Content && m != Mood::Meh) return;
  uint32_t now = env.nowMs;
  Species own = pet_.species();
  if (owlAt_) {  // after a 大吉 or an evening's work, if the bird is free soon after
    bool fresh = now - owlAt_ < 15 * 60 * 1000u;
    owlAt_ = 0;
    if (fresh && own != Species::Owl) return friendVisit(FRIEND_OWL, now);
  }
  if ((int32_t)(now - nextFriend_) < 0) return;
  nextFriend_ = now + rng_.range(FRIEND_MIN_MS / 60000, FRIEND_MAX_MS / 60000) * 60000;
  PetBirds& b = pet_.birdsMut();
  int32_t day = civil(env.today);
  if (b.visitDay != day) {
    b.visitDay = day;
    b.visitsToday = 0;
  }
  if (b.visitsToday >= FRIEND_PER_DAY) return;
  Friend can[FRIEND_COUNT];
  int n = 0;
  uint8_t mo = env.today.m;
  bool daytime = env.hour >= 9 && env.hour < 19;
  if (own != Species::Budgie && daytime) can[n++] = FRIEND_BUDGIE;
  if (daytime && (b.visitDay % 2 != 0 || own == Species::Budgie)) can[n++] = FRIEND_PIGEON;  // off duty, just saying hello
  if (mo == 12 || mo <= 2 || weather(now) == Weather::Snow) can[n++] = FRIEND_ENAGA;
  if (mo >= 2 && mo <= 4) can[n++] = FRIEND_MEJIRO;
  // The owl keeps its evenings special: every other evening at most.
  if (own != Species::Owl && env.hour >= 19 && civil(env.today) % 2 == 0) can[n++] = FRIEND_OWL;
  if (!n) return;
  b.visitsToday++;
  friendVisit(can[rng_.range(0, n - 1)], now);
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
      pet_.asleep(env.epoch) || pet_.away(env.epoch))
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
      pet_.mood(env.epoch) != Mood::Content || pet_.away(env.epoch))
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
  int sh = birdH();
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
  if (s.stage == Stage::Egg || s.stage == Stage::Gone || s.lightsOff || pet_.away(epoch_)) return false;
  if (screen_ != Screen::Main && screen_ != Screen::Anim && !inGame()) return false;
  switch (g) {
    case Game::Duet: duet_.start(rng_.next(), nowMs); screen_ = Screen::Duet; break;
    case Game::Peek: peek_.start(rng_.next(), nowMs); screen_ = Screen::Peek; break;
    case Game::Rain: rain_.start(rng_.next(), nowMs, birdW()); screen_ = Screen::Rain; break;
    case Game::Janken: janken_.start(rng_.next(), nowMs); screen_ = Screen::Janken; break;
    case Game::Dance: dance_.start(rng_.next(), nowMs); screen_ = Screen::Dance; break;
    case Game::LeftRight: lr_.start(rng_.next(), nowMs); screen_ = Screen::LeftRight; break;
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
  if (screen_ == Screen::LeftRight) {
    if (p == Press::Left || p == Press::Right) lr_.input(p == Press::Right, nowMs);
    return;
  }
  if (screen_ == Screen::Rain) rain_.input(p);
  else if (screen_ == Screen::Duet) duet_.input(p, nowMs);
  else if (screen_ == Screen::Peek) peek_.input(nowMs);
}


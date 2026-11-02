// 0.6: training and tricks, age parties, old age and the golden form, Left or Right?, the clock and alarm.
#include "ui_internal.h"

using namespace uidetail;

// ---------------------------------------------------------------- 0.6: training, old age, the clock

namespace {
constexpr uint32_t TRICK_MS = 3600, NEW_TRICK_MS = 1400;  // a new trick gets a banner first
const char* const WEEKDAYS[] = {"MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};
const char* const MONTHS[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
int weekday(const Date& d) { return (int)((civil(d) % 7 + 7 + 3) % 7); }  // 1970-01-01 was a Thursday; Monday = 0
}  // namespace

uint32_t Ui::animMs() const {
  switch (anim_) {
    case Anim::Hatch: return 3600;
    case Anim::Gift: return 3400;
    case Anim::Omikuji:
    case Anim::Occasion:
    case Anim::FocusDone: return 6000;
    case Anim::Friend: return FRIEND_MS;
    case Anim::Hug: return PIGEON_MS + 5200;
    case Anim::HomeDay: return 5200;
    case Anim::Countdown: return crossOff_ ? 5600 : 4500;
    case Anim::Trick: return TRICK_MS + (newTrick_ ? NEW_TRICK_MS : 0);
    case Anim::Party: return 6500;
    case Anim::Senior: return 5000;
    case Anim::Golden: return 6500;
    case Anim::Wake: return 6000;
    case Anim::Leave: return 2600;
    case Anim::Home: return 6000;
    case Anim::Toy: return 4500;
    case Anim::Partner: return 6000;
    case Anim::NewEgg: return 7000;
    case Anim::Family: return 7000;
    default: return ANIM_MS;
  }
}

// Growing old, birthdays and tantrums: only on the main screen of an awake bird, one thing at a time.
void Ui::maybeLife(const Env& env) {
  const PetState& s = pet_.state();
  if (screen_ != Screen::Main || s.stage == Stage::Egg || s.stage == Stage::Gone || pet_.asleep(env.epoch) ||
      pet_.away(env.epoch))
    return;
  uint32_t now = env.nowMs;
  if (pet_.homeFrom() >= 0) {  // back from an outing: the postcard, then "I'm home!"
    place_ = pet_.homeFrom();
    pet_.greetedHome();
    postcardTaken_ = false;
    dirty_ = true;
    startAnim(Anim::Home, now);
    return;
  }
  if (env.today.y && pet_.eggDue(civil(env.today))) {  // the morning after the partner came: an egg
    pet_.nextGeneration(env.epoch);
    generationOut_ = true;
    dirty_ = true;
    startAnim(Anim::NewEgg, now);
    return;
  }
  PetLife& l = pet_.lifeMut();
  if ((l.golden && !l.goldenShown) || (l.senior && !l.seniorShown)) {  // saved, so a reboot can't lose the moment
    bool gold = l.golden && !l.goldenShown;
    milestoneOut_ = gold ? 2 : 1;
    l.seniorShown = 1;
    if (gold) l.goldenShown = 1;
    dirty_ = true;
    startAnim(gold ? Anim::Golden : Anim::Senior, now);
    return;
  }
  if (uint16_t d = pet_.partyDue(env.epoch)) {
    pet_.markParty(d);
    partyDays_ = partyOut_ = d;
    dirty_ = true;
    startAnim(Anim::Party, now);
    return;
  }
  if (env.today.y && (int32_t)(now - nextFussCheck_) >= 0) {
    nextFussCheck_ = now + 60000;
    if (pet_.maybeFuss(env.epoch, civil(env.today), rng_.next())) dirty_ = true;
  }
  if (pet_.trickCount() && pet_.mood(env.epoch) == Mood::Content && !pet_.fussing(env.epoch) &&
      (int32_t)(now - nextTrick_) >= 0) {
    nextTrick_ = now + rng_.range(60, 150) * 60 * 1000;
    showTrick(-1, now);
    return;
  }
  // Bored, or just now and then: off to play with the toy in the cage.
  if (pet_.home().toy && !pet_.fussing(env.epoch) && (int32_t)(now - nextToy_) >= 0 &&
      (s.happy <= 2 || pet_.mood(env.epoch) == Mood::Content)) {
    nextToy_ = now + rng_.range(40, 90) * 60 * 1000;
    playWithToy(now);
    return;
  }
  // The family drops by: one of the birds in the album, every couple of days, in the daytime.
  const PetHome& h = pet_.home();
  if (h.familyCount && env.hour >= 9 && env.hour < 19 && pet_.mood(env.epoch) == Mood::Content &&
      (int32_t)(now - nextFamily_) >= 0) {
    nextFamily_ = now + rng_.range(30, 60) * 60 * 1000;
    PetLife& l = pet_.lifeMut();
    if (l.familyAt && env.epoch - l.familyAt < 2 * rules::DAY) return;
    l.familyAt = env.epoch;
    family_ = rng_.range(0, h.familyCount - 1);
    fromX_ = birdX_;
    birdX_ = h.toy ? toyW() + 1 : 1;
    facingLeft_ = false;
    dirty_ = true;
    startAnim(Anim::Family, now);
  }
}

// The alarm Haruki set on the page: the bird pops up with a good morning (the relay also sends it to her phone).
// If the bird is busy right then (a game, a note), it greets as soon as it's free, within ten minutes.
void Ui::checkAlarm(const Env& env) {
  PetLife& l = pet_.lifeMut();
  const PetLife::Alarm& a = l.alarm;
  if (!a.on || !env.today.y || env.hour < 0 || env.minute < 0) return;
  int late = (env.hour * 60 + env.minute) - (a.h * 60 + a.m);
  int32_t day = civil(env.today);
  if (late < 0 || late >= 10 || l.alarmDay == day || !((a.days >> weekday(env.today)) & 1)) return;
  const PetState& s = pet_.state();
  if (s.stage == Stage::Egg || s.stage == Stage::Gone) return;
  if (pet_.away(env.epoch)) {  // out: only the good morning on her phone (the relay sends it)
    l.alarmDay = day;
    return;
  }
  bool free = screen_ == Screen::Main || screen_ == Screen::Lights || screen_ == Screen::Say || screen_ == Screen::Clock ||
              (screen_ == Screen::Anim && !onceKind(anim_));
  if (!free) return;
  l.alarmDay = day;
  dirty_ = true;
  startAnim(Anim::Wake, env.nowMs);
}

void Ui::calmed(int newTrick, uint32_t nowMs) {
  pendingTrick_ = newTrick;
  newTrick_ = false;
  startAnim(Anim::Calm, nowMs);
}

bool Ui::showTrick(int trick, uint32_t nowMs) {
  const PetState& s = pet_.state();
  if (s.stage == Stage::Egg || s.stage == Stage::Gone || s.lightsOff || pet_.asleep(epoch_)) return false;
  if (!canReact()) return false;
  if (trick < 0) trick = pet_.pickTrick(rng_.next());
  if (trick < 0 || trick >= TRICK_COUNT || !((pet_.life().tricks >> trick) & 1)) return false;
  trick_ = trick;
  newTrick_ = false;
  startAnim(Anim::Trick, nowMs);
  return true;
}

bool Ui::showClock(uint32_t nowMs) {
  if (screen_ != Screen::Clock && !canReact()) return false;
  screen_ = Screen::Clock;
  t0_ = nowMs;
  return true;
}

void Ui::drawTrick(uint32_t el) {
  int cx = (LCD_W - birdW()) / 2;
  bool alt = (el / 250) % 2;
  switch (trick_) {
    case TRICK_WAVE: {  // one wing up, waving hello
      bool front = !lookOf(pet_.species()).sideways;
      drawBird(alt ? Pose::Happy : Pose::Up, cx, LCD_H, false);
      const Sprite& w = alt ? spr::fr_wing_up : spr::fr_wing_dn;
      int wy = LCD_H - birdH() + birdH() / 3 - (alt ? w.h - 1 : 0);
      if (front) frame_.sprite(w, cx + birdW() - 1, wy, true);
      else frame_.sprite(w, cx + 2, wy, false);
      break;
    }
    case TRICK_SPIN: {  // round and round, hopping
      int k = (int)(el / 180);
      bool flip = k % 2;
      int hop = k % 4 == 1 || k % 4 == 3 ? 2 : 0;
      drawBird(el > 2800 ? Pose::Happy : Pose::Up, cx, LCD_H - hop, flip);
      if (el > 2800) frame_.sprite(spr::sparkle, alt ? 1 : LCD_W - 6, 2);
      break;
    }
    case TRICK_BOW: {  // a deep bow, twice, then a proud little hop
      uint32_t k = el / 700;
      Pose pose = k == 1 || k == 3 ? Pose::Peck : k >= 4 ? Pose::Happy : Pose::Idle;
      drawBird(pose, cx, LCD_H - (k >= 4 && alt ? 1 : 0), false);
      if (k >= 4) frame_.sprite(spr::heart, cx + birdW() + 1, 2);
      break;
    }
    default: {  // hanging upside down from the perch, swinging
      const Look& l = lookOf(pet_.species());
      const Sprite& sp = *l.stage[stageIndex(pet_.state().stage)][uint8_t(Pose::Happy)];
      int swing = (el / 400) % 2 ? 1 : -1;
      frame_.hline(2, 1, LCD_W - 4);
      Pattern pat = pet_.golden() && pet_.state().stage == Stage::Adult ? Pattern::Clear : Pattern::Solid;
      if (pat == Pattern::Solid && pet_.state().stage == Stage::Adult)
        pat = pet_.state().variant == Variant::Pearl ? Pattern::Pearl : pet_.state().variant == Variant::Pied ? Pattern::Pied
              : pet_.state().variant == Variant::Scruffy ? Pattern::Scruffy : Pattern::Solid;
      frame_.spriteUpsideDown(sp, cx + swing, 2, false, pat);
      if (alt) frame_.sprite(spr::note, cx + sp.w + 2, 10);
      break;
    }
  }
}

void Ui::drawLifeAnim(const Env& env, uint32_t el) {
  int cx = (LCD_W - birdW()) / 2;
  bool alt = (el / 300) % 2;
  char buf[16];
  switch (anim_) {
    case Anim::Calm:  // a calm "no": it sulks a moment, then settles down
      if (el < 1200) {
        drawBird(Pose::Flat, cx, LCD_H, false);
        frame_.sprite(spr::sweat, cx + birdW() + 1, LCD_H - birdH() + 2 + (alt ? 1 : 0));
      } else {
        drawBird(Pose::Blink, cx, LCD_H, false);
        frame_.text("OK", LCD_W - 9, 1);
      }
      return;
    case Anim::Trick:
      if (newTrick_) {
        if (el < NEW_TRICK_MS) {
          frame_.textCentered("NEW", 1, 2);
          frame_.textCentered("TRICK!", 13);
          if (alt) frame_.sprite(spr::sparkle, 1, 2);
          return;
        }
        el -= NEW_TRICK_MS;
      }
      drawTrick(el);
      return;
    case Anim::Party:  // "10 DAYS OLD!", then cake, a party hat and confetti
      if (el < 2600) {
        snprintf(buf, sizeof buf, "%d", partyDays_);
        frame_.textCentered(buf, 1, 2);
        frame_.textCentered(partyDays_ == 365 ? "ONE YEAR!" : "DAYS OLD!", 13);
        return;
      }
      drawBird(Pose::Happy, cx - 8, LCD_H - (alt ? 1 : 0), false);
      frame_.sprite(spr::party_hat, headX(cx - 8, false) + 1, LCD_H - birdH() - 4);
      frame_.sprite(spr::cake, LCD_W - 10, LCD_H - spr::cake.h);
      for (int i = 0; i < 5; i++) {
        int x = 2 + i * 8 + (i % 2) * 3, y = (int)((el / 110 + i * 5) % 22) - 2;
        frame_.sprite(spr::confetti, x, y, (el / 400 + i) % 2);
      }
      return;
    case Anim::Senior:  // a wise old bird now, with its walking stick
      if (el < 2400) {
        frame_.textCentered("WISE", 1, 2);
        frame_.textCentered("OLD BIRD", 13);
        return;
      }
      drawBird(alt && el > 3800 ? Pose::Happy : Pose::Idle, cx, LCD_H, false);
      if (alt) frame_.sprite(spr::sparkle, cx + birdW() + 1, 2);
      return;
    case Anim::Golden:  // the secret form: flashing, then the reveal
      if (el < 1800) {
        drawBird(Pose::Up, cx, LCD_H, false);
        if ((el / 150) % 2) frame_.invert();
        return;
      }
      if (el < 3800) {
        frame_.textCentered("GOLDEN", 1, 1);
        frame_.textCentered("BIRD!", 8, 2);
        frame_.sprite(spr::sparkle, alt ? 1 : LCD_W - 6, 1);
        return;
      }
      drawBird(Pose::Happy, cx, LCD_H - (alt ? 1 : 0), false);
      frame_.sprite(spr::sparkle, alt ? cx - 6 : cx + birdW() + 1, alt ? 3 : 7);
      frame_.sprite(spr::sparkle, alt ? cx + birdW() + 1 : cx - 6, alt ? 9 : 1);
      return;
    case Anim::Wake:  // the alarm: good morning!
      if (el < 2400) {
        frame_.textCentered("OHAYO", 1, 2);
        frame_.textCentered("MORNING!", 13);
        return;
      }
      frame_.sprite(spr::sun, LCD_W - 8, 1);
      drawBird(alt ? Pose::Up : Pose::Happy, cx - 4, LCD_H - ((el / 200) % 3 == 1 ? 2 : 0), false);
      if (blink(env.nowMs, 250)) frame_.sprite(spr::exclaim, cx + birdW() - 2, 1);
      return;
    default:
      return;
  }
}

void Ui::drawLeftRight(const Env& env) {
  const LeftRight& g = lr_;
  uint32_t now = env.nowMs, el = g.phaseMs(now);
  int cx = (LCD_W - birdW()) / 2;
  // Score: her right guesses on the left, rounds still to go on the right.
  for (int i = 0; i < g.hits(); i++) frame_.sprite(spr::heart_s, 1 + i * 4, 0);
  for (int i = g.round(); i < LeftRight::ROUNDS; i++) frame_.set(LCD_W - 2 - (LeftRight::ROUNDS - 1 - i) * 2, 1, true);
  if (g.phase() == LeftRight::Phase::Turn) {
    bool r = g.turn();
    int x = cx + (r ? 6 : -6);
    if (el < 200) x = cx + (r ? 3 : -3);
    drawBird(g.hit() ? Pose::Happy : Pose::Flat, x, LCD_H - (el < 300 ? 1 : 0), !r);
    frame_.sprite(r ? spr::arr_r : spr::arr_l, r ? LCD_W - 4 : 1, 8);
    if (el > 300) {
      if (g.hit()) frame_.sprite(spr::heart, r ? 1 : LCD_W - 8, 7);
      else frame_.sprite(spr::cloud, r ? 1 : LCD_W - 9, 6);
    }
    return;
  }
  // Which way will it go? It looks at her, shifting from foot to foot.
  bool shift = (now / 500) % 2;
  drawBird(now % 1700 < 150 ? Pose::Blink : Pose::Idle, cx, LCD_H, shift);
  if (blink(now, 500)) {
    frame_.sprite(spr::arr_l, 1, 8);
    frame_.sprite(spr::arr_r, LCD_W - 4, 8);
  }
  if (!blink(now, 500)) frame_.sprite(spr::question, shift ? cx - 5 : cx + birdW() + 1, 2);
}

void Ui::drawTime(const Env& env, int y, bool on) {
  char buf[8];
  if (env.hour < 0 || env.minute < 0) snprintf(buf, sizeof buf, "--:--");
  else snprintf(buf, sizeof buf, "%02d:%02d", env.hour, env.minute);
  frame_.textCentered(buf, y, 2, on);
}

void Ui::drawClock(const Env& env) {
  drawTime(env, 1, true);
  if (!env.today.y) return;
  char buf[16];
  snprintf(buf, sizeof buf, "%s %d %s", WEEKDAYS[weekday(env.today)], env.today.d, MONTHS[(env.today.m + 11) % 12]);
  frame_.textCentered(buf, 14);
}


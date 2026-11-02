// Drawing: the main screens, menus, stats, animations, games, seasons and the focus buddy.
#include "ui_internal.h"

using namespace uidetail;

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
    case Screen::LeftRight: drawLeftRight(env); break;
    case Screen::Clock: drawClock(env); break;
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
    const Sprite& egg = *lookOf(pet_.species()).egg;
    frame_.sprite(egg, (LCD_W - egg.w) / 2 + wobble, LCD_H - egg.h);
    return;
  }

  if (pet_.away(env.epoch)) return drawAway(env);
  drawToyInCage(now);

  Pose pose = Pose::Idle;
  if (m == Mood::Asleep) pose = Pose::Sleep;
  else if (m == Mood::Sad || m == Mood::Sick) pose = Pose::Flat;
  if ((int32_t)(blinkUntil_ - now) > 0 && pose == Pose::Idle) pose = Pose::Blink;
  if ((int32_t)(dozeUntil_ - now) > 0 && pose == Pose::Idle) pose = Pose::Sleep;
  bool fuss = pet_.fussing(env.epoch) && (m == Mood::Content || m == Mood::Meh);
  if (fuss) pose = (now / 350) % 2 ? Pose::Up : Pose::Flat;  // stamping about, cross
  int y = LCD_H - (hop_ || (fuss && (now / 350) % 4 == 1) ? 1 : 0);
  Weather w = weather(now);
  if (w == Weather::Rain && pose != Pose::Sleep) {  // the umbrella goes behind the bird, over its head
    int hx = headX(birdX_, facingLeft_) + 3 - 5;
    if (lookOf(pet_.species()).sideways)
      hx = facingLeft_ ? birdX_ + birdW() - (birdW() * 2) / 3 - 5 : birdX_ + (birdW() * 2) / 3 - 5;
    frame_.sprite(spr::umbrella, hx, y - birdH() - 3);
  }
  drawBird(pose, birdX_, y, facingLeft_);
  drawDecor(env, birdX_, y, facingLeft_);
  drawPoop(now);
  if (fuss) {
    int hx = headX(birdX_, facingLeft_);
    bool side = lookOf(pet_.species()).sideways;
    int fx = side ? (facingLeft_ ? hx - 6 : hx + 7) : birdX_ + birdW() + 1;
    int fy = side ? y - birdH() : y - birdH() + 3;
    frame_.sprite(spr::fuss, fx, fy < 0 ? 0 : fy);
  }
  if (pet_.golden() && s.stage == Stage::Adult && (now / 900) % 3 == 0)  // a golden glint now and then
    frame_.sprite(spr::sparkle, facingLeft_ ? birdX_ + birdW() : birdX_ - 5, y - birdH() + 1);

  // Status icons in the top-left corner.
  if (m == Mood::Sick && blink(now, 500)) frame_.sprite(spr::ico_vet, 0, 0);
  else if (m == Mood::Sad) frame_.sprite(spr::cloud, 0, 0);
  else if (m == Mood::Asleep && blink(now, 700)) frame_.sprite(spr::zz, 1, 1);
  if (pet_.needsAttention(env.epoch) && blink(now, 400)) frame_.sprite(spr::exclaim, LCD_W - 2, 0);
}

void Ui::drawSleep(const Env& env) {
  uint32_t now = env.nowMs;
  if (pet_.state().stage == Stage::Egg) {
    frame_.sprite(*lookOf(pet_.species()).egg, 8, LCD_H - birdH());
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
      if (el < 2400) {
        const Look& l = lookOf(pet_.species());
        const Sprite& egg = el < 1600 ? *l.egg : *l.crack;
        int wob = el < 1600 ? (alt ? 1 : -1) : 0;
        frame_.sprite(egg, (LCD_W - egg.w) / 2 + wob, LCD_H - egg.h);
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
    case Anim::Friend:
      drawFriend(el);
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
    case Anim::Calm:
    case Anim::Trick:
    case Anim::Party:
    case Anim::Senior:
    case Anim::Golden:
    case Anim::Wake:
      drawLifeAnim(env, el);
      return;
    case Anim::Leave:
    case Anim::Home:
    case Anim::Toy:
    case Anim::Partner:
    case Anim::NewEgg:
    case Anim::Family:
      drawWorldAnim(env, el);
      return;
    case Anim::Hug:
      if (el < PIGEON_MS) return drawDelivery(el, spr::heart_s, (LCD_W - spr::heart_s.w) / 2, 10);
      el -= PIGEON_MS;
      alt = (el / 300) % 2;
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
  if (el < PIGEON_MS) return drawDelivery(el, spr::mail_s, (LCD_W - spr::mail_s.w) / 2, 9);
  if (el < MSG_INTRO_MS) {
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
  int x = LCD_W - (int)(((el - MSG_INTRO_MS) % pass) / 55);
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
  if (pet_.life().clockOnCover && env.hour >= 0) {  // a little clock tag on the cover, zz drifting underneath
    frame_.fillRect(0, 0, LCD_W, 13, false);
    drawTime(env, 2, true);
    frame_.spriteInverse(spr::zz, 17 + phase * 3, 14);
    return;
  }
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
  int top = by - birdH();
  int hx = headX(bx, flip);
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
      frame_.sprite(spr::sweat, flip ? hx - 3 : hx + 6, top + 4 + (alt ? 1 : 0));
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
  if (s.stage == Stage::Egg || s.stage == Stage::Gone || s.lightsOff || minutes < 1 || minutes > 90 ||
      pet_.away(epoch_))
    return false;
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


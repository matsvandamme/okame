// 0.7: cage toys, outings and postcards, partners, new generations and family visits.
#include "ui_internal.h"

using namespace uidetail;

// ---------------------------------------------------------------- 0.7: Okame's world

namespace {
const Sprite* const TOYS[TOY_COUNT] = {&spr::toy_swing, &spr::toy_bell, &spr::toy_mirror, &spr::toy_ladder, &spr::toy_perch};
const bool TOY_HANGS[TOY_COUNT] = {true, true, false, false, false};
const char* const PLACE_NAMES[PLACE_COUNT] = {"PARK", "CAFE", "BEACH", "SHRINE", "WINDMILL", "FRITES"};
Pattern patternOf(Variant v) {
  return v == Variant::Pearl ? Pattern::Pearl : v == Variant::Pied ? Pattern::Pied : v == Variant::Scruffy ? Pattern::Scruffy
                                                                                                            : Pattern::Solid;
}
}  // namespace

// Another grown bird of a given kind and look (a partner, or one of the family), without hats or a walking stick.
void Ui::drawAs(uint8_t species, Variant v, bool golden, Pose pose, int x, int y, bool flip) {
  const Look& l = lookOf(Species(species < uint8_t(Species::Count) ? species : 0));
  const Sprite* sp = l.stage[2][uint8_t(pose)];
  frame_.sprite(*sp, x, y - sp->h, flip, golden ? Pattern::Clear : patternOf(v));
}

int Ui::toyW() const {
  uint8_t t = pet_.home().toy;
  return t && t <= TOY_COUNT ? TOYS[t - 1]->w : 0;
}

void Ui::drawToyInCage(uint32_t nowMs) {
  uint8_t t = pet_.home().toy;
  if (!t || t > TOY_COUNT) return;
  const Sprite& s = *TOYS[t - 1];
  int sway = t - 1 == TOY_SWING && (nowMs / 900) % 2 ? 1 : 0;
  frame_.sprite(s, sway, TOY_HANGS[t - 1] ? 0 : LCD_H - s.h);
}

bool Ui::goOut(int place, uint32_t nowMs) {
  if (screen_ != Screen::Main && !(screen_ == Screen::Anim && !onceKind(anim_))) return false;
  if (!pet_.goOut(uint8_t(place), epoch_, rng_.next())) return false;
  place_ = place;
  dirty_ = true;
  startAnim(Anim::Leave, nowMs);
  return true;
}

bool Ui::playWithToy(uint32_t nowMs) {
  if (!pet_.home().toy || !canReact() || pet_.state().lightsOff || pet_.asleep(epoch_)) return false;
  if (pet_.playToy(epoch_)) dirty_ = true;
  startAnim(Anim::Toy, nowMs);
  return true;
}

bool Ui::partnerVisit(uint32_t nowMs) {
  fromX_ = birdX_;
  birdX_ = pet_.home().toy ? toyW() + 1 : 1;
  facingLeft_ = false;
  startAnim(Anim::Partner, nowMs);
  return true;
}

// Out and about: an empty cage with a note on the door, and where it went.
void Ui::drawAway(const Env& env) {
  int p = pet_.life().outing.place - 1;
  drawToyInCage(env.nowMs);
  int x0 = pet_.home().toy ? toyW() + 2 : 1;
  frame_.text("OUT", x0, 2);
  uint32_t back = pet_.life().outing.back, now = env.epoch;
  int mins = back > now ? (int)((back - now + 59) / 60) : 0;
  char buf[12];
  snprintf(buf, sizeof buf, "%d", mins);
  frame_.text(buf, x0, 9);
  frame_.text("MIN", x0, 15);
  const Sprite* icon[] = {&spr::pl_tree, &spr::pl_cup, &spr::umbrella, &spr::pl_torii, &spr::pl_mill_body, &spr::pl_frites};
  if (p >= 0 && p < PLACE_COUNT) {
    const Sprite& s = *icon[p];
    frame_.sprite(s, LCD_W - s.w - 1, LCD_H - s.h);
    if (p == PLACE_WINDMILL)
      frame_.sprite(spr::pl_mill_x, LCD_W - s.w - 3, LCD_H - s.h - 5);
  }
}

// A postcard: the place on the right, the bird enjoying it on the left.
void Ui::drawPlace(int place, uint32_t el) {
  bool alt = (el / 400) % 2;
  int bx = 0;
  Pose pose = alt ? Pose::Happy : Pose::Idle;
  switch (place) {
    case PLACE_PARK:
      frame_.sprite(spr::pl_tree, LCD_W - spr::pl_tree.w - 1, LCD_H - spr::pl_tree.h);
      frame_.sprite(spr::note, 20, 2 + (alt ? 1 : 0));
      break;
    case PLACE_CAFE:
      frame_.sprite(spr::pl_cup, LCD_W - spr::pl_cup.w, LCD_H - spr::pl_cup.h - (alt ? 0 : 0));
      if (alt) frame_.sprite(spr::heart_s, 22, 2);
      pose = alt ? Pose::Peck : Pose::Idle;
      break;
    case PLACE_BEACH:
      frame_.sprite(spr::sun, LCD_W - 8, 0);
      frame_.sprite(spr::umbrella, LCD_W - spr::umbrella.w - 6, 5);
      for (int i = 0; i < 2; i++) frame_.sprite(spr::pl_wave, 20 + i * 10 - (int)((el / 300) % 4), LCD_H - 3);
      break;
    case PLACE_SHRINE:
      frame_.sprite(spr::pl_torii, LCD_W - spr::pl_torii.w, LCD_H - spr::pl_torii.h);
      for (int i = 0; i < 2; i++)
        frame_.sprite(spr::petal, 19 + i * 9, (int)((el / 200 + i * 9) % 20) - 3);
      break;
    case PLACE_WINDMILL:
      frame_.sprite(spr::pl_mill_body, LCD_W - 10, LCD_H - spr::pl_mill_body.h);
      frame_.sprite((el / 300) % 2 ? spr::pl_mill_x : spr::pl_mill_plus, LCD_W - 12, LCD_H - spr::pl_mill_body.h - 5);
      break;
    default:  // a frietkot: fries!
      frame_.sprite(spr::pl_frites, LCD_W - spr::pl_frites.w - 2, LCD_H - spr::pl_frites.h);
      pose = alt ? Pose::Peck : Pose::Happy;
      break;
  }
  drawBird(pose, bx, LCD_H, false);
}

void Ui::drawWorldAnim(const Env& env, uint32_t el) {
  int cx = (LCD_W - birdW()) / 2;
  bool alt = (el / 300) % 2;
  const PetState& s = pet_.state();
  switch (anim_) {
    case Anim::Leave: {  // a wave, then off it walks to the right
      if (el < 900) {
        drawBird(alt ? Pose::Happy : Pose::Up, cx, LCD_H, false);
        frame_.textCentered("BYE!", 1) ;
        return;
      }
      int x = cx + (int)(el - 900) / 45;
      drawBird((el / 150) % 2 ? Pose::Idle : Pose::Up, x, LCD_H - ((el / 150) % 2), false);
      return;
    }
    case Anim::Home:  // the postcard from where it went, then "I'm home!" with a souvenir
      if (el < 3000) {
        drawPlace(place_, el);
        if (!postcardTaken_) {  // this frame goes to Telegram as a postcard
          postcardTaken_ = true;
          postcardOut_ = place_ + 1;
        }
        return;
      }
      if (el < 4400) {
        frame_.textCentered("TADAIMA!", 2, 1);
        frame_.textCentered("I'M HOME", 10);
        return;
      }
      drawBird(Pose::Happy, cx - 6, LCD_H - (alt ? 1 : 0), false);
      frame_.sprite(spr::bag, cx + birdW() - 4, LCD_H - spr::bag.h);
      frame_.sprite(spr::sparkle, cx + birdW() + 3, 2 + (alt ? 1 : 0));
      return;
    case Anim::Toy: {
      uint8_t t = pet_.home().toy;
      if (!t || t > TOY_COUNT) {
        drawBird(Pose::Happy, cx, LCD_H, false);
        return;
      }
      int k = t - 1;
      const Sprite& toy = *TOYS[k];
      int bx = toy.w + 1;
      switch (k) {
        case TOY_SWING: {  // on the swing, back and forth
          int sw = (el / 350) % 4;
          int off = sw == 0 ? 0 : sw == 1 ? 2 : sw == 2 ? 0 : -2;
          int x = cx + off, top = LCD_H - birdH() - 1;
          frame_.vline(x + 2, 0, top + birdH() / 2);
          frame_.vline(x + birdW() - 4, 0, top + birdH() / 2);
          drawBird(Pose::Happy, x, LCD_H - 1, false);
          frame_.hline(x, LCD_H - 1, birdW());
          return;
        }
        case TOY_BELL: {  // pecks the bell: ding!
          bool hit = (el / 500) % 2;
          frame_.sprite(toy, hit ? 1 : 0, 0);
          drawBird(hit ? Pose::Peck : Pose::Up, bx - 2, LCD_H, true);
          if (hit) frame_.sprite(spr::note, bx + birdW() / 2, 2);
          return;
        }
        case TOY_MIRROR:  // a cockatiel and its reflection: love at first sight
          frame_.sprite(toy, 0, LCD_H - toy.h);
          drawBird(alt ? Pose::Happy : Pose::Idle, bx, LCD_H, true);
          if (el > 1200) frame_.sprite(spr::heart_s, bx + birdW() + 1, 3 + (alt ? 1 : 0));
          return;
        case TOY_LADDER: {  // up and down the ladder
          frame_.sprite(toy, 0, LCD_H - toy.h);
          int up = (int)((el / 300) % 6);
          int y = LCD_H - (up < 3 ? up : 6 - up) * 2;
          drawBird(Pose::Up, bx - 1, y, true);
          return;
        }
        default: {  // hops up onto a little perch and preens
          int px = cx, up = el > 900 ? 3 : (int)el / 300;
          frame_.hline(px + 3, LCD_H - 1 - up, birdW() - 6);   // the bar under its feet
          frame_.vline(px + birdW() / 2, LCD_H - up, up);       // its post
          frame_.hline(px + birdW() / 2 - 2, LCD_H - 1, 5);     // the foot
          drawBird(el > 1000 ? (alt ? Pose::Peck : Pose::Happy) : Pose::Up, px, LCD_H - 1 - up, false);
          return;
        }
      }
    }
    case Anim::Partner: {  // a partner of its own kind flies in; hearts
      uint8_t sp = uint8_t(pet_.species());
      const Sprite& p = *lookOf(pet_.species()).stage[2][0];
      int px = LCD_W - p.w - 1;
      if (el < 1500) px += (int)(1500 - el) / 40;
      drawBird(el > 1500 && alt ? Pose::Happy : Pose::Up, birdX_, LCD_H, false);
      drawAs(sp, Variant::Grey, false, el > 1500 && !alt ? Pose::Happy : Pose::Idle, px, LCD_H, true);
      if (el > 1800) frame_.sprite(spr::heart, (LCD_W - 7) / 2, 1 + (int)((el / 250) % 3));
      return;
    }
    case Anim::NewEgg: {  // the parent with its egg, then it flies off to the family nest
      const PetHome& h = pet_.home();
      if (!h.familyCount) return;
      const PetHome::Member& m = h.family[h.familyCount - 1];
      const Sprite& egg = *lookOf(pet_.species()).egg;
      if (el < 2600) {
        frame_.textCentered("AN EGG!", 2, 1);
        frame_.sprite(egg, (LCD_W - egg.w) / 2, LCD_H - egg.h);
        if (alt) frame_.sprite(spr::sparkle, LCD_W / 2 + 6, 8);
        return;
      }
      int px = 1;
      if (el > 5000) px -= (int)(el - 5000) / 30;  // off to the family album
      frame_.sprite(egg, LCD_W - egg.w - 4, LCD_H - egg.h);
      drawAs(m.species, Variant(m.variant), m.golden, alt ? Pose::Happy : Pose::Idle, px, LCD_H, false);
      if (el < 5000) frame_.sprite(spr::heart_s, px + 20, 3);
      return;
    }
    case Anim::Family: {  // someone from the album drops by
      const PetHome& h = pet_.home();
      if (family_ >= h.familyCount) return;
      const PetHome::Member& m = h.family[family_];
      if (el < 1800) {
        frame_.textCentered("FAMILY!", 2, 1);
        frame_.textCentered(m.name, 10);
        return;
      }
      int bx = birdX_;
      drawBird(alt ? Pose::Happy : Pose::Idle, bx, LCD_H, false);
      const Sprite& a = *lookOf(Species(m.species < uint8_t(Species::Count) ? m.species : 0)).stage[2][0];
      int ax = LCD_W - a.w;
      if (el < 2600) ax += (int)(2600 - el) / 40;
      drawAs(m.species, Variant(m.variant), m.golden, alt ? Pose::Idle : Pose::Happy, ax, LCD_H, true);
      if (el > 3000) frame_.sprite(spr::heart_s, 18, 2 + (alt ? 1 : 0));
      return;
    }
    default:
      (void)env;
      (void)s;
      return;
  }
}


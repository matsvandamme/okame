// Friends who visit, and the pigeon that brings the post.
#include "ui_internal.h"

using namespace uidetail;

// ---------------------------------------------------------------- friends

void Ui::drawFlyer(Friend f, int x, int bottom, bool flip, int wing, const Sprite* look) {
  const FriendLook& fl = FRIENDS[f];
  const Sprite& sp = look ? *look : *fl.sprite;
  int top = bottom - sp.h;
  frame_.sprite(sp, x, top, flip, Pattern::Pearl);  // patterned dots read as colour: green, grey wings
  if (!wing) return;
  const Sprite& w = wing == 1 ? spr::fr_wing_up : spr::fr_wing_dn;
  int wy = wing == 1 ? top + fl.backY - (w.h - 1) : top + fl.backY;
  if (f == FRIEND_OWL) {  // seen from the front: a wing either side
    frame_.sprite(w, x - w.w + 1, wy, false);
    frame_.sprite(w, x + sp.w - 1, wy, true);
    return;
  }
  int wx = flip ? sp.w - fl.backX - w.w : fl.backX;
  frame_.sprite(w, x + wx, wy, flip);
}

// The pigeon flies in from the right with the post under its feet, lets go over (dropX, dropY) and flies on.
void Ui::drawDelivery(uint32_t el, const Sprite& cargo, int dropX, int dropY) {
  const Sprite& pg = spr::fr_pigeon;
  const uint32_t DROP = 1300;
  int wing = (el / 120) % 2 ? 1 : 2;
  int stopX = dropX + cargo.w / 2 - pg.w / 2, bottom = pg.h + 1;
  int x, cy;
  if (el < DROP) {
    x = LCD_W - (int)((LCD_W - stopX) * el / DROP);
    bottom += (el / 200) % 2;
    cy = bottom;
  } else {
    uint32_t t = el - DROP;
    x = stopX - (int)(t / 30);
    cy = bottom + (int)(t / 25);
    bottom -= (int)(t / 70);
    if (cy > dropY) cy = dropY;
    if (el / 250 % 2) frame_.sprite(spr::sparkle, dropX + cargo.w + 2, dropY - 4);
  }
  drawFlyer(FRIEND_PIGEON, x, bottom, true, wing);
  int cx = el < DROP ? x + pg.w / 2 - cargo.w / 2 : dropX;
  frame_.sprite(cargo, cx, cy);
}

void Ui::drawFriend(uint32_t el) {
  const Sprite& sp = *FRIENDS[friend_].sprite;
  // Walk from where the bird stood to the spot it makes room at, as the friend flies in.
  int bx = birdX_;
  if (el < 800 && fromX_ != birdX_) bx = fromX_ + (birdX_ - fromX_) * (int)el / 800;
  bool mejiro = friend_ == FRIEND_MEJIRO;
  // The mejiro sits in a plum branch; everyone else lands on the floor next to the bird.
  if (mejiro) frame_.sprite(spr::fr_branch, LCD_W - spr::fr_branch.w, 7);
  int landX = LCD_W - sp.w - 1, landB = mejiro ? 11 : LCD_H;
  int wing = (el / 120) % 2 ? 1 : 2;
  bool alt = (el / 300) % 2;

  if (el < FRIEND_IN_MS) {  // flying in, down to its spot; the bird looks up
    int p = (int)(el * 100 / FRIEND_IN_MS);
    int x = LCD_W + 1 - (LCD_W + 1 - landX) * p / 100;
    int bottom = 6 + (landB - 6) * p * p / 10000;
    drawBird(Pose::Up, bx, LCD_H, false);
    drawFlyer(friend_, x, bottom, true, wing);
    return;
  }
  if (el >= FRIEND_OUT_MS) {  // off it goes, up and away; the bird waves goodbye
    int q = (int)(el - FRIEND_OUT_MS);
    drawBird(alt ? Pose::Happy : Pose::Idle, bx, LCD_H, false);
    drawFlyer(friend_, landX + q / 60, landB - q / 45, false, wing);
    return;
  }
  uint32_t s = el - FRIEND_IN_MS;
  switch (friend_) {
    case FRIEND_BUDGIE: {  // head-bobbing back and forth, chirping
      bool bob = (s / 350) % 2;
      drawBird(bob ? Pose::Happy : Pose::Idle, bx, LCD_H - (bob ? 1 : 0), false);
      drawFlyer(friend_, landX, landB - (bob ? 0 : 1), true, 0);
      for (int i = 0; i < 2; i++) frame_.sprite(spr::note, 21 + i * 3, 12 - (int)((s / 90 + i * 7) % 14));
      break;
    }
    case FRIEND_ENAGA: {  // a fluffy snuggle in the snow
      int x = landX - (int)(s / 400 < 3 ? s / 400 : 3);
      drawBird(s > 1800 ? Pose::Happy : Pose::Idle, bx, LCD_H, false);
      drawFlyer(friend_, x, landB, true, 0, (s / 600) % 2 ? &spr::fr_enaga_puff : nullptr);
      for (int i = 0; i < 3; i++) {
        int fx = 4 + i * 13 + (int)((s / 600 + i) % 2), fy = (int)((s / 160 + i * 6) % 22) - 3;
        frame_.set(fx, fy, true);
        frame_.set(fx + 1, fy + 1, true);
      }
      if (s > 1800) frame_.sprite(spr::heart_s, 22, 6 - (int)((s - 1800) / 200 % 5));
      break;
    }
    case FRIEND_MEJIRO:  // sings from the blossom; the bird listens
      drawBird(alt && s > 1500 ? Pose::Happy : Pose::Up, bx, LCD_H, false);
      drawFlyer(friend_, landX, landB, true, 0);
      for (int i = 0; i < 2; i++) frame_.sprite(spr::note, landX - 4 - i * 3, 5 - (int)((s / 120 + i * 4) % 8));
      break;
    case FRIEND_OWL: {  // sits quietly and blinks, slowly; the bird blinks with it. Luck sparkles around.
      bool shut = s % 1200 < 250;
      drawBird(shut ? Pose::Blink : Pose::Idle, bx, LCD_H, false);
      drawFlyer(friend_, landX, landB, true, 0, shut ? &spr::fr_owl_blink : nullptr);
      frame_.sprite(spr::sparkle, alt ? landX - 6 : LCD_W - 6, alt ? 3 : 1);
      break;
    }
    default:  // the pigeon, if it ever stays: a bow and a coo
      drawBird(alt ? Pose::Happy : Pose::Idle, bx, LCD_H, false);
      drawFlyer(friend_, landX, landB - (alt ? 1 : 0), true, 0);
      break;
  }
}


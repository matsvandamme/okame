#include "games.h"

void Duet::start(uint32_t seed, uint32_t now) {
  rng_ = Rng(seed);
  round_ = wins_ = 0;
  beginRound(now);
}

void Duet::beginRound(uint32_t now) {
  for (int i = 0; i < length(); i++) pattern_[i] = rng_.next() & 1;
  entered_ = 0;
  showIdx_ = -1;
  phase_ = Phase::Showing;
  t0_ = now;
}

void Duet::update(uint32_t now) {
  uint32_t el = now - t0_;
  switch (phase_) {
    case Phase::Showing: {
      uint32_t t = 600;  // lead-in
      showIdx_ = -1;
      for (int i = 0; i < length(); i++) {
        uint32_t d = pattern_[i] ? LONG_MS : SHORT_MS;
        if (el >= t && el < t + d) showIdx_ = i;
        t += d + GAP_MS;
      }
      if (el >= t) {
        phase_ = Phase::Listening;
        t0_ = now;
      }
      break;
    }
    case Phase::Listening:
      if (el >= TIMEOUT_MS) {
        phase_ = Phase::RoundLost;
        t0_ = now;
      }
      break;
    case Phase::RoundWon:
    case Phase::RoundLost:
      if (el >= RESULT_MS) {
        if (++round_ >= ROUNDS)
          phase_ = Phase::Done;
        else
          beginRound(now);
      }
      break;
    case Phase::Done:
      break;
  }
}

void Duet::input(Press p, uint32_t now) {
  if (phase_ != Phase::Listening) return;
  bool isLong = p == Press::Long;
  input_[entered_] = isLong;
  if (isLong != pattern_[entered_]) {
    entered_++;
    phase_ = Phase::RoundLost;
    t0_ = now;
    return;
  }
  t0_ = now;  // each correct note resets the timeout
  if (++entered_ == length()) {
    wins_++;
    phase_ = Phase::RoundWon;
  }
}

void Peekaboo::start(uint32_t seed, uint32_t now) {
  rng_ = Rng(seed);
  round_ = hits_ = 0;
  beginRound(now);
}

void Peekaboo::beginRound(uint32_t now) {
  phase_ = Phase::Hidden;
  timedOut_ = false;
  hideMs_ = rng_.range(1000, 3000);
  right_ = rng_.next() & 1;
  t0_ = now;
}

void Peekaboo::update(uint32_t now) {
  uint32_t el = now - t0_;
  uint32_t peekMs = 900 - round_ * 100;
  switch (phase_) {
    case Phase::Hidden:
      if (el >= hideMs_) {
        phase_ = Phase::Peeking;
        t0_ = now;
      }
      break;
    case Phase::Peeking:
      if (el >= peekMs) {
        phase_ = Phase::Miss;
        timedOut_ = true;
        t0_ = now;
      }
      break;
    case Phase::Hit:
    case Phase::Miss:
      if (el >= RESULT_MS) {
        if (++round_ >= ROUNDS)
          phase_ = Phase::Done;
        else
          beginRound(now);
      }
      break;
    case Phase::Done:
      break;
  }
}

void Peekaboo::input(uint32_t now) {
  if (phase_ == Phase::Miss && timedOut_ && now - t0_ < GRACE_MS) {
    hits_++;  // just after the window closed: count it
    phase_ = Phase::Hit;
    timedOut_ = false;
  } else if (phase_ == Phase::Peeking) {
    hits_++;
    phase_ = Phase::Hit;
  } else if (phase_ == Phase::Hidden) {
    phase_ = Phase::Miss;
  } else {
    return;
  }
  t0_ = now;
}

void SeedRain::start(uint32_t seed, uint32_t now, int birdW) {
  rng_ = Rng(seed);
  for (auto& s : seeds_) s.live = false;
  birdW_ = birdW;
  birdX_ = (40 - birdW) / 2;
  score_ = 0;
  lives_ = LIVES;
  t0_ = last_ = now;
  nextSpawn_ = now + 600;
  done_ = false;
}

void SeedRain::update(uint32_t now) {
  if (done_) return;
  uint32_t el = now - t0_, dt = now - last_;
  last_ = now;
  // Falls faster as the game goes on: from 9 to ~20 dots per second.
  int speed16 = 16 * (9 + (int)(el / 4000)) * (int)dt / 1000;
  if (speed16 < 1) speed16 = 1;
  if ((int32_t)(now - nextSpawn_) >= 0) {
    for (auto& s : seeds_)
      if (!s.live) {
        s = {(int)rng_.range(1, 37), -2 * 16, rng_.range(0, 99) < 15, true};
        break;
      }
    nextSpawn_ = now + 1400 - (el / 60 < 700 ? el / 60 : 700);
  }
  for (auto& s : seeds_) {
    if (!s.live) continue;
    s.y16 += speed16;
    int y = s.y16 / 16;
    // The beak is at the top of the bird: catch when the seed reaches the bird's height over its body.
    if (y >= FLOOR - 9 && y <= FLOOR - 7 && s.x >= birdX_ - 1 && s.x <= birdX_ + birdW_) {
      score_ += s.golden ? 3 : 1;
      s.live = false;
    } else if (y > FLOOR) {
      s.live = false;
      if (!s.golden && --lives_ <= 0) done_ = true;
    }
  }
  if (el >= MAX_MS) done_ = true;
}

void SeedRain::input(Press p) {
  if (done_) return;
  if (p == Press::Left) birdX_ -= 3;
  if (p == Press::Right) birdX_ += 3;
  if (birdX_ < 0) birdX_ = 0;
  if (birdX_ > 40 - birdW_) birdX_ = 40 - birdW_;
}

int Janken::outcome(Hand you, Hand bird) {
  if (you == bird) return 0;
  return (you == Rock && bird == Scissors) || (you == Scissors && bird == Paper) || (you == Paper && bird == Rock)
             ? 1 : -1;
}

void Janken::start(uint32_t seed, uint32_t now) {
  rng_ = Rng(seed);
  wins_ = losses_ = throws_ = last_ = 0;
  timedOut_ = false;
  phase_ = Phase::Chant;
  t0_ = now;
}

void Janken::update(uint32_t now) {
  uint32_t el = now - t0_;
  if (phase_ == Phase::Chant && el >= TIMEOUT_MS) {
    phase_ = Phase::Done;  // nobody threw: the bird wanders off
    timedOut_ = true;
  } else if (phase_ == Phase::Reveal && el >= REVEAL_MS) {
    bool over = wins_ >= TO_WIN || losses_ >= TO_WIN || throws_ >= MAX_THROWS;
    phase_ = over ? Phase::Done : Phase::Chant;
    t0_ = now;
  }
}

void Janken::input(Hand h, uint32_t now) {
  if (phase_ != Phase::Chant) return;
  you_ = h;
  bird_ = Hand(rng_.range(0, 2));
  last_ = outcome(you_, bird_);
  if (last_ > 0) wins_++;
  if (last_ < 0) losses_++;
  throws_++;
  phase_ = Phase::Reveal;
  t0_ = now;
}

void Dance::start(uint32_t seed, uint32_t now) {
  rng_ = Rng(seed);
  for (auto& m : seq_) m = Move(rng_.range(0, 3));
  len_ = START;
  best_ = 0;
  echo_ = -1;
  beginRound(now);
}

void Dance::beginRound(uint32_t now) {
  entered_ = 0;
  showIdx_ = -1;
  phase_ = Phase::Showing;
  t0_ = now;
}

void Dance::update(uint32_t now) {
  uint32_t el = now - t0_;
  switch (phase_) {
    case Phase::Showing: {
      uint32_t t = 700;  // lead-in
      showIdx_ = -1;
      for (int i = 0; i < len_; i++) {
        if (el >= t && el < t + MOVE_MS) showIdx_ = i;
        t += MOVE_MS + GAP_MS;
      }
      if (el >= t) {
        phase_ = Phase::Listening;
        t0_ = now;
      }
      break;
    }
    case Phase::Listening:
      if (el >= TIMEOUT_MS) {
        phase_ = Phase::Fail;
        t0_ = now;
      }
      break;
    case Phase::Good:
      if (el >= RESULT_MS) {
        if (len_ >= MAX) {
          phase_ = Phase::Done;
        } else {
          len_++;
          beginRound(now);
        }
      }
      break;
    case Phase::Fail:
      if (el >= RESULT_MS) phase_ = Phase::Done;
      break;
    case Phase::Done:
      break;
  }
}

void Dance::input(Move m, uint32_t now) {
  if (phase_ != Phase::Listening) return;
  echo_ = m;
  echoAt_ = now;
  t0_ = now;  // each move resets the timeout
  if (m != seq_[entered_]) {
    phase_ = Phase::Fail;
    return;
  }
  if (++entered_ == len_) {
    best_ = len_;
    phase_ = Phase::Good;
  }
}

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

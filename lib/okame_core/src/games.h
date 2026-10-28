#pragma once
#include <stdint.h>
#include "rng.h"

enum class Press : uint8_t { Short, Long };

// Whistle duet: the bird whistles short/long notes, the player repeats them.
class Duet {
 public:
  enum class Phase : uint8_t { Showing, Listening, RoundWon, RoundLost, Done };
  static constexpr int ROUNDS = 3;
  static constexpr uint32_t SHORT_MS = 450, LONG_MS = 900, GAP_MS = 300;
  static constexpr uint32_t RESULT_MS = 1200, TIMEOUT_MS = 6000;

  void start(uint32_t seed, uint32_t now);
  void update(uint32_t now);
  void input(Press p, uint32_t now);

  Phase phase() const { return phase_; }
  int round() const { return round_; }
  int length() const { return 3 + round_; }
  bool note(int i) const { return pattern_[i]; }  // true = long
  int showing() const { return showIdx_; }       // note being whistled, -1 in a gap
  int entered() const { return entered_; }
  bool enteredNote(int i) const { return input_[i]; }
  int wins() const { return wins_; }
  bool won() const { return wins_ >= 2; }

 private:
  void beginRound(uint32_t now);
  Rng rng_;
  Phase phase_ = Phase::Done;
  int round_ = 0, wins_ = 0, entered_ = 0, showIdx_ = -1;
  bool pattern_[6] = {}, input_[6] = {};
  uint32_t t0_ = 0;
};

// Peekaboo: the bird hides behind a branch; press while it peeks out.
class Peekaboo {
 public:
  enum class Phase : uint8_t { Hidden, Peeking, Hit, Miss, Done };
  static constexpr int ROUNDS = 5;
  static constexpr uint32_t RESULT_MS = 900;

  void start(uint32_t seed, uint32_t now);
  void update(uint32_t now);
  void input(uint32_t now);

  Phase phase() const { return phase_; }
  int round() const { return round_; }
  int hits() const { return hits_; }
  bool peekRight() const { return right_; }
  bool won() const { return hits_ >= 3; }

 private:
  void beginRound(uint32_t now);
  Rng rng_;
  Phase phase_ = Phase::Done;
  int round_ = 0, hits_ = 0;
  bool right_ = false;
  uint32_t t0_ = 0, hideMs_ = 0;
};

#pragma once
#include <stdint.h>
#include "rng.h"

// Up/Down are the dance's hop and bow; One/Two/Three are janken's rock, scissors and paper.
enum class Press : uint8_t { Short, Long, Left, Right, Up, Down, One, Two, Three };

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

// Seed Rain: seeds fall, steer the bird left/right to catch them. Golden millet is worth 3.
class SeedRain {
 public:
  static constexpr int MAX_SEEDS = 4, LIVES = 3, FLOOR = 19;
  static constexpr uint32_t MAX_MS = 45000;
  struct Seed { int x; int y16; bool golden, live; };

  void start(uint32_t seed, uint32_t now, int birdW);
  void update(uint32_t now);
  void input(Press p);
  bool done() const { return done_; }
  int score() const { return score_; }
  int lives() const { return lives_; }
  int birdX() const { return birdX_; }
  const Seed& seed(int i) const { return seeds_[i]; }

 private:
  Rng rng_;
  Seed seeds_[MAX_SEEDS] = {};
  int birdX_ = 0, birdW_ = 10, score_ = 0, lives_ = LIVES;
  uint32_t t0_ = 0, last_ = 0, nextSpawn_ = 0;
  bool done_ = true;
};

// Peekaboo: the bird hides behind a branch; press while it peeks out.
class Peekaboo {
 public:
  enum class Phase : uint8_t { Hidden, Peeking, Hit, Miss, Done };
  static constexpr int ROUNDS = 5;
  static constexpr uint32_t RESULT_MS = 900;
  static constexpr uint32_t GRACE_MS = 250;  // taps from the phone arrive a little late over Wi-Fi

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
  bool right_ = false, timedOut_ = false;
  uint32_t t0_ = 0, hideMs_ = 0;
};

// Janken (rock-paper-scissors): first to two wins, a draw ("aiko") is thrown again.
class Janken {
 public:
  enum class Phase : uint8_t { Chant, Reveal, Done };
  enum Hand : uint8_t { Rock, Scissors, Paper };
  static constexpr int TO_WIN = 2, MAX_THROWS = 7;
  static constexpr uint32_t REVEAL_MS = 1800, TIMEOUT_MS = 12000;

  void start(uint32_t seed, uint32_t now);
  void update(uint32_t now);
  void input(Hand h, uint32_t now);
  static int outcome(Hand you, Hand bird);  // 1 you win, 0 draw, -1 bird wins

  Phase phase() const { return phase_; }
  int wins() const { return wins_; }
  int losses() const { return losses_; }
  Hand you() const { return you_; }
  Hand bird() const { return bird_; }
  int last() const { return last_; }  // outcome of the throw on show
  bool won() const { return wins_ > losses_; }
  bool timedOut() const { return timedOut_; }  // nobody threw: end quietly, no win or loss
  uint32_t phaseMs(uint32_t now) const { return now - t0_; }

 private:
  Rng rng_;
  Phase phase_ = Phase::Done;
  int wins_ = 0, losses_ = 0, throws_ = 0, last_ = 0;
  bool timedOut_ = false;
  Hand you_ = Rock, bird_ = Rock;
  uint32_t t0_ = 0;
};

// Copy the dance: the bird shows a sequence of moves, the player repeats it; one move longer each round.
class Dance {
 public:
  enum class Phase : uint8_t { Showing, Listening, Good, Fail, Done };
  enum Move : uint8_t { Left, Right, Hop, Bow };
  static constexpr int START = 3, MAX = 12;
  static constexpr uint32_t MOVE_MS = 650, GAP_MS = 250, RESULT_MS = 1100, TIMEOUT_MS = 7000;

  void start(uint32_t seed, uint32_t now);
  void update(uint32_t now);
  void input(Move m, uint32_t now);

  Phase phase() const { return phase_; }
  int length() const { return len_; }
  Move move(int i) const { return seq_[i]; }
  int showing() const { return showIdx_; }  // move being danced, -1 in a gap
  int entered() const { return entered_; }
  int score() const { return best_; }       // longest sequence copied
  int echo(uint32_t now) const { return now - echoAt_ < 450 ? echo_ : -1; }  // the player's last move, mirrored

 private:
  void beginRound(uint32_t now);
  Rng rng_;
  Phase phase_ = Phase::Done;
  Move seq_[MAX] = {};
  int len_ = START, entered_ = 0, showIdx_ = -1, best_ = 0, echo_ = -1;
  uint32_t t0_ = 0, echoAt_ = 0;
};

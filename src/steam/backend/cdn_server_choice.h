#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace playden::steam {

// Picks the content server for each chunk by measured speed, as the Steam client does. The first
// kCandidates servers of the directory's list (already sorted by load) are each tried kProbes times;
// after that most chunks go to the fastest by a running average, and every kRecheckEvery-th pick
// goes to another candidate so a server that speeds up or slows down is noticed. A failure counts as
// a very slow sample, and the retry after it avoids the server that failed. Safe from any thread.
class CdnServerChoice {
 public:
  static constexpr std::size_t kCandidates = 4;
  static constexpr std::uint32_t kProbes = 3;
  static constexpr std::uint32_t kRecheckEvery = 16;

  explicit CdnServerChoice(std::size_t servers);

  // The server index for the next request; `avoid` (when below the server count) is skipped if
  // another candidate exists.
  std::size_t Pick(std::size_t avoid = static_cast<std::size_t>(-1));
  void Succeeded(std::size_t server, std::uint64_t bytes, std::uint64_t microseconds);
  void Failed(std::size_t server);
  // Bytes per second by running average; 0 before the first sample.
  double Rate(std::size_t server) const;

 private:
  struct Candidate {
    double rate = 0;
    std::uint32_t samples = 0;
  };
  mutable std::mutex mutex_;
  std::vector<Candidate> candidates_;
  std::uint64_t picks_ = 0;
  std::size_t recheck_ = 0;
  void Sample(std::size_t server, double rate);
};

}  // namespace playden::steam

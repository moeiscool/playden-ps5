#include "steam/backend/cdn_server_choice.h"

#include <cassert>
#include <cstddef>
#include <vector>

using namespace playden::steam;

namespace {

// Feeds `picks` requests where server i moves `speeds[i]` bytes per microsecond (0 = fails) and
// returns how many each server got.
std::vector<int> Run(CdnServerChoice& choice, const std::vector<int>& speeds, int picks) {
  std::vector<int> counts(speeds.size());
  for (int pick = 0; pick < picks; ++pick) {
    const auto server = choice.Pick();
    ++counts[server];
    if (speeds[server] == 0) choice.Failed(server);
    else choice.Succeeded(server, 1000ull * static_cast<unsigned>(speeds[server]), 1000);
  }
  return counts;
}

void ProbesEachCandidateFirst() {
  CdnServerChoice choice(6);
  std::vector<int> counts(6);
  for (std::uint32_t pick = 0; pick < CdnServerChoice::kCandidates * CdnServerChoice::kProbes; ++pick) {
    const auto server = choice.Pick();
    ++counts[server];
    choice.Succeeded(server, 1000, 1000);
  }
  for (std::size_t index = 0; index < CdnServerChoice::kCandidates; ++index)
    assert(counts[index] == static_cast<int>(CdnServerChoice::kProbes));
  assert(counts[4] == 0 && counts[5] == 0);  // only the least loaded few are candidates
}

void MostChunksGoToTheFastest() {
  CdnServerChoice choice(4);
  const auto counts = Run(choice, {2, 9, 1, 3}, 400);
  assert(counts[1] > 300);
  // The others are still checked now and then.
  assert(counts[0] > static_cast<int>(CdnServerChoice::kProbes) && counts[2] > 0 && counts[3] > 0);
  assert(choice.Rate(1) > choice.Rate(3) && choice.Rate(3) > choice.Rate(0));
}

void MovesAwayFromAServerThatSlowsDown() {
  CdnServerChoice choice(3);
  Run(choice, {9, 3, 1}, 100);
  const auto counts = Run(choice, {1, 3, 1}, 200);
  assert(counts[1] > counts[0]);
}

void FailingServerIsAvoided() {
  CdnServerChoice choice(3);
  const auto counts = Run(choice, {5, 0, 4}, 200);
  assert(counts[1] < 30);
  // The retry after a failure goes elsewhere.
  CdnServerChoice retry(2);
  assert(retry.Pick(0) == 1 && retry.Pick(1) == 0);
}

void OneOrNoServer() {
  CdnServerChoice one(1);
  assert(one.Pick() == 0 && one.Pick(0) == 0);
  CdnServerChoice none(0);
  assert(none.Pick() == 0);
  none.Succeeded(3, 10, 10);
  assert(none.Rate(3) == 0);
}

}  // namespace

int main() {
  ProbesEachCandidateFirst();
  MostChunksGoToTheFastest();
  MovesAwayFromAServerThatSlowsDown();
  FailingServerIsAvoided();
  OneOrNoServer();
}

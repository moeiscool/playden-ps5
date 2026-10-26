#include "steam/backend/cdn_server_choice.h"

#include <algorithm>

namespace playden::steam {
namespace {

// Weight of a new sample in the running average: recent chunks matter most, one outlier little.
constexpr double kNewWeight = 0.25;

}  // namespace

CdnServerChoice::CdnServerChoice(std::size_t servers)
    : candidates_(std::min(servers, kCandidates)) {}

std::size_t CdnServerChoice::Pick(std::size_t avoid) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto count = candidates_.size();
  if (count == 0) return 0;
  if (count == 1) return 0;
  const auto allowed = [&](std::size_t index) { return index != avoid; };
  ++picks_;
  // Probe first: the least sampled candidate until each has kProbes samples.
  std::size_t least = count;
  for (std::size_t index = 0; index < count; ++index) {
    if (!allowed(index) || candidates_[index].samples >= kProbes) continue;
    if (least == count || candidates_[index].samples < candidates_[least].samples) least = index;
  }
  if (least != count) return least;
  std::size_t best = count;
  for (std::size_t index = 0; index < count; ++index) {
    if (allowed(index) && (best == count || candidates_[index].rate > candidates_[best].rate)) best = index;
  }
  if (picks_ % kRecheckEvery == 0) {
    for (std::size_t step = 0; step < count; ++step) {
      recheck_ = (recheck_ + 1) % count;
      if (recheck_ != best && allowed(recheck_)) return recheck_;
    }
  }
  return best;
}

void CdnServerChoice::Sample(std::size_t server, double rate) {
  if (server >= candidates_.size()) return;
  auto& candidate = candidates_[server];
  candidate.rate = candidate.samples == 0 ? rate : candidate.rate + kNewWeight * (rate - candidate.rate);
  ++candidate.samples;
}

void CdnServerChoice::Succeeded(std::size_t server, std::uint64_t bytes, std::uint64_t microseconds) {
  std::lock_guard<std::mutex> lock(mutex_);
  Sample(server, static_cast<double>(bytes) * 1e6 / static_cast<double>(std::max<std::uint64_t>(microseconds, 1)));
}

void CdnServerChoice::Failed(std::size_t server) {
  std::lock_guard<std::mutex> lock(mutex_);
  Sample(server, 0);
}

double CdnServerChoice::Rate(std::size_t server) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return server < candidates_.size() ? candidates_[server].rate : 0;
}

}  // namespace playden::steam

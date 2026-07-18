#include "nyxbyte/brain.hpp"

#include <algorithm>
#include <chrono>

namespace nyxbyte {

AmbientBrain::AmbientBrain()
    : random_state_(
          static_cast<std::uint64_t>(
              std::chrono::high_resolution_clock::now().time_since_epoch().count())
          | 1ULL) {
}

void AmbientBrain::reset(const std::chrono::milliseconds now) {
    next_decision_ = now + range(1800, 4200);
}

std::uint64_t AmbientBrain::next_random() {
    // xorshift64*: tiny, deterministic, and more than adequate for ambient motion.
    random_state_ ^= random_state_ >> 12U;
    random_state_ ^= random_state_ << 25U;
    random_state_ ^= random_state_ >> 27U;
    return random_state_ * 0x2545F4914F6CDD1DULL;
}

std::chrono::milliseconds AmbientBrain::range(
    const std::int64_t low,
    const std::int64_t high) {
    const auto span = static_cast<std::uint64_t>(std::max<std::int64_t>(1, high - low + 1));
    return std::chrono::milliseconds{low + static_cast<std::int64_t>(next_random() % span)};
}

BrainDecision AmbientBrain::think(const BrainContext& context) {
    if (context.animation_busy || context.now < next_decision_) {
        return {};
    }

    const auto roll = next_random() % 100ULL;
    BrainDecision result{};

    if (context.roaming_enabled && roll < 62ULL) {
        const bool choose_right = (next_random() & 1ULL) != 0ULL;
        if ((choose_right && !context.at_right_edge) || context.at_left_edge) {
            result.action = Action::roam_right;
        } else {
            result.action = Action::roam_left;
        }
        result.duration = range(1600, 4200);
    } else if (roll < 78ULL) {
        result.action = Action::wave;
    } else if (roll < 88ULL) {
        result.action = Action::jump;
    } else if (roll < 95ULL) {
        result.action = Action::focus;
        result.duration = range(1800, 3200);
    } else {
        result.action = Action::review;
    }

    next_decision_ = context.now + result.duration + range(2600, 6400);
    return result;
}

} // namespace nyxbyte

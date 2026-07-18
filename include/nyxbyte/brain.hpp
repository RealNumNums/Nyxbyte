#pragma once

#include <chrono>
#include <cstdint>

namespace nyxbyte {

enum class Action {
    none,
    idle,
    roam_left,
    roam_right,
    wave,
    jump,
    focus,
    review,
};

struct BrainContext {
    std::chrono::milliseconds now{};
    bool roaming_enabled{true};
    bool animation_busy{false};
    bool at_left_edge{false};
    bool at_right_edge{false};
};

struct BrainDecision {
    Action action{Action::none};
    std::chrono::milliseconds duration{};
};

class ICompanionBrain {
public:
    virtual ~ICompanionBrain() = default;
    virtual BrainDecision think(const BrainContext& context) = 0;
    virtual void reset(std::chrono::milliseconds now) = 0;
};

class AmbientBrain final : public ICompanionBrain {
public:
    AmbientBrain();

    BrainDecision think(const BrainContext& context) override;
    void reset(std::chrono::milliseconds now) override;

private:
    std::uint64_t next_random();
    std::chrono::milliseconds range(std::int64_t low, std::int64_t high);

    std::uint64_t random_state_{};
    std::chrono::milliseconds next_decision_{};
};

} // namespace nyxbyte

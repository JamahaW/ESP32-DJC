#pragma once

#include "kfgui/core/Behavior.hpp"


namespace kfgui {

struct BehaviorManager final {

private:

    Behavior *current_behavior{nullptr};

public:

    static BehaviorManager &instance() noexcept {
        static BehaviorManager instance{};
        return instance;
    }

    [[nodiscard]] inline bool isActive(const Behavior &behavior) const noexcept {
        return &behavior == current_behavior;
    }

    void bindBehavior(Behavior &behavior) noexcept {
        current_behavior = &behavior;
        behavior.onBind();
    }

    void render(kf::Painter &painter) noexcept {
        if (current_behavior != nullptr) {
            painter.fill(false);
            current_behavior->render();
        }
    }

    void loop() noexcept {
        if (current_behavior != nullptr) {
            current_behavior->loop();
        }
    }

private:
    BehaviorManager() = default;

public:
    BehaviorManager(const BehaviorManager &) = delete;
};


}
#pragma once

#include "kf/Painter.hpp"


namespace kfgui {

struct Widget {
    kf::Painter painter{};

    virtual void render() noexcept = 0;
};

}
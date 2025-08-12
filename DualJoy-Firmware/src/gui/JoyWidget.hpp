#pragma once

#include "KiraFlux-GUI.hpp"


struct JoyWidget final : kfgui::Widget {

    const float *x{nullptr};
    const float *y{nullptr};

    void doRender(kf::Painter &p) const noexcept override {
        constexpr auto text_offset = static_cast<kf::Position>(2);

        const auto max_x = static_cast<kf::Position>(p.frame.width - 1);
        const auto max_y = static_cast<kf::Position>(p.frame.height - 1);

        p.rect(0, 0, max_x, max_y, kf::Painter::Mode::FillBorder);

        const auto center_x = static_cast<kf::Position>(max_x / 2);
        const auto center_y = static_cast<kf::Position>(max_y / 2);

        if (x != nullptr) {
            const auto line_x = static_cast<kf::Position>(*x * static_cast<float>(center_x));
            p.line(center_x, center_y, static_cast<kf::Position>(center_x + line_x), center_y); // x

            const auto text = rs::formatted<12>("%+1.2f   X", *x);
            p.text(0, text_offset, text.data());
        }

        if (y != nullptr) {
            const auto line_y = static_cast<kf::Position>(*y * static_cast<float>(center_x));
            p.line(center_x, center_y, center_x, static_cast<kf::Position>(center_y - line_y)); // y

            const auto text = rs::formatted<12>("%+1.2f   Y", *y);
            p.text(0, static_cast<kf::Position>(center_y + text_offset), text.data());
        }
    }
};

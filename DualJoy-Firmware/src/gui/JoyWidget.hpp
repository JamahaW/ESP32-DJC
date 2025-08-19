#pragma once

#include "KiraFlux-GUI.hpp"


struct JoyWidget final : kfgui::Widget {

private:

    const float *x{nullptr};
    const float *y{nullptr};

public:

    void bindAxis(const float &axis_x, const float &axis_y) noexcept {
        x = &axis_x;
        y = &axis_y;
    }

    void render() noexcept override {
        constexpr auto text_offset = static_cast<kf::Position>(2);
        constexpr auto format = "%+1.2f";

        painter.rect(0, 0, painter.maxX(), painter.maxY(), kf::Painter::Mode::FillBorder);

        const auto center_x = painter.centerX();
        const auto center_y = painter.centerY();

        const auto right_text_x = painter.maxGlyphX() - text_offset;

        if (x != nullptr) {
            painter.line(
                center_x,
                center_y,
                static_cast<kf::Position>(center_x + *x * static_cast<float>(center_x)),
                center_y
            );

            painter.setCursor(text_offset, text_offset);
            painter.text(rs::formatted<8>(format, *x).data());
            painter.setCursor(right_text_x, text_offset);
            painter.text("X");
        }

        if (y != nullptr) {
            painter.line(
                center_x,
                center_y,
                center_x,
                static_cast<kf::Position>(center_y - *y * static_cast<float>(center_y))
            );

            const auto text_offset_y = static_cast<kf::Position>(center_y + text_offset);

            painter.setCursor(text_offset, text_offset_y);
            painter.text(rs::formatted<8>(format, *y).data());
            painter.setCursor(right_text_x, text_offset_y);
            painter.text("Y");
        }
    }
};

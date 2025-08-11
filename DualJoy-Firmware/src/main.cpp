#include <WiFi.h>
#include "Arduino.h"

#include "kf/GFX.hpp"
#include "kf/SSD1306.h"
#include "kf/Joystick.hpp"

#include "espnow/Protocol.hpp"


static constexpr espnow::Mac target = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

struct Window {
    virtual void render(kf::Painter &p) const noexcept = 0;
};

struct GUI {

private:

    Window *current_window{nullptr};

public:

    static GUI &instance() noexcept {
        static GUI instance;

        return instance;
    }

    void bindWindow(Window &window) noexcept {
        current_window = &window;
    }

    void render(kf::Painter &painter) noexcept {
        if (current_window == nullptr) { return; }

        painter.fill(false);

        current_window->render(painter);
    }

private:

    GUI() = default;

public:

    GUI(const GUI &) = delete;

};

struct InitWindow : Window {

    static InitWindow &instance() noexcept {
        static InitWindow instance;
        return instance;
    }

    void render(kf::Painter &p) const noexcept override {
        p.text(0, 0, "Init");
    }

private:

    InitWindow() = default;
};

struct JoyWidget final {

    const float *x{nullptr};
    const float *y{nullptr};

    void render(kf::Painter &p) const noexcept {
        const auto r = static_cast<kf::Position>(std::min(p.frame.width - 1, p.frame.height - 1) / 2);
        p.circle(r, r, r, kf::Painter::Mode::FillBorder);

        if (x == nullptr or y == nullptr) { return; }

        const auto line_x = static_cast<kf::Position>(static_cast<float>(r) + *x * static_cast<float>(r));
        const auto line_y = static_cast<kf::Position>(static_cast<float>(r) - *y * static_cast<float>(r));

        p.line(r, r, line_x, line_y);
    }
};

struct AxisWidget final {

    static constexpr kf::Position width = 8;

    const float *value{nullptr};

    void render(kf::Painter &p) const noexcept {
        const auto x_end = static_cast<kf::Position>(p.frame.width - 1);
        const auto y_end = static_cast<kf::Position>(p.frame.height - 1);
        const auto x_start = static_cast<kf::Position>(x_end - width);

        if (value == nullptr) {
            p.rect(x_start, 0, x_end, y_end, kf::Painter::Mode::FillBorder);
            return;
        }

        const auto y_center = static_cast<kf::Position>(y_end / 2);
        const auto height = static_cast<kf::Position>(static_cast<float>(y_center) - *value * static_cast<float>(y_center));

        p.rect(x_start, y_center, x_end, height, kf::Painter::Mode::Fill);
    }
};

struct JoyControlWindow : Window {

    JoyWidget left_joy_widget, right_joy_widget;

    static JoyControlWindow &instance() noexcept {
        static JoyControlWindow instance;
        return instance;
    }

    void render(kf::Painter &p) const noexcept override {
        left_joy_widget.render(p);
        right_joy_widget.render(p);
    }

private:

    JoyControlWindow() = default;
};

void initEspNow() {
    {
        WiFiClass::mode(WIFI_MODE_STA);

        const auto result = espnow::Protocol::init();

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
            return;
        }
    }

    {
        const auto result = espnow::Peer::add(target);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
            return;
        }
    }

    {
        auto on_receive = [](const espnow::Mac &mac, const void *data, rs::u8 size) {
            Serial.printf("[%s] : ", rs::toArrayString(mac).data());
            Serial.print(static_cast<const char *>(data));
        };

        const auto result = espnow::Protocol::instance().setReceiveHandler(on_receive);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
            return;
        }
    }
}

[[noreturn]] void inputTask(void *) {
    static auto left_joystick = kf::Joystick(32, 33, 0.8);
    static auto right_joystick = kf::Joystick(35, 34, 0.8);

    struct JoyPacket {
        float left_x, left_y;
        float right_x, right_y;
    } packet{};

    left_joystick.init();
    right_joystick.init();

    left_joystick.axis_x.inverted = true;
    left_joystick.axis_y.inverted = false;

    right_joystick.axis_y.inverted = true;
    right_joystick.axis_x.inverted = false;

    left_joystick.calibrate(100);
    right_joystick.calibrate(100);

    auto &joy = JoyControlWindow::instance();
    joy.left_joy_widget.x = &packet.left_x;
    joy.left_joy_widget.y = &packet.left_y;
    joy.right_joy_widget.x = &packet.right_x;
    joy.right_joy_widget.y = &packet.right_y;

    GUI::instance().bindWindow(joy);

    while (true) {
        packet.left_x = left_joystick.axis_x.read();
        packet.left_y = left_joystick.axis_y.read();

        packet.right_x = right_joystick.axis_x.read();
        packet.right_y = right_joystick.axis_y.read();

        const auto result = espnow::Protocol::send(target, packet);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
        }

        delay(10);
    }
}

[[noreturn]] void displayTask(void *) {
    constexpr auto target_fps = 20;
    constexpr auto ms_per_frame = 1000 / target_fps;

    kf::SSD1306 display_driver{};

    kf::Painter main_gfx{
        kf::FrameView::create(
            display_driver.buffer,
            kf::SSD1306::width,
            kf::SSD1306::width,
            kf::SSD1306::height, 0, 0
        ).value
    };

    main_gfx.setFont(kf::fonts::gyver_5x7_en);

    Wire.setClock(1000000UL);
    display_driver.init();

    auto &gui = GUI::instance();

    while (true) {
        delay(ms_per_frame);
        gui.render(main_gfx);
        display_driver.update();
    }
}

void setup() {
    GUI::instance().bindWindow(InitWindow::instance());

    Serial.begin(115200);

    initEspNow();

    xTaskCreate(displayTask, "display", 4096, nullptr, 1, nullptr);
    xTaskCreate(inputTask, "input", 4096, nullptr, 1, nullptr);
}

void loop() {}
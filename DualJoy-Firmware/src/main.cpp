#include <WiFi.h>
#include "Arduino.h"

#include "kf/GFX.hpp"
#include "kf/SSD1306.h"
#include "kf/Joystick.hpp"

#include "espnow/Protocol.hpp"


static constexpr espnow::Mac target = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

struct [[gnu::packed]] Packet {
    float x, y, a;
} packet;

enum class State {
    Setup,
    Running
};

State current_state = State::Setup;

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
    static auto left_joystick = kf::Joystick(32, 33, 0.9);
    static auto right_axis = kf::AnalogAxis(34, 0.6);

    left_joystick.init();
    right_axis.init();

    left_joystick.axis_x.inverted = true;
    left_joystick.axis_y.inverted = false;
    right_axis.inverted = true;

    left_joystick.calibrate(100);
    {
        int s = 0;

        for (int i = 0; i < 100; i++) {
            s += right_axis.readRaw();
            delay(1);
        }
        s /= 100;
        right_axis.updateCenter(s);
    }

    current_state = State::Running;

    while (true) {
        const auto left_data = left_joystick.read();
        packet.a = right_axis.read();

        packet.x = left_data.x;
        packet.y = left_data.y;

        const auto result = espnow::Protocol::send(target, packet);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
        }

        delay(10);
    }
}

static void drawJoyState(kf::Painter &p) {

    const auto h = p.frame.height - 1;
    const auto w = p.frame.width - 1;

    const auto r = static_cast<kf::Position>(std::min(h, w) / 2);

    const auto cx = static_cast<kf::Position>(w / 2);
    const auto cy = static_cast<kf::Position>(h / 2);

    p.circle(cx, cy, r, kf::Painter::Mode::FillBorder);

    const auto x = static_cast<kf::Position>(0.5f * packet.x * p.frame.width + cx);
    const auto y = static_cast<kf::Position>(-0.5f * packet.y * p.frame.height + cy);

    p.line(cx, cy, x, y);
}

static void drawAxisState(kf::Painter &p) {
    const auto w = 12;

    const auto h = p.frame.height - 1;
    const auto cy = static_cast<kf::Position>(h / 2);

    const auto ww = static_cast<kf::Position>(p.frame.width - 1);

    p.rect(
        static_cast<kf::Position>(ww - w),
        cy,
        ww,
        cy + packet.a * -0.5 * h,
        kf::Painter::Mode::Fill
    );
}

[[noreturn]] void displayTask(void *) {
    constexpr auto fps = 20;
    constexpr auto ms_per_frame = 1000 / fps;

    static auto display_driver = kf::SSD1306();

    static auto main_gfx = kf::Painter(kf::FrameView::create(
        display_driver.buffer,
        kf::SSD1306::width,
        kf::SSD1306::width,
        kf::SSD1306::height,
        0,
        0
    ).value);

    static auto left_gfx = kf::Painter(
        main_gfx.frame.sub(64, main_gfx.frame.height, 0, 0).value
    );

    static auto right_gfx = kf::Painter(
        main_gfx.frame.sub(
            64,
            64,
            left_gfx.frame.width,
            0
        ).value
    );

    main_gfx.setFont(kf::fonts::gyver_5x7_en);
    left_gfx.setFont(kf::fonts::gyver_5x7_en);
    right_gfx.setFont(kf::fonts::gyver_5x7_en);

    display_driver.init();

    while (true) {
        main_gfx.fill(false);

        switch (current_state) {
            case State::Setup:
                main_gfx.text(0, 0, "Setup...");
                break;

            case State::Running:
                drawJoyState(left_gfx);
                drawAxisState(right_gfx);
                break;
        }

        display_driver.update();

        //

        delay(ms_per_frame);
    }
}

void setup() {
    Serial.begin(115200);

    initEspNow();

    xTaskCreate(displayTask, "display", 4096, nullptr, 1, nullptr);
    xTaskCreate(inputTask, "input", 4096, nullptr, 1, nullptr);
}

void loop() {}
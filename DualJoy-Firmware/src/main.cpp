#include <WiFi.h>
#include "Arduino.h"

#include "KiraFlux-GUI.hpp"

#include "kf/SSD1306.h"
#include "kf/Joystick.hpp"
#include "kf/Button.hpp"

#include "espnow/Protocol.hpp"
#include "gui/JoyWidget.hpp"
#include "gui/FlagDisplay.hpp"


static constexpr espnow::Mac target = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};


struct DualJoyApp : kfgui::WindowsManager {
    kfgui::Window joy_window;
    kfgui::Window init_window;

    JoyWidget left_joy_widget;
    JoyWidget right_joy_widget;
    FlagDisplay left_hold;
    FlagDisplay left_toggle;
    FlagDisplay right_hold;
    FlagDisplay right_toggle;

    static DualJoyApp &instance() {
        static DualJoyApp instance;
        return instance;
    }

private:

    DualJoyApp() = default;

public:

    DualJoyApp(const DualJoyApp &) = delete;
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
    static struct {
        float left_x, left_y;
        float right_x, right_y;
        bool toggle_left, toggle_right;
        bool hold_left, hold_right;
    } packet{};

    auto &app = DualJoyApp::instance();

    kf::Button left_button{GPIO_NUM_15, kf::Button::Mode::PullUp};
    kf::Button right_button{GPIO_NUM_4, kf::Button::Mode::PullUp};

    kf::Joystick left_joystick{32, 33, 0.8};
    kf::Joystick right_joystick{35, 34, 0.8};

    app.init_window.title = "Buttons";
    left_button.init(false);

    left_button.handler = []() {
        packet.toggle_left ^= 1;
    };

    right_button.init(false);

    right_button.handler = []() {
        packet.toggle_right ^= 1;
    };

    app.init_window.title = "Joystick: Left";
    left_joystick.init();
    left_joystick.axis_x.inverted = true;
    left_joystick.axis_y.inverted = false;
    left_joystick.calibrate(100);

    app.init_window.title = "Joystick: Right";
    right_joystick.init();
    right_joystick.axis_y.inverted = true;
    right_joystick.axis_x.inverted = false;
    right_joystick.calibrate(100);

    app.left_joy_widget.x = &packet.left_x;
    app.left_joy_widget.y = &packet.left_y;

    app.right_joy_widget.x = &packet.right_x;
    app.right_joy_widget.y = &packet.right_y;

    app.left_hold.flag = &packet.hold_left;
    app.right_hold.flag = &packet.hold_right;
    app.right_toggle.flag = &packet.toggle_right;
    app.left_toggle.flag = &packet.toggle_left;

    app.bindWindow(app.joy_window);

    while (true) {
        packet.left_x = left_joystick.axis_x.read();
        packet.left_y = left_joystick.axis_y.read();

        packet.right_x = right_joystick.axis_x.read();
        packet.right_y = right_joystick.axis_y.read();

        packet.hold_left = left_button.read();
        packet.hold_right = right_button.read();

        left_button.poll();
        right_button.poll();

        const auto result = espnow::Protocol::send(target, packet);

        if (result.fail()) {
            Serial.println(rs::toString(result.error));
        }

        delay(10);
    }
}

[[noreturn]] void displayTask(void *) {
    auto &app = DualJoyApp::instance();

    constexpr auto target_fps = 25;
    constexpr auto ms_per_frame = 1000 / target_fps;

    kf::SSD1306 display_driver;

    kf::Painter main_gfx{
        kf::FrameView::create(
            display_driver.buffer,
            kf::SSD1306::width,
            kf::SSD1306::width,
            kf::SSD1306::height, 0, 0
        ).value
    };
    main_gfx.setFont(kf::fonts::gyver_5x7_en);

    constexpr auto column_width = 64;
    constexpr auto joy_height = 44;

    const auto column_right_offset = static_cast<kf::Position>(main_gfx.frame.width - column_width - 1);

    kf::Painter left_joy{
        main_gfx.frame.sub(
            column_width,
            joy_height,
            0,
            0
        ).value
    };
    left_joy.setFont(kf::fonts::gyver_5x7_en);
    app.left_joy_widget.bindPainter(left_joy);

    kf::Painter right_joy{
        main_gfx.frame.sub(
            column_width,
            joy_height,
            column_right_offset,
            0
        ).value
    };
    right_joy.setFont(kf::fonts::gyver_5x7_en);
    app.right_joy_widget.bindPainter(right_joy);

    constexpr auto flag_display_height = 8;

    kf::Painter left_hold{
        main_gfx.frame.sub(
            column_width,
            flag_display_height,
            0,
            static_cast<kf::Position>(main_gfx.frame.height - flag_display_height)
        ).value
    };
    left_hold.setFont(kf::fonts::gyver_5x7_en);
    app.left_hold.bindPainter(left_hold);

    kf::Painter right_hold{
        main_gfx.frame.sub(
            column_width,
            flag_display_height,
            column_right_offset,
            static_cast<kf::Position>(main_gfx.frame.height - flag_display_height)
        ).value
    };
    right_hold.setFont(kf::fonts::gyver_5x7_en);
    app.right_hold.bindPainter(right_hold);


    kf::Painter left_toggle{
        main_gfx.frame.sub(
            column_width,
            flag_display_height,
            0,
            static_cast<kf::Position>(main_gfx.frame.height - flag_display_height * 2)
        ).value
    };
    left_toggle.setFont(kf::fonts::gyver_5x7_en);
    app.left_toggle.bindPainter(left_toggle);

    kf::Painter right_toggle{
        main_gfx.frame.sub(
            column_width,
            flag_display_height,
            column_right_offset,
            static_cast<kf::Position>(main_gfx.frame.height - flag_display_height * 2)
        ).value
    };
    right_toggle.setFont(kf::fonts::gyver_5x7_en);
    app.right_toggle.bindPainter(right_toggle);


    display_driver.init();
    Wire.setClock(1000000UL);

    while (true) {
        delay(ms_per_frame);
        app.render(main_gfx);
        display_driver.update();
    }
}

void setup() {
    auto &app = DualJoyApp::instance();

    app.joy_window.add(app.left_joy_widget);
    app.joy_window.add(app.right_joy_widget);
    app.joy_window.add(app.left_hold);
    app.joy_window.add(app.right_hold);
    app.joy_window.add(app.left_toggle);
    app.joy_window.add(app.right_toggle);

    app.left_hold.label = "Hold L";
    app.right_hold.label = "Hold R";
    app.right_toggle.label = "Toggle R";
    app.left_toggle.label = "Toggle L";

    app.init_window.title = "Initializing";
    app.bindWindow(app.init_window);

    Serial.begin(115200);

    initEspNow();

    xTaskCreate(displayTask, "display", 4096, nullptr, 1, nullptr);
    xTaskCreate(inputTask, "input", 4096, nullptr, 1, nullptr);
}

void loop() {}
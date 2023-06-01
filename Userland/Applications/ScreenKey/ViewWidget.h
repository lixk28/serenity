/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Array.h>
#include <AK/Vector.h>
#include <AK/RefPtr.h>
#include <AK/StringView.h>
#include <LibCore/Timer.h>
#include <LibCore/Notifier.h>
#include <LibGUI/Action.h>
#include <LibGUI/Widget.h>
#include <LibGfx/Color.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/Font/FontDatabase.h>
#include <LibThreading/Mutex.h>
#include <LibThreading/Thread.h>

namespace ScreenKey {

class ViewWidget final : public GUI::Widget {
    C_OBJECT(ViewWidget);

public:
    virtual ~ViewWidget() override;

    void set_font();
    void set_font_size();
    void set_font_color(Gfx::Color);
    void set_background_color(Gfx::Color);
    void set_background_opacity();

    constexpr static StringView s_keyboard_device_path { "/dev/input/keyboard/0"sv };

private:
    ViewWidget();
    virtual void paint_event(GUI::PaintEvent&) override;

    RefPtr<Gfx::Font const> m_font { Gfx::FontDatabase::default_font() };
    Gfx::Color m_font_color { Gfx::Color::White };
    Gfx::Color m_background_color { Gfx::Color::Black };

    RefPtr<Core::Notifier> m_keypress_notifier;

    int m_keyboard_fd { -1 };
    // Array<int, 2> m_pipe_fds { -1 };
    Vector<KeyCode> m_key_codes;
    Threading::Mutex m_key_codes_mutex;
    RefPtr<Core::Timer> m_clear_keys_timer;
    // RefPtr<Threading::Thread> m_listening_thread;
};

}

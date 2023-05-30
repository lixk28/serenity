/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Vector.h>
#include <AK/RefPtr.h>
#include <LibCore/Timer.h>
#include <LibGUI/Widget.h>
#include <LibGfx/Font/Font.h>
#include <LibGfx/Font/FontDatabase.h>
#include <LibThreading/Mutex.h>
#include <LibThreading/Thread.h>

namespace ScreenKey {

class ViewWidget final : public GUI::Widget {
    C_OBJECT(ViewWidget);

public:
    virtual ~ViewWidget() override;

private:
    ViewWidget();
    virtual void paint_event(GUI::PaintEvent&) override;

    constexpr static char const* const s_keyboard_device_path = "/dev/input/keyboard/0";

    RefPtr<Gfx::Font const> m_font { Gfx::FontDatabase::default_font() };

    int m_keyboard_fd { -1 };
    int m_pipe_fds[2] { -1 };
    Vector<KeyCode> m_key_codes;
    Threading::Mutex m_key_codes_mutex;
    RefPtr<Core::Timer> m_clear_keys_timer;
    RefPtr<Threading::Thread> m_listening_thread;
};

}

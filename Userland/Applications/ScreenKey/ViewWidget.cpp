/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ViewWidget.h"
#include <LibCore/EventLoop.h>
#include <LibGUI/Painter.h>
#include <LibC/fcntl.h>
#include <LibC/unistd.h>

REGISTER_WIDGET(ScreenKey, ViewWidget)

namespace ScreenKey {

ViewWidget::ViewWidget()
{
    m_keyboard_fd = open(s_keyboard_device_path, O_RDONLY | O_CLOEXEC);
    VERIFY(m_keyboard_fd >= 0);

    m_listening_thread = Threading::Thread::construct([this, &gui_event_loop = Core::EventLoop::current()]() {
        while (true) {
            ::KeyEvent event;
            ssize_t nread = read(m_keyboard_fd, &event, sizeof(::KeyEvent));

            if (nread == sizeof(::KeyEvent)) {
                if (event.is_press()) {
                    Threading::MutexLocker key_codes_locker { m_key_codes_mutex };
                    m_key_codes.append(event.key);
                    gui_event_loop.deferred_invoke([this]() {
                        repaint();
                        m_clear_keys_timer->restart();
                    });
                }
            } else if (nread == 0) {
                continue;
            } else if (nread == -1) {
                perror("Error on reading raw keyboard input!\n");
                return 1;
            } else if ((size_t)nread < sizeof(::KeyEvent)) {
                return 1;
            }
        }
        return 0;
    }, "KeyPress Listener"sv);
    m_listening_thread->start();

    m_clear_keys_timer = MUST(Core::Timer::create_repeating(2000, [this]() {
        Threading::MutexLocker key_codes_locker { m_key_codes_mutex };
        m_key_codes.clear();
        repaint();
    }));
    m_clear_keys_timer->start();
}

ViewWidget::~ViewWidget()
{
    (void)m_listening_thread->join();
    close(m_keyboard_fd);
}

void ViewWidget::paint_event(GUI::PaintEvent& event)
{
    GUI::Painter painter(*this);
    painter.add_clip_rect(event.rect());
    painter.clear_rect(event.rect(), Color::Black);

    Threading::MutexLocker key_codes_locker { m_key_codes_mutex };

    if (m_key_codes.is_empty())
        return;

    StringBuilder builder;
    for (size_t i = 0; i < m_key_codes.size(); i++) {
        builder.append(DeprecatedString(key_code_to_string(m_key_codes[i])));
        if (i != m_key_codes.size() - 1)
            builder.append(' ');
    }
    StringView keys_to_draw = builder.string_view();

    int keys_width = m_font->width_rounded_up(keys_to_draw);
    Gfx::IntPoint center_point = event.rect().center();
    Gfx::IntPoint baseline_start = { center_point.x() - keys_width / 2, center_point.y() };

    painter.draw_text_run(baseline_start, Utf8View(keys_to_draw), *m_font, Color::White);
}

}

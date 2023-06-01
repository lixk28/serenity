/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ViewWidget.h"
#include <LibCore/EventLoop.h>
#include <LibCore/System.h>
#include <LibGUI/Painter.h>
#include <LibC/sched.h>

REGISTER_WIDGET(ScreenKey, ViewWidget)

namespace ScreenKey {

ViewWidget::ViewWidget()
{
    m_keyboard_fd = MUST(Core::System::open(s_keyboard_device_path, O_RDONLY | O_CLOEXEC | O_NONBLOCK));

    m_keypress_notifier = MUST(Core::Notifier::try_create(m_keyboard_fd, Core::Notifier::Type::Read));
    m_keypress_notifier->on_activation = [this, &gui_event_loop = Core::EventLoop::current()] {
        while (true) {
            Array<u8, sizeof(::KeyEvent)> event_buffer;
            ssize_t nread = MUST(Core::System::read(m_keyboard_fd, event_buffer.span()));
            VERIFY(nread >= 0);

            if (nread == 0)
                break;

            if (nread == sizeof(::KeyEvent)) {
                ::KeyEvent event;
                memcpy(&event, event_buffer.data(), sizeof(::KeyEvent));
                if (event.is_press()) {
                    Threading::MutexLocker key_codes_locker { m_key_codes_mutex };
                    m_key_codes.append(event.key);
                    gui_event_loop.deferred_invoke([this]() {
                        repaint();
                        m_clear_keys_timer->restart();
                    });
                }
            }
        }
    };

    // // Make both read and write ends of pipe nonblocking,
    // // this pipe is used for "communication" between the main thread and the listening thread.
    // m_pipe_fds = MUST(Core::System::pipe2(O_NONBLOCK));
    //
    // m_listening_thread = Threading::Thread::construct([this, &gui_event_loop = Core::EventLoop::current()]() {
    //     // The first file descriptor refers to the read end of the pipe,
    //     // which is used to "receive" unblock message from the main thread,
    //     // it's actually known as the self-pipe trick (http://cr.yp.to/docs/selfpipe.html).
    //     // The second one refers to the keyboard device.
    //     pollfd poll_fds[2] = {
    //         { m_pipe_fds[0], POLLIN, 0 },
    //         { m_keyboard_fd, POLLIN, 0 },
    //     };
    //
    //     while (true) {
    //         // Block until there is a fd ready to read.
    //         MUST(Core::System::poll(poll_fds, -1));
    //
    //         // The listening thread is about to exit once there is a byte sent from the main thread.
    //         if (poll_fds[0].revents == POLLIN)
    //             return 0;
    //
    //         // Try reading keyboard events once there is data to read from the keyboard.
    //         if (poll_fds[1].revents == POLLIN) {
    //             Array<u8, sizeof(::KeyEvent)> event_buffer;
    //             ssize_t nread = MUST(Core::System::read(m_keyboard_fd, event_buffer.span()));
    //
    //             if (nread == sizeof(::KeyEvent)) {
    //                 ::KeyEvent event;
    //                 memcpy(&event, event_buffer.data(), sizeof(::KeyEvent));
    //                 if (event.is_press()) {
    //                     Threading::MutexLocker key_codes_locker { m_key_codes_mutex };
    //                     m_key_codes.append(event.key);
    //                     gui_event_loop.deferred_invoke([this]() {
    //                         repaint();
    //                         m_clear_keys_timer->restart();
    //                     });
    //                 }
    //             } else if (nread == 0) {
    //                 continue;
    //             } else if ((size_t)nread < sizeof(::KeyEvent)) {
    //                 return 1;
    //             }
    //         }
    //     }
    //     return 0;
    // }, "KeyPress Listener"sv);
    // MUST(m_listening_thread->set_priority(sched_get_priority_max(SCHED_FIFO)));
    // m_listening_thread->start();

    m_clear_keys_timer = MUST(Core::Timer::create_repeating(2000, [this]() {
        Threading::MutexLocker key_codes_locker { m_key_codes_mutex };
        m_key_codes.clear();
        repaint();
    }));
    m_clear_keys_timer->start();
}

ViewWidget::~ViewWidget()
{
    // Write a byte to the write end of the pipe to "unblock" the listening thread :^)
    // constexpr static char e = 'x';
    // MUST(Core::System::write(m_pipe_fds[1], { &e, sizeof(char) }));

    // Join the listening thread.
    // (void)m_listening_thread->join();

    // Close file descriptors.
    MUST(Core::System::close(m_keyboard_fd));
    // MUST(Core::System::close(m_pipe_fds[0]));
    // MUST(Core::System::close(m_pipe_fds[1]));
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

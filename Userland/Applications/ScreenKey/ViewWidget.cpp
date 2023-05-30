/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ViewWidget.h"
#include <LibCore/EventLoop.h>
#include <LibGUI/Painter.h>
#include <LibC/fcntl.h>
#include <LibC/poll.h>
#include <LibC/sched.h>
#include <LibC/unistd.h>

REGISTER_WIDGET(ScreenKey, ViewWidget)

namespace ScreenKey {

ViewWidget::ViewWidget()
{
    m_keyboard_fd = open(s_keyboard_device_path, O_RDONLY | O_CLOEXEC);
    VERIFY(m_keyboard_fd >= 0);

    int rc = pipe(m_pipe_fds);
    VERIFY(rc != -1);

    // Make both read and write ends of pipe nonblocking.
    auto make_fd_non_blocking = [](int fd) {
        int flags = fcntl(fd, F_GETFL);
        VERIFY(flags != -1);
        flags |= O_NONBLOCK;
        int rc = fcntl(fd, F_SETFL, flags);
        VERIFY(rc != -1);
    };
    make_fd_non_blocking(m_pipe_fds[0]);
    make_fd_non_blocking(m_pipe_fds[1]);

    m_listening_thread = Threading::Thread::construct([this, &gui_event_loop = Core::EventLoop::current()]() {
        // The first file descriptor refers to the read end of the pipe,
        // which is used to "receive" unblock message from the main thread,
        // it's actually known as the self-pipe trick (http://cr.yp.to/docs/selfpipe.html).
        // The second one refers to the keyboard device.
        pollfd poll_fds[2] = {
            { m_pipe_fds[0], POLLIN, 0 },
            { m_keyboard_fd, POLLIN, 0 },
        };

        while (true) {
            // Block until there is a fd ready to read.
            int rc = poll(poll_fds, 2, -1);
            if (rc == -1) {
                perror("Error on polling file descriptors!\n");
                return 1;
            }

            // The listening thread is about to exit once there is a byte sent from the main thread.
            if (poll_fds[0].revents == POLLIN)
                return 0;

            // Try reading keyboard events once there is data to read from the keyboard.
            if (poll_fds[1].revents == POLLIN) {
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
    // Write a byte to the write end of the pipe to "unblock" the listening thread :^)
    char e = 'x';
    write(m_pipe_fds[1], &e, sizeof(char));

    // Join the listening thread.
    (void)m_listening_thread->join();

    // Close file descriptors.
    close(m_keyboard_fd);
    close(m_pipe_fds[0]);
    close(m_pipe_fds[1]);
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

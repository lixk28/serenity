/*
 * Copyright (c) 2022, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "TerminalChangeListener.h"
#include "TerminalTabWidget.h"
#include "TerminalUtilities.h"
#include <LibCore/System.h>
#include <LibConfig/Client.h>
#include <LibGfx/Palette.h>
#include <LibGUI/Button.h>
#include <LibGUI/CheckBox.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Process.h>
#include <LibGUI/TextBox.h>
#include <LibGUI/Window.h>
#include <pty.h>

TerminalTabWidget::TerminalTabWidget()
{
    m_find_window = MUST(create_and_setup_find_window());

    // TODO: This should be moved into main.cpp
    on_tab_count_change = [this](size_t tab_count) {
        set_bar_visible(tab_count > 1);
    };
}

void TerminalTabWidget::keydown_event(GUI::KeyEvent& event)
{
    if (event.ctrl() && event.shift() && event.key() == Key_T) {
        auto new_tab = MUST(create_and_setup_new_tab());
        MUST(add_tab(new_tab, MUST("A New Tab"_string)));
        set_active_widget(new_tab);
    }

    return GUI::TabWidget::keydown_event(event);
}

ErrorOr<NonnullRefPtr<VT::TerminalWidget>> TerminalTabWidget::create_and_setup_new_tab()
{
    int ptm_fd;
    pid_t shell_pid = forkpty(&ptm_fd, nullptr, nullptr, nullptr);
    if (shell_pid == 0) {
        MUST(run_command(Config::read_string("Terminal"sv, "Startup"sv, "Command"sv, ""sv), false));
        VERIFY_NOT_REACHED();
    }

    auto ptsname = TRY(Core::System::ptsname(ptm_fd));
    utmp_update(ptsname, shell_pid, true);

    auto terminal = TRY(VT::TerminalWidget::try_create(ptm_fd, true));

    terminal->on_command_exit = [&] {
        if (tab_count() > 1) {
            remove_tab(terminal);
            activate_last_tab();
        } else if (tab_count() == 1) {
            // Is this OK?
            window()->close();
        } else {
            VERIFY_NOT_REACHED();
        }
    };
    terminal->on_title_change = [&](auto title) {
        window()->set_title(title);
    };
    terminal->on_terminal_size_change = [&](auto size) {
        window()->resize(size);
    };
    terminal->apply_size_increments_to_window(*window());

    Config::monitor_domain("Terminal");
    auto should_confirm_close = Config::read_bool("Terminal"sv, "Terminal"sv, "ConfirmClose"sv, true);
    TerminalChangeListener listener { terminal };

    auto bell = Config::read_string("Terminal"sv, "Window"sv, "Bell"sv, "Visible"sv);
    if (bell == "AudibleBeep") {
        terminal->set_bell_mode(VT::TerminalWidget::BellMode::AudibleBeep);
    } else if (bell == "Disabled") {
        terminal->set_bell_mode(VT::TerminalWidget::BellMode::Disabled);
    } else {
        terminal->set_bell_mode(VT::TerminalWidget::BellMode::Visible);
    }

    auto cursor_shape = VT::TerminalWidget::parse_cursor_shape(Config::read_string("Terminal"sv, "Cursor"sv, "Shape"sv, "Block"sv)).value_or(VT::CursorShape::Block);
    terminal->set_cursor_shape(cursor_shape);

    auto cursor_blinking = Config::read_bool("Terminal"sv, "Cursor"sv, "Blinking"sv, true);
    terminal->set_cursor_blinking(cursor_blinking);

    auto new_opacity = Config::read_i32("Terminal"sv, "Window"sv, "Opacity"sv, 255);
    terminal->set_opacity(new_opacity);
    window()->set_has_alpha_channel(new_opacity < 255);

    auto new_scrollback_size = Config::read_i32("Terminal"sv, "Terminal"sv, "MaxHistorySize"sv, terminal->max_history_size());
    terminal->set_max_history_size(new_scrollback_size);

    auto show_scroll_bar = Config::read_bool("Terminal"sv, "Terminal"sv, "ShowScrollBar"sv, true);
    terminal->set_show_scrollbar(show_scroll_bar);

    auto open_settings_action = TRY(GUI::Action::try_create("Terminal &Settings", TRY(Gfx::Bitmap::load_from_file("/res/icons/16x16/settings.png"sv)), [this](auto&) {
        GUI::Process::spawn_or_show_error(window(), "/bin/TerminalSettings"sv);
    }));
    TRY(terminal->context_menu().try_add_separator());
    TRY(terminal->context_menu().try_add_action(open_settings_action));

    return terminal;
}

ErrorOr<NonnullRefPtr<GUI::Window>> TerminalTabWidget::create_and_setup_find_window()
{
    auto window = TRY(GUI::Window::try_create(*this));
    window->set_window_mode(GUI::WindowMode::RenderAbove);
    window->set_title("Find in Terminal");
    window->set_resizable(false);
    window->resize(300, 90);

    auto main_widget = TRY(window->set_main_widget<GUI::Widget>());
    main_widget->set_fill_with_background_color(true);
    main_widget->set_background_role(ColorRole::Button);
    TRY(main_widget->try_set_layout<GUI::VerticalBoxLayout>(4));

    auto find = TRY(main_widget->try_add<GUI::Widget>());
    TRY(find->try_set_layout<GUI::HorizontalBoxLayout>(4));
    find->set_fixed_height(30);

    auto find_textbox = TRY(find->try_add<GUI::TextBox>());
    find_textbox->set_fixed_width(230);
    find_textbox->set_focus(true);
    if (current_tab()->has_selection())
        find_textbox->set_text(current_tab()->selected_text().replace("\n"sv, " "sv, ReplaceMode::All));
    auto find_backwards = TRY(find->try_add<GUI::Button>());
    find_backwards->set_fixed_width(25);
    find_backwards->set_icon(TRY(Gfx::Bitmap::load_from_file("/res/icons/16x16/upward-triangle.png"sv)));
    auto find_forwards = TRY(find->try_add<GUI::Button>());
    find_forwards->set_fixed_width(25);
    find_forwards->set_icon(TRY(Gfx::Bitmap::load_from_file("/res/icons/16x16/downward-triangle.png"sv)));

    find_textbox->on_return_pressed = [find_backwards] {
        find_backwards->click();
    };

    find_textbox->on_shift_return_pressed = [find_forwards] {
        find_forwards->click();
    };

    auto match_case = TRY(main_widget->try_add<GUI::CheckBox>(TRY("Case sensitive"_string)));
    auto wrap_around = TRY(main_widget->try_add<GUI::CheckBox>(TRY("Wrap around"_string)));

    find_backwards->on_click = [this, find_textbox, match_case, wrap_around](auto) {
        auto needle = find_textbox->text();
        if (needle.is_empty()) {
            return;
        }

        auto found_range = current_tab()->find_previous(needle, current_tab()->normalized_selection().start(), match_case->is_checked(), wrap_around->is_checked());

        if (found_range.is_valid()) {
            current_tab()->scroll_to_row(found_range.start().row());
            current_tab()->set_selection(found_range);
        }
    };
    find_forwards->on_click = [this, find_textbox, match_case, wrap_around](auto) {
        auto needle = find_textbox->text();
        if (needle.is_empty()) {
            return;
        }

        auto found_range = current_tab()->find_next(needle, current_tab()->normalized_selection().end(), match_case->is_checked(), wrap_around->is_checked());

        if (found_range.is_valid()) {
            current_tab()->scroll_to_row(found_range.start().row());
            current_tab()->set_selection(found_range);
        }
    };

    return window;
}

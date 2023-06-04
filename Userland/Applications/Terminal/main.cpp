/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "TerminalChangeListener.h"
#include "TerminalTabWidget.h"
#include "TerminalUtilities.h"
#include <AK/FixedArray.h>
#include <AK/QuickSort.h>
#include <AK/URL.h>
#include <LibConfig/Client.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/Directory.h>
#include <LibCore/System.h>
#include <LibDesktop/Launcher.h>
#include <LibFileSystem/FileSystem.h>
#include <LibGUI/Action.h>
#include <LibGUI/ActionGroup.h>
#include <LibGUI/Application.h>
#include <LibGUI/BoxLayout.h>
#include <LibGUI/Button.h>
#include <LibGUI/CheckBox.h>
#include <LibGUI/ComboBox.h>
#include <LibGUI/Event.h>
#include <LibGUI/Icon.h>
#include <LibGUI/ItemListModel.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Menubar.h>
#include <LibGUI/MessageBox.h>
#include <LibGUI/Process.h>
#include <LibGUI/TextBox.h>
#include <LibGUI/Widget.h>
#include <LibGUI/Window.h>
#include <LibGfx/Font/FontDatabase.h>
#include <LibGfx/Palette.h>
#include <LibMain/Main.h>
#include <LibVT/TerminalWidget.h>
#include <assert.h>
#include <errno.h>
#include <pty.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

static void utmp_update(DeprecatedString const& tty, pid_t pid, bool create)
{
    int utmpupdate_pid = fork();
    if (utmpupdate_pid < 0) {
        perror("fork");
        return;
    }
    if (utmpupdate_pid == 0) {
        // Be careful here! Because fork() only clones one thread it's
        // possible that we deadlock on anything involving a mutex,
        // including the heap! So resort to low-level APIs
        char pid_str[32];
        snprintf(pid_str, sizeof(pid_str), "%d", pid);
        execl("/bin/utmpupdate", "/bin/utmpupdate", "-f", "Terminal", "-p", pid_str, (create ? "-c" : "-d"), tty.characters(), nullptr);
    } else {
    wait_again:
        int status = 0;
        if (waitpid(utmpupdate_pid, &status, 0) < 0) {
            int err = errno;

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    TRY(Core::System::pledge("stdio tty rpath cpath wpath recvfd sendfd proc exec unix sigaction"));

    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_flags = SA_NOCLDWAIT;
    act.sa_handler = SIG_IGN;

    TRY(Core::System::sigaction(SIGCHLD, &act, nullptr));

    auto app = TRY(GUI::Application::create(arguments));

    TRY(Core::System::pledge("stdio tty rpath cpath wpath recvfd sendfd proc exec unix"));

    Config::pledge_domain("Terminal");

    StringView command_to_execute;
    bool keep_open = false;

    Core::ArgsParser args_parser;
    args_parser.add_option(command_to_execute, "Execute this command inside the terminal", nullptr, 'e', "command");
    args_parser.add_option(keep_open, "Keep the terminal open after the command has finished executing", nullptr, 'k');

    args_parser.parse(arguments);

    if (keep_open && command_to_execute.is_empty()) {
        warnln("Option -k can only be used in combination with -e.");
        return 1;
    }

    int ptm_fd;
    pid_t shell_pid = forkpty(&ptm_fd, nullptr, nullptr, nullptr);
    if (shell_pid < 0) {
        perror("forkpty");
        return 1;
    }
    if (shell_pid == 0) {
        close(ptm_fd); // this line is redundent
        if (!command_to_execute.is_empty())
            TRY(run_command(command_to_execute, keep_open));
        else
            TRY(run_command(Config::read_string("Terminal"sv, "Startup"sv, "Command"sv, ""sv), false));
        VERIFY_NOT_REACHED();
    }

    auto ptsname = TRY(Core::System::ptsname(ptm_fd));
    utmp_update(ptsname, shell_pid, true);

    auto app_icon = GUI::Icon::default_icon("app-terminal"sv);

    auto window = TRY(GUI::Window::try_create());
    window->set_title("Terminal");
    window->set_obey_widget_min_size(false);

    auto tab_widget = TRY(window->set_main_widget<TerminalTabWidget>());
    TRY(tab_widget->add_tab(TRY(VT::TerminalWidget::try_create(ptm_fd, true)), "Tab1"_short_string));

    auto open_settings_action = GUI::Action::create("Terminal &Settings", TRY(Gfx::Bitmap::load_from_file("/res/icons/16x16/settings.png"sv)),
        [&](auto&) {
            GUI::Process::spawn_or_show_error(window, "/bin/TerminalSettings"sv);
        });

    auto file_menu = TRY(window->try_add_menu("&File"_short_string));
    TRY(file_menu->try_add_action(GUI::Action::create("Open New &Terminal", { Mod_Ctrl | Mod_Shift, Key_N }, TRY(Gfx::Bitmap::load_from_file("/res/icons/16x16/app-terminal.png"sv)), [&](auto&) {
        GUI::Process::spawn_or_show_error(window, "/bin/Terminal"sv);
    })));

    TRY(file_menu->try_add_action(open_settings_action));
    TRY(file_menu->try_add_separator());

    // TODO: Add action {New tab, Close tab} in file_menu

    auto tty_has_foreground_process = [&] {
        pid_t fg_pid = tcgetpgrp(ptm_fd);
        return fg_pid != -1 && fg_pid != shell_pid;
    };

    auto shell_child_process_count = [&] {
        int background_process_count = 0;
        Core::Directory::for_each_entry(DeprecatedString::formatted("/proc/{}/children", shell_pid), Core::DirIterator::Flags::SkipParentAndBaseDir, [&](auto&, auto&) {
            ++background_process_count;
            return IterationDecision::Continue;
        }).release_value_but_fixme_should_propagate_errors();
        return background_process_count;
    };

    auto check_terminal_quit = [&]() -> GUI::Dialog::ExecResult {
        if (!should_confirm_close)
            return GUI::MessageBox::ExecResult::OK;
        Optional<DeprecatedString> close_message;
        auto title = "Running Process"sv;
        if (tty_has_foreground_process()) {
            close_message = "Close Terminal and kill its foreground process?";
        } else {
            auto child_process_count = shell_child_process_count();
            if (child_process_count > 1) {
                title = "Running Processes"sv;
                close_message = DeprecatedString::formatted("Close Terminal and kill its {} background processes?", child_process_count);
            } else if (child_process_count == 1) {
                close_message = "Close Terminal and kill its background process?";
            }
        }
        if (close_message.has_value())
            return GUI::MessageBox::show(window, *close_message, title, GUI::MessageBox::Type::Warning, GUI::MessageBox::InputType::OKCancel);
        return GUI::MessageBox::ExecResult::OK;
    };

    TRY(file_menu->try_add_action(GUI::CommonActions::make_quit_action([&](auto&) {
        dbgln("Terminal: Quit menu activated!");
        if (check_terminal_quit() == GUI::MessageBox::ExecResult::OK)
            GUI::Application::the()->quit();
    })));

    auto edit_menu = TRY(window->try_add_menu("&Edit"_short_string));
    TRY(edit_menu->try_add_action(terminal->copy_action()));
    TRY(edit_menu->try_add_action(terminal->paste_action()));
    TRY(edit_menu->try_add_separator());
    TRY(edit_menu->try_add_action(GUI::Action::create("&Find...", { Mod_Ctrl | Mod_Shift, Key_F }, TRY(Gfx::Bitmap::load_from_file("/res/icons/16x16/find.png"sv)),
        [&](auto&) {
            tab_widget->find_window().show();
            tab_widget->find_window().move_to_front();
        })));

    auto view_menu = TRY(window->try_add_menu("&View"_short_string));
    TRY(view_menu->try_add_action(GUI::CommonActions::make_fullscreen_action([&](auto&) {
        window->set_fullscreen(!window->is_fullscreen());
    })));
    TRY(view_menu->try_add_action(tab_widget->current_tab().clear_including_history_action()));

    auto adjust_font_size = [&](float adjustment) {
        auto& font = tab_widget->current_tab().font();
        auto new_size = max(5, font.presentation_size() + adjustment);
        if (auto new_font = Gfx::FontDatabase::the().get(font.family(), new_size, font.weight(), font.width(), font.slope())) {
            tab_widget->current_tab().set_font_and_resize_to_fit(*new_font);
            tab_widget->current_tab().apply_size_increments_to_window(*window);
            window->resize(tab_widget->current_tab().size());
        }
    };

    TRY(view_menu->try_add_separator());
    TRY(view_menu->try_add_action(GUI::CommonActions::make_zoom_in_action([&](auto&) {
        adjust_font_size(1);
    })));
    TRY(view_menu->try_add_action(GUI::CommonActions::make_zoom_out_action([&](auto&) {
        adjust_font_size(-1);
    })));

    auto help_menu = TRY(window->try_add_menu("&Help"_short_string));
    TRY(help_menu->try_add_action(GUI::CommonActions::make_command_palette_action(window)));
    TRY(help_menu->try_add_action(GUI::CommonActions::make_help_action([](auto&) {
        Desktop::Launcher::open(URL::create_with_file_scheme("/usr/share/man/man1/Applications/Terminal.md"), "/bin/Help");
    })));
    TRY(help_menu->try_add_action(GUI::CommonActions::make_about_action("Terminal", app_icon, window)));

    window->on_close_request = [&]() -> GUI::Window::CloseRequestDecision {
        if (check_terminal_quit() == GUI::MessageBox::ExecResult::OK)
            return GUI::Window::CloseRequestDecision::Close;
        return GUI::Window::CloseRequestDecision::StayOpen;
    };

    window->on_input_preemption_change = [&](bool is_preempted) {
        tab_widget->current_tab().set_logical_focus(!is_preempted);
    };

    TRY(Core::System::unveil("/res", "r"));
    TRY(Core::System::unveil("/bin", "r"));
    TRY(Core::System::unveil("/proc", "r"));
    TRY(Core::System::unveil("/bin/Shell", "x"));
    TRY(Core::System::unveil("/bin/Terminal", "x"));
    TRY(Core::System::unveil("/bin/TerminalSettings", "x"));
    TRY(Core::System::unveil("/bin/utmpupdate", "x"));
    TRY(Core::System::unveil("/dev/ptmx", "rw"));
    TRY(Core::System::unveil("/dev/pts", "rw"));
    TRY(Core::System::unveil("/etc/FileIconProvider.ini", "r"));
    TRY(Core::System::unveil("/etc/passwd", "r"));
    TRY(Core::System::unveil("/tmp/session/%sid/portal/launch", "rw"));
    TRY(Core::System::unveil("/tmp/session/%sid/portal/config", "rw"));
    TRY(Core::System::unveil(nullptr, nullptr));

    auto modified_state_check_timer = TRY(Core::Timer::create_repeating(500, [&] {
        window->set_modified(tty_has_foreground_process() || shell_child_process_count() > 0);
    }));

    listener.on_confirm_close_changed = [&](bool confirm_close) {
        if (confirm_close) {
            modified_state_check_timer->start();
        } else {
            modified_state_check_timer->stop();
            window->set_modified(false);
        }
        should_confirm_close = confirm_close;
    };

    window->show();
    if (should_confirm_close)
        modified_state_check_timer->start();
    int result = app->exec();
    dbgln("Exiting terminal, updating utmp");
    utmp_update(ptsname, 0, false);
    return result;
}

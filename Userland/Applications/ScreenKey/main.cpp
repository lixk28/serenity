/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PreferencesDialog.h"
#include "ViewWidget.h"
#include <AK/String.h>
#include <LibConfig/Client.h>
#include <LibCore/System.h>
#include <LibGUI/Application.h>
#include <LibGUI/Menu.h>
#include <LibGUI/Window.h>
#include <LibMain/Main.h>

using namespace ScreenKey;

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    TRY(Core::System::pledge("stdio recvfd sendfd rpath wpath cpath unix proc exec thread"));

    auto app = TRY(GUI::Application::create(arguments));

    TRY(Core::System::unveil("/res"sv, "r"sv));
    TRY(Core::System::unveil(ViewWidget::s_keyboard_device_path, "r"sv));
    // TRY(Core::System::unveil("/bin/ScreenKeySettings", "x"));
    TRY(Core::System::unveil(nullptr, nullptr));

    auto window = TRY(GUI::Window::try_create());
    window->set_title("ScreenKey");
    window->resize(800, 100);
    window->center_on_screen();
    window->set_resizable(true);
    window->set_always_on_top(true);

    auto file_menu = TRY(window->try_add_menu("File"_short_string));
    TRY(file_menu->try_add_action(GUI::Action::create("Settings", [&](GUI::Action const&) {
        auto preferences_dialog = MUST(PreferencesDialog::try_create(window));
        if (preferences_dialog->exec() != GUI::Dialog::ExecResult::OK)
            return;
            // preferences_dialog->revert_possible_changes();
        // preferences_dialog->exec();
    })));
    file_menu->add_separator();
    TRY(file_menu->try_add_action(GUI::CommonActions::make_quit_action([&](GUI::Action const&) {
        app->quit();
    })));

    auto main_widget = TRY(window->set_main_widget<ViewWidget>());
    main_widget->set_fill_with_background_color(true);

    window->show();
    return app->exec();
}

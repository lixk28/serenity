/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "ViewWidget.h"
#include <LibConfig/Client.h>
#include <LibCore/System.h>
#include <LibGUI/Application.h>
#include <LibGUI/Window.h>
#include <LibMain/Main.h>

using namespace ScreenKey;

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    TRY(Core::System::pledge("stdio recvfd sendfd rpath wpath cpath unix proc exec thread"));

    auto app = TRY(GUI::Application::create(arguments));

    auto window = TRY(GUI::Window::try_create());
    window->set_title("ScreenKey");
    window->resize(800, 100);
    window->center_on_screen();
    window->set_resizable(true);
    window->set_always_on_top(true);

    auto main_widget = TRY(window->set_main_widget<ViewWidget>());
    main_widget->set_fill_with_background_color(true);

    window->show();
    return app->exec();
}

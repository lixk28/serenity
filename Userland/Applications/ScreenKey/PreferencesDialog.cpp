/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "PreferencesDialog.h"
#include <LibGUI/BoxLayout.h>

namespace ScreenKey {

PreferencesDialog::PreferencesDialog(GUI::Window* parent_window, Dialog::ScreenPosition position)
    : GUI::Dialog(parent_window, position)
{
    set_title("ScreenKey Preferences");

    auto main_widget = MUST(set_main_widget<GUI::Widget>());
    main_widget->set_layout<GUI::VerticalBoxLayout>();

    m_keys_compress_spinbox = GUI::SpinBox::construct();
    m_keys_compress_spinbox->set_min(1);
    m_keys_compress_spinbox->set_max(10);
    m_keys_compress_spinbox->set_value(2);
    // m_keys_compress_spinbox->on_change = [&](auto) {
    // };

    m_background_opacity_slider = MUST(GUI::HorizontalOpacitySlider::try_create());
    // m_background_opacity_slider->on_change = [&](auto) {
    // };

    resize(600, 400);
    set_resizable(true);

    main_widget->add_child(*m_keys_compress_spinbox);
    main_widget->add_child(*m_background_opacity_slider);
}

}

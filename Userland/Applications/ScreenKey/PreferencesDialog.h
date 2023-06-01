/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGUI/Dialog.h>
#include <LibGUI/ColorPicker.h>
#include <LibGUI/ComboBox.h>
#include <LibGUI/FontPicker.h>
#include <LibGUI/OpacitySlider.h>
#include <LibGUI/SpinBox.h>

namespace ScreenKey {

class PreferencesDialog final : public GUI::Dialog {
    C_OBJECT(PreferencesDialog);

public:

private:
    PreferencesDialog(GUI::Window* parent_window, Dialog::ScreenPosition position = Dialog::ScreenPosition::Center);

    // compress key after repeat N times
    RefPtr<GUI::SpinBox> m_keys_compress_spinbox;

    // raw / composed / symbolic
    // RefPtr<GUI::ComboBox> m_key_display_mode { nullptr };

    RefPtr<GUI::HorizontalOpacitySlider> m_background_opacity_slider;
    // RefPtr<GUI::ColorButton> m_background_color_button { nullptr };
    // RefPtr<GUI::ColorButton> m_background_color_picker { nullptr };

    // RefPtr<GUI::FontPicker> m_font_picker { nullptr };
};

}

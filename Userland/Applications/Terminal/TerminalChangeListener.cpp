/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "TerminalChangeListener.h"
#include <LibGUI/Window.h>

void TerminalChangeListener::config_bool_did_change(DeprecatedString const& domain, DeprecatedString const& group, DeprecatedString const& key, bool value)
{
    VERIFY(domain == "Terminal");

    if (group == "Terminal") {
        if (key == "ShowScrollBar")
            m_parent_terminal.set_show_scrollbar(value);
        else if (key == "ConfirmClose" && on_confirm_close_changed)
            on_confirm_close_changed(value);
    } else if (group == "Cursor" && key == "Blinking") {
        m_parent_terminal.set_cursor_blinking(value);
    }
}

void TerminalChangeListener::config_string_did_change(DeprecatedString const& domain, DeprecatedString const& group, DeprecatedString const& key, DeprecatedString const& value)
{
    VERIFY(domain == "Terminal");

    if (group == "Window" && key == "Bell") {
        auto bell_mode = VT::TerminalWidget::BellMode::Visible;
        if (value == "AudibleBeep")
            bell_mode = VT::TerminalWidget::BellMode::AudibleBeep;
        if (value == "Visible")
            bell_mode = VT::TerminalWidget::BellMode::Visible;
        if (value == "Disabled")
            bell_mode = VT::TerminalWidget::BellMode::Disabled;
        m_parent_terminal.set_bell_mode(bell_mode);
    } else if (group == "Text" && key == "Font") {
        auto font = Gfx::FontDatabase::the().get_by_name(value);
        if (font.is_null())
            font = Gfx::FontDatabase::default_fixed_width_font();
        m_parent_terminal.set_font_and_resize_to_fit(*font);
        m_parent_terminal.apply_size_increments_to_window(*m_parent_terminal.window());
        m_parent_terminal.window()->resize(m_parent_terminal.size());
    } else if (group == "Cursor" && key == "Shape") {
        auto cursor_shape = VT::TerminalWidget::parse_cursor_shape(value).value_or(VT::CursorShape::Block);
        m_parent_terminal.set_cursor_shape(cursor_shape);
    }
}

void TerminalChangeListener::config_i32_did_change(DeprecatedString const& domain, DeprecatedString const& group, DeprecatedString const& key, i32 value)
{
    VERIFY(domain == "Terminal");

    if (group == "Terminal" && key == "MaxHistorySize") {
        m_parent_terminal.set_max_history_size(value);
    } else if (group == "Window" && key == "Opacity") {
        m_parent_terminal.set_opacity(value);
    }
}

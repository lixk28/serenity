/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibVT/TerminalWidget.h>
#include <LibGUI/TabWidget.h>
#include <LibGUI/Window.h>

class TerminalTabWidget final : public GUI::TabWidget {
    C_OBJECT(TerminalTabWidget);

public:
    ErrorOr<void> add_tab(NonnullRefPtr<VT::TerminalWidget> const& tab, String title);
    // ErrorOr<void> remove_current_tab();

    // ErrorOr<NonnullRefPtr<VT::TerminalWidget>> create_and_setup_new_tab_at();
    // Note: These casts are OK since we only add VT::TerminalWidget
    VT::TerminalWidget& current_tab() { return *verify_cast<VT::TerminalWidget>(active_widget()); }
    // NonnullRefPtr<VT::TerminalWidget> last_active_tab() const;

    GUI::Window& find_window() const { return *m_find_window; };

private:
    TerminalTabWidget();

    virtual void keydown_event(GUI::KeyEvent& event) override;

    ErrorOr<NonnullRefPtr<VT::TerminalWidget>> create_and_setup_new_tab();
    ErrorOr<NonnullRefPtr<GUI::Window>> create_and_setup_find_window();

    RefPtr<GUI::Window> m_find_window;
};

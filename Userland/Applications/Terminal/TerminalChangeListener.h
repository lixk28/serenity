/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibGfx/Font/FontDatabase.h>
#include <LibConfig/Listener.h>
#include <LibVT/TerminalWidget.h>

class TerminalChangeListener final : public Config::Listener {
public:
    TerminalChangeListener(VT::TerminalWidget& parent_terminal)
        : m_parent_terminal(parent_terminal)
    {
    }

    virtual void config_bool_did_change(DeprecatedString const& domain, DeprecatedString const& group, DeprecatedString const& key, bool value) override;
    virtual void config_string_did_change(DeprecatedString const& domain, DeprecatedString const& group, DeprecatedString const& key, DeprecatedString const& value) override;
    virtual void config_i32_did_change(DeprecatedString const& domain, DeprecatedString const& group, DeprecatedString const& key, i32 value) override;

    Function<void(bool)> on_confirm_close_changed;

private:
    VT::TerminalWidget& m_parent_terminal;
};

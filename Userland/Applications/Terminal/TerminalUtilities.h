/*
 * Copyright (c) 2023, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/DeprecatedString.h>
#include <AK/StringView.h>
#include <sys/types.h>

void utmp_update(DeprecatedString const& tty, pid_t pid, bool create);

ErrorOr<void> run_command(DeprecatedString command, bool keep_open);

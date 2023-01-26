#!/usr/bin/env -S bash ../.port_include.sh
port=mg
version=3.5
files="https://github.com/troglobit/mg/releases/download/v${version}/mg-${version}.tar.gz mg-${version}.tar.gz a906eab9370c0f24a5fa25923561ad933b74ad339d0b2851d2067badf0d7e4ce"
auth_type=sha256
useconfigure=true
use_fresh_config_sub=true
use_fresh_config_guess=true
config_sub_paths=("build-aux/config.sub")
config_guess_paths=("build-aux/config.guess")
configopts=("--without-curses")

pre_configure() {
    run "aclocal"
}

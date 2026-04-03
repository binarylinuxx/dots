#
# spec file for package blxshell-shell
#
# Copyright (c) 2026 Nir Rudov <nrw58886@gmail.com>
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license given to you by the open source license is solely the
# license under which you may use this software. All other rights reserved.
#
# THIS PACKAGE IS PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
# MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#

Name:           blxshell-shell
Version:        1.0
Release:        1
Summary:        Blxshell Shell Dependencies (terminal, shell, fonts, theming)
License:        custom
URL:            https://github.com/binarylinuxx/dots
BuildArch:      noarch

# shell & terminal
Requires:       fish
Requires:       ghostty
Requires:       starship

# fonts — available in repos
Requires:       jetbrains-mono-fonts
Requires:       symbols-only-nerd-fonts
Requires:       twemoji-color-font

# fonts — custom (published in home:binarylinuxx:blxshell OBS repo)
Requires:       blxshell-font-material-symbols
Requires:       blxshell-font-readex-pro
Requires:       blxshell-font-rubik
Requires:       blxshell-font-space-grotesk

# col_gen dependencies (Material You color generation)
# NOTE: uv has no zypper package — installed via install.sh using astral.sh installer
Requires:       python313
Requires:       python313-Pillow

%description
Metapackage pulling in all shell, terminal and font dependencies for blxshell.

Includes fish shell, ghostty terminal, starship prompt, all required fonts,
and Python dependencies for the col_gen Material You color generator.

Note: uv (Python project manager) is installed separately via the blxshell
installer script as it has no openSUSE package.

%files
# metapackage — no files

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

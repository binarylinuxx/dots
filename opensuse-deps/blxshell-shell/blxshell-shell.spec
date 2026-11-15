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
Version:        2.0
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

# colours from the wallpaper
Requires:       matugen

%description
Metapackage pulling in all shell, terminal and font dependencies for blxshell.

Includes fish shell, ghostty terminal, starship prompt, the fonts and
matugen for colours from the wallpaper.

%files
# metapackage — no files

%changelog
* Sat Nov 14 2026 Nir Rudov <nrw58886@gmail.com> - 2.0-1
- For Vellum: matugen instead of the Python colour generator
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

#
# spec file for package blxshell-hyprland
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

Name:           blxshell-hyprland
Version:        1.0
Release:        4
Summary:        Blxshell Hyprland Dependencies (WM, compositor, portals)
License:        custom
URL:            https://github.com/binarylinuxx/dots
BuildArch:      noarch

Requires:       hyprland
Requires:       hyprsunset
Requires:       wl-clipboard
Requires:       xdg-desktop-portal
# xdg-desktop-portal-kde → xdg-desktop-portal-kde6 on openSUSE (KF6 variant)
Requires:       xdg-desktop-portal-kde6
Requires:       xdg-desktop-portal-gtk
Requires:       xdg-desktop-portal-hyprland

%description
Metapackage pulling in all Hyprland compositor and portal dependencies
for blxshell.

Includes Hyprland WM, hyprsunset for night light, wl-clipboard for
Wayland clipboard support, and all required xdg-desktop-portal backends.

%files
# metapackage — no files

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-4
- Initial openSUSE package

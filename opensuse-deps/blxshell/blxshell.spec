#
# spec file for package blxshell
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

# Set to a pushed commit of dots main (and in _fetch).
%define _commit PIN_AFTER_PUSH

Name:           blxshell
Version:        4.0.0~git
Release:        1
Summary:        Material You desktop shell for zwwm and Hyprland, in C++ on vw
License:        GPL-3.0-only
URL:            https://github.com/binarylinuxx/dots

Source0:        dots-%{_commit}.tar.gz

BuildRequires:  clang
BuildRequires:  cmake
BuildRequires:  ninja
BuildRequires:  shaderc
BuildRequires:  vw-devel

Requires:       blxshell-fonts
Recommends:     matugen
Recommends:     NetworkManager
Recommends:     bluez
Recommends:     upower
Recommends:     power-profiles-daemon
Suggests:       gpu-screen-recorder
Suggests:       greetd

%description
blxshell "Vellum": a Material You desktop shell for zwwm and Hyprland, written
in C++ on vw. Bar, launcher, dashboard, sidebar, notifications, lock screen,
wallpaper picker and settings, plus blxshell-greeter, a greetd login screen.

%prep
%autosetup -n dots-%{_commit}

%build
export CC=clang CXX=clang++
cmake -GNinja -B build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DVW_SOURCE_DIR=
cmake --build build

%install
DESTDIR=%{buildroot} cmake --install build

%files
%license LICENSE
%{_bindir}/blxshell
%{_bindir}/blxshell-greeter
%{_datadir}/blxshell/

%changelog
* Sat Nov 14 2026 Nir Rudov <nrw58886@gmail.com> - 4.0.0~git-1
- Vellum: blxshell rewritten in C++ on vw (replaces the Quickshell packages)

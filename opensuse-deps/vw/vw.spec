#
# spec file for package vw
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

%define _commit 6892f4eeb6934ff2500fb16cb1314cf2152cf0a6
%define _zwwm 1912b91d325e039a21b1c17f87ed16b26d755689

Name:           vw
Version:        0.1.0~git.6892f4e
Release:        1
Summary:        C++23 Wayland widget library with its own Vulkan renderer
License:        BSD-3-Clause
URL:            https://github.com/binarylinuxx/vw

Source0:        vw-6892f4e.tar.gz
Source1:        zwwm-1912b91.tar.gz

BuildRequires:  clang
BuildRequires:  cmake
BuildRequires:  ninja
BuildRequires:  shaderc
BuildRequires:  pam-devel
BuildRequires:  quickjs-devel
BuildRequires:  stb-devel
BuildRequires:  nlohmann_json-devel
BuildRequires:  vulkan-devel
BuildRequires:  wayland-protocols-devel
BuildRequires:  pkgconfig(cairo)
BuildRequires:  pkgconfig(fontconfig)
BuildRequires:  pkgconfig(freetype2)
BuildRequires:  pkgconfig(gio-unix-2.0)
BuildRequires:  pkgconfig(gtk+-3.0)
BuildRequires:  pkgconfig(harfbuzz)
BuildRequires:  pkgconfig(libpipewire-0.3)
BuildRequires:  pkgconfig(librsvg-2.0)
BuildRequires:  pkgconfig(libwebp)
BuildRequires:  pkgconfig(pangocairo)
BuildRequires:  pkgconfig(wayland-client)
BuildRequires:  pkgconfig(xkbcommon)

%description
vw is a C++23 widget library for Wayland with its own Vulkan renderer: layer
shell surfaces, text, animation and services (PipeWire, MPRIS, network,
Bluetooth, battery, tray, notifications) for desktop shells and widgets.

%package devel
Summary:        Headers and CMake package for vw
Requires:       %{name} = %{version}
Requires:       nlohmann_json-devel

%description devel
Headers, the CMake package (find_package(vw)) and vw-shaderc for building
programs against vw.

%prep
%setup -q -n vw-%{_commit}
%setup -q -T -D -a 1 -n vw-%{_commit}

%build
export CC=clang CXX=clang++
cmake -GNinja -B build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DCMAKE_INSTALL_LIBDIR=%{_lib} \
    -DZWAYLAND_ROOT=$PWD/zwwm-%{_zwwm} \
    -DVW_WITH_LUA=OFF \
    -DVW_BUILD_EXAMPLES=OFF
cmake --build build

%install
DESTDIR=%{buildroot} cmake --install build
# zwwm's protocol files come along from the zwayland build; zwwm owns them.
rm -rf %{buildroot}%{_datadir}/zwwm

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%license LICENSE
%{_libdir}/libvw.so.*

%files devel
%{_bindir}/vw-shaderc
%{_includedir}/vw/
%{_libdir}/libvw.so
%{_libdir}/cmake/vw/
%{_datadir}/vw/

%changelog
* Sat Nov 14 2026 Nir Rudov <nrw58886@gmail.com> - 0.1.0~git.6892f4e-1
- Initial openSUSE package

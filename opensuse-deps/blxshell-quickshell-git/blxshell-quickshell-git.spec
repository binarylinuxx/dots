#
# spec file for package blxshell-quickshell-git
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

%define _commit 4b77936

Name:           blxshell-quickshell-git
Version:        0.1.0~git.4b77936
Release:        13
Summary:        Quickshell pinned commit build for blxshell
License:        LGPL-3.0-only
URL:            https://github.com/quickshell-mirror/quickshell
# Conflicts with upstream quickshell packages
Conflicts:      quickshell
Provides:       quickshell = %{version}

Source0:        quickshell-4b77936.tar.gz

BuildRequires:  cmake
BuildRequires:  ninja
BuildRequires:  git
BuildRequires:  qt6-base-devel
BuildRequires:  qt6-declarative-devel
BuildRequires:  qt6-quick-private-devel
BuildRequires:  qt6-wayland-private-devel
BuildRequires:  qt6-base-private-devel
BuildRequires:  qt6-gui-private-devel
BuildRequires:  qt6-svg-devel
BuildRequires:  qt6-wayland-devel
BuildRequires:  qt6-shadertools-devel
BuildRequires:  spirv-tools
BuildRequires:  wayland-devel
BuildRequires:  wayland-protocols-devel
BuildRequires:  libxcb-devel
BuildRequires:  libdrm-devel
BuildRequires:  pipewire-devel
BuildRequires:  jemalloc-devel
BuildRequires:  cli11-devel
BuildRequires:  Mesa-devel
BuildRequires:  Mesa-libGLESv3-devel
BuildRequires:  pam-devel
BuildRequires:  patchelf
BuildRequires:  polkit-devel

# Runtime dependencies
# qt6-declarative → libQt6Qml6 + libQt6Quick6 on openSUSE
Requires:       libQt6Qml6
Requires:       libQt6Quick6
# qt6-base → libQt6Core6 + libQt6Gui6 on openSUSE
Requires:       libQt6Core6
Requires:       libQt6Gui6
# jemalloc → libjemalloc2 on openSUSE
Requires:       libjemalloc2
# qt6-svg → libQt6Svg6 on openSUSE
Requires:       libQt6Svg6
# libpipewire → libpipewire-0_3-0 on openSUSE
Requires:       libpipewire-0_3-0
# libxcb → libxcb1 on openSUSE
Requires:       libxcb1
# wayland → libwayland-client0 on openSUSE
Requires:       libwayland-client0
# libdrm → libdrm2 on openSUSE
Requires:       libdrm2
Requires:       Mesa
# google-breakpad: bundled via cmake FetchContent — no system package needed

%description
Quickshell built from a pinned git commit (db1777c) for blxshell compatibility.

Quickshell is a flexible, QtQuick-based desktop shell toolkit for Wayland.
This package pins a specific upstream commit to ensure compatibility with
blxshell's QML components, as the upstream release may lag behind.

Conflicts with any other quickshell package.

%prep
%autosetup -n quickshell-4b77936-4b77936

%build
cmake -GNinja -B build \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DDISTRIBUTOR="OBS (home:binarylinuxx:blxshell)" \
    -DDISTRIBUTOR_DEBUGINFO_AVAILABLE=NO \
    -DINSTALL_QML_PREFIX=lib/qt6/qml \
    -DCRASH_HANDLER=OFF \
    -DCMAKE_C_FLAGS="-I/usr/include/wayland" \
    -DCMAKE_CXX_FLAGS="-I/usr/include/wayland"
cmake --build build

%install
DESTDIR=%{buildroot} cmake --install build
install -Dm644 LICENSE %{buildroot}%{_datadir}/licenses/%{name}/LICENSE
# Remove embedded RPATH — quickshell sets $ORIGIN-relative rpath which rpmlint rejects
patchelf --remove-rpath %{buildroot}%{_bindir}/quickshell

%files
%license LICENSE
%{_bindir}/quickshell
%{_bindir}/qs
%dir /usr/lib/qt6
%dir /usr/lib/qt6/qml
/usr/lib/qt6/qml/Quickshell/
%{_datadir}/applications/org.quickshell.desktop
%dir %{_datadir}/icons/hicolor
%dir %{_datadir}/icons/hicolor/scalable
%dir %{_datadir}/icons/hicolor/scalable/apps
%{_datadir}/icons/hicolor/scalable/apps/org.quickshell.svg

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 0.1.0~git.4b77936-13
- Strip embedded RPATH from quickshell binary with patchelf to pass rpmlint
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 0.1.0~git.4b77936-12
- Add %%dir entries for unowned directories in %%files
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 0.1.0~git.4b77936-11
- Add /usr/bin/qs (brp-suse symlink) to %%files
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 0.1.0~git.4b77936-10
- Fix %%files: use literal /usr/lib/qt6/qml/Quickshell/ path, add icons/desktop entries
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 0.1.0~git.4b77936-9
- Add pam-devel BuildRequires for security/_pam_types.h
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 0.1.0~git.4b77936-8
- Add Mesa-libGLESv3-devel BuildRequires for GLES3/gl32.h
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 0.1.0~git.4b77936-7
- Switch to GitHub mirror (quickshell-mirror/quickshell) for OBS reliability
- Update pinned commit to 4b77936
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 0.1.0~git.db1777c-6
- Initial openSUSE package, pinned commit db1777c

#
# spec file for package blxshell-audio
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

Name:           blxshell-audio
Version:        1.0
Release:        1
Summary:        Blxshell Audio Dependencies (pipewire, controls, visualizers)
License:        custom
URL:            https://github.com/binarylinuxx/dots
BuildArch:      noarch

Requires:       cava
Requires:       pavucontrol-qt
Requires:       wireplumber
# pipewire-pulse → pipewire-pulseaudio on openSUSE
Requires:       pipewire-pulseaudio
# libdbusmenu-gtk3 → libdbusmenu-gtk3-4 on openSUSE (versioned soname)
Requires:       libdbusmenu-gtk3-4
Requires:       playerctl

%description
Metapackage pulling in all audio dependencies for blxshell.

Includes PipeWire PulseAudio backend, WirePlumber session manager,
cava audio visualizer, pavucontrol-qt volume control, and playerctl
for media player control via MPRIS.

%files
# metapackage — no files

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

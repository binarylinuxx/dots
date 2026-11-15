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
Version:        2.0
Release:        1
Summary:        Blxshell Audio Dependencies (pipewire, controls, visualizers)
License:        custom
URL:            https://github.com/binarylinuxx/dots
BuildArch:      noarch

Requires:       cava
Requires:       pavucontrol
Requires:       wireplumber
# pipewire-pulse → pipewire-pulseaudio on openSUSE
Requires:       pipewire-pulseaudio

%description
Metapackage pulling in all audio dependencies for blxshell.

Includes the PipeWire PulseAudio backend, the WirePlumber session manager,
the cava audio visualizer and pavucontrol. blxshell talks to PipeWire and
MPRIS itself.

%files
# metapackage — no files

%changelog
* Sat Nov 14 2026 Nir Rudov <nrw58886@gmail.com> - 2.0-1
- For Vellum: pavucontrol instead of pavucontrol-qt; drop playerctl and dbusmenu
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

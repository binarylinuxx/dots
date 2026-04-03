#
# spec file for package blxshell-font-space-grotesk
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

Name:           blxshell-font-space-grotesk
Version:        1.0
Release:        1
Summary:        Blxshell Font: Space Grotesk variable typeface
License:        OFL-1.1
URL:            https://github.com/floriankarsten/space-grotesk
BuildArch:      noarch

Provides:       otf-space-grotesk
Conflicts:      otf-space-grotesk

Requires:       fontconfig

Source0:        https://github.com/floriankarsten/space-grotesk/raw/master/fonts/ttf/SpaceGrotesk%5Bwght%5D.ttf#/SpaceGrotesk.ttf

%description
Space Grotesk variable typeface for blxshell.

Space Grotesk is a variable sans-serif typeface with a weight axis, based
on Space Mono. Used as a secondary sans-serif in blxshell's UI elements.

No openSUSE package exists for this font — this spec downloads the TTF
directly from the upstream floriankarsten/space-grotesk repository.

%install
mkdir -p %{buildroot}%{_datadir}/fonts/blxshell
install -Dm644 %{SOURCE0} %{buildroot}%{_datadir}/fonts/blxshell/SpaceGrotesk.ttf

%post
%{_bindir}/fc-cache -f 2>/dev/null || :

%postun
%{_bindir}/fc-cache -f 2>/dev/null || :

%files
%dir %{_datadir}/fonts/blxshell
%{_datadir}/fonts/blxshell/SpaceGrotesk.ttf

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

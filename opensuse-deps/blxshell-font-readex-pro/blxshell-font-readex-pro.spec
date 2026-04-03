#
# spec file for package blxshell-font-readex-pro
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

Name:           blxshell-font-readex-pro
Version:        1.0
Release:        1
Summary:        Blxshell Font: Readex Pro variable typeface
License:        OFL-1.1
URL:            https://github.com/Readex-Pro/Readex-Pro
BuildArch:      noarch

Provides:       ttf-readex-pro
Conflicts:      ttf-readex-pro

Requires:       fontconfig

Source0:        https://github.com/ThomasJockin/readexpro/raw/master/fonts/variable/Readexpro%5BHEXP%2Cwght%5D.ttf#/ReadexPro.ttf

%description
Readex Pro variable typeface for blxshell.

Readex Pro is a variable font designed for readability across Latin and
Arabic scripts, used as a secondary typeface in blxshell's UI.

No openSUSE package exists for this font — this spec downloads the TTF
directly from the upstream Readex Pro repository.

%install
mkdir -p %{buildroot}%{_datadir}/fonts/blxshell
install -Dm644 %{SOURCE0} %{buildroot}%{_datadir}/fonts/blxshell/ReadexPro.ttf

%post
%{_bindir}/fc-cache -f 2>/dev/null || :

%postun
%{_bindir}/fc-cache -f 2>/dev/null || :

%files
%dir %{_datadir}/fonts/blxshell
%{_datadir}/fonts/blxshell/ReadexPro.ttf

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

#
# spec file for package blxshell-font-rubik
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

Name:           blxshell-font-rubik
Version:        1.0
Release:        1
Summary:        Blxshell Font: Rubik variable typeface
License:        OFL-1.1
URL:            https://github.com/googlefonts/rubik
BuildArch:      noarch

Provides:       ttf-rubik-vf
Conflicts:      ttf-rubik-vf

Requires:       fontconfig

Source0:        https://github.com/googlefonts/rubik/raw/main/fonts/variable/Rubik%5Bwght%5D.ttf#/Rubik.ttf
Source1:        https://github.com/googlefonts/rubik/raw/main/fonts/variable/Rubik-Italic%5Bwght%5D.ttf#/Rubik-Italic.ttf

%description
Rubik variable typeface for blxshell.

Rubik is a variable font family with slightly rounded corners, designed for
display and UI use. Used as a display/heading typeface in blxshell.

No openSUSE package exists for the variable version — this spec downloads
the TTFs directly from the upstream googlefonts/rubik repository.

%install
mkdir -p %{buildroot}%{_datadir}/fonts/blxshell
install -Dm644 %{SOURCE0} %{buildroot}%{_datadir}/fonts/blxshell/Rubik.ttf
install -Dm644 %{SOURCE1} %{buildroot}%{_datadir}/fonts/blxshell/Rubik-Italic.ttf

%post
%{_bindir}/fc-cache -f 2>/dev/null || :

%postun
%{_bindir}/fc-cache -f 2>/dev/null || :

%files
%dir %{_datadir}/fonts/blxshell
%{_datadir}/fonts/blxshell/Rubik.ttf
%{_datadir}/fonts/blxshell/Rubik-Italic.ttf

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

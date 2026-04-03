#
# spec file for package blxshell-font-googlesans
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

Name:           blxshell-font-googlesans
Version:        1.0
Release:        1
Summary:        Blxshell Font: Google Sans Flex variable typeface
License:        OFL-1.1
URL:            https://fonts.google.com/specimen/Google+Sans+Flex
BuildArch:      noarch

Provides:       ttf-google-sans-flex
Conflicts:      ttf-google-sans-flex

Requires:       fontconfig

Source0:        https://github.com/LineageOS/android_external_google-fonts_google-sans-flex/raw/lineage-23.0/GoogleSansFlex-Regular.ttf

%description
Google Sans Flex variable typeface for blxshell.

Google Sans Flex is a variable version of Google Sans with a weight axis,
used as the primary UI typeface in blxshell's Material You theme.

%install
mkdir -p %{buildroot}%{_datadir}/fonts/blxshell
install -Dm644 %{SOURCE0} %{buildroot}%{_datadir}/fonts/blxshell/GoogleSansFlex-Regular.ttf

%post
%{_bindir}/fc-cache -f 2>/dev/null || :

%postun
%{_bindir}/fc-cache -f 2>/dev/null || :

%files
%dir %{_datadir}/fonts/blxshell
%{_datadir}/fonts/blxshell/GoogleSansFlex-Regular.ttf

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

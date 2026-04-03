#
# spec file for package blxshell-font-bitcount
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

Name:           blxshell-font-bitcount
Version:        1.0
Release:        1
Summary:        Blxshell Font: Bitcount Single variable pixel fonts
License:        OFL-1.1
URL:            https://github.com/petrvanblokland/TYPETR-Bitcount
BuildArch:      noarch

Provides:       ttf-bitcount-single-variable
Conflicts:      ttf-bitcount-single-variable

Requires:       fontconfig

Source0:        https://github.com/petrvanblokland/TYPETR-Bitcount/raw/main/fonts/ttf/variable/BitcountSingle%5BCRSV%2CELSH%2CELXP%2Cslnt%2Cwght%5D.ttf#/BitcountSingle.ttf
Source1:        https://github.com/petrvanblokland/TYPETR-Bitcount/raw/main/fonts/ttf/variable/BitcountGridSingle%5BCRSV%2CELSH%2CELXP%2Cslnt%2Cwght%5D.ttf#/BitcountGridSingle.ttf

%description
Bitcount Single variable pixel fonts for blxshell (Prop + Grid variants).

Bitcount is a variable pixel font family by Peter van Blokland / TYPETR.
Both the proportional (BitcountSingle) and grid-aligned (BitcountGridSingle)
variable fonts are included, with axes: CRSV, ELSH, ELXP, slnt, wght.

%install
mkdir -p %{buildroot}%{_datadir}/fonts/blxshell
install -Dm644 %{SOURCE0} %{buildroot}%{_datadir}/fonts/blxshell/BitcountSingle.ttf
install -Dm644 %{SOURCE1} %{buildroot}%{_datadir}/fonts/blxshell/BitcountGridSingle.ttf

%post
%{_bindir}/fc-cache -f 2>/dev/null || :

%postun
%{_bindir}/fc-cache -f 2>/dev/null || :

%files
%dir %{_datadir}/fonts/blxshell
%{_datadir}/fonts/blxshell/BitcountSingle.ttf
%{_datadir}/fonts/blxshell/BitcountGridSingle.ttf

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

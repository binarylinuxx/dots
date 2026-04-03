#
# spec file for package blxshell-font-material-symbols
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

Name:           blxshell-font-material-symbols
Version:        1.0
Release:        1
Summary:        Blxshell Font: Material Symbols variable icon font (Google)
License:        OFL-1.1
URL:            https://github.com/google/material-design-icons
BuildArch:      noarch

Provides:       ttf-material-symbols-variable
Conflicts:      ttf-material-symbols-variable
# Arch AUR equivalent: ttf-material-symbols-variable-git

Requires:       fontconfig

Source0:        https://github.com/google/material-design-icons/raw/master/variablefont/MaterialSymbolsOutlined%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf#/MaterialSymbolsOutlined.ttf
Source1:        https://github.com/google/material-design-icons/raw/master/variablefont/MaterialSymbolsRounded%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf#/MaterialSymbolsRounded.ttf
Source2:        https://github.com/google/material-design-icons/raw/master/variablefont/MaterialSymbolsSharp%5BFILL%2CGRAD%2Copsz%2Cwght%5D.ttf#/MaterialSymbolsSharp.ttf

%description
Google Material Symbols variable icon fonts for blxshell.

Material Symbols is Google's icon font with variable axes for FILL, GRAD,
opsz, and wght. All three styles (Outlined, Rounded, Sharp) are included.
Used throughout blxshell's Quickshell UI for iconography.

No openSUSE package exists for this font — this spec downloads the TTFs
directly from the upstream Google material-design-icons repository.

%install
mkdir -p %{buildroot}%{_datadir}/fonts/blxshell
install -Dm644 %{SOURCE0} %{buildroot}%{_datadir}/fonts/blxshell/MaterialSymbolsOutlined.ttf
install -Dm644 %{SOURCE1} %{buildroot}%{_datadir}/fonts/blxshell/MaterialSymbolsRounded.ttf
install -Dm644 %{SOURCE2} %{buildroot}%{_datadir}/fonts/blxshell/MaterialSymbolsSharp.ttf

%post
%{_bindir}/fc-cache -f 2>/dev/null || :

%postun
%{_bindir}/fc-cache -f 2>/dev/null || :

%files
%dir %{_datadir}/fonts/blxshell
%{_datadir}/fonts/blxshell/MaterialSymbolsOutlined.ttf
%{_datadir}/fonts/blxshell/MaterialSymbolsRounded.ttf
%{_datadir}/fonts/blxshell/MaterialSymbolsSharp.ttf

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

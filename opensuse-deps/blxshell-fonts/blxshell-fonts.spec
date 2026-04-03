#
# spec file for package blxshell-fonts
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

Name:           blxshell-fonts
Version:        1.0
Release:        1
Summary:        Blxshell Fonts — all custom font packages
License:        custom
URL:            https://github.com/binarylinuxx/dots
BuildArch:      noarch

# Custom font packages from home:binarylinuxx:blxshell
Requires:       blxshell-font-bitcount
Requires:       blxshell-font-googlesans
Requires:       blxshell-font-material-symbols
Requires:       blxshell-font-readex-pro
Requires:       blxshell-font-rubik
Requires:       blxshell-font-space-grotesk

%description
Metapackage that pulls in all custom font packages required by blxshell.

All fonts are sourced from their upstream repositories and packaged in
the home:binarylinuxx:blxshell OBS repository.

%files
# metapackage — no files

%changelog
* Fri Mar 13 2026 Nir Rudov <nrw58886@gmail.com> - 1.0-1
- Initial openSUSE package

#
# spec file for package dualsensectl
#
# Copyright (c) 2023 SUSE LLC
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via https://bugs.opensuse.org/
#


Name:           dualsensectl
Version:        0.3
Release:        0
Summary:        Tool for controlling PlayStation 5 DualSense controllers on Linux
License:        GPL-2.0
URL:            https://github.com/nowrep/dualsensectl
Source0:        https://github.com/nowrep/%{name}/archive/refs/tags/v%{version}.tar.gz#/%{name}-%{version}.tar.gz
BuildRequires:  gcc-devel gcc-c++ dbus-1-devel libdbus-c++-devel systemd-devel
BuildRequires:  libhidapi-devel libudev-devel
Requires:       gcc gcc-c++ dbus-1
Requires:       libhidapi-hidraw0 libhidapi-libusb0
Requires:       udev libudev1

%description
DualSense Control [DualSenseCTL] is a tool for controlling Sony PlayStation 5 DualSense
controllers on Linux.

%prep
%setup

%build
%make_build %{?_smp_mflags}

%install
%make_install

%files
%{_bindir}/%{name}
%dir %{_datadir}/bash-completion
%dir %{_datadir}/bash-completion/completions
%{_datadir}/bash-completion/completions/dualsensectl
%dir %{_datadir}/zsh
%dir %{_datadir}/zsh/site-functions
%{_datadir}/zsh/site-functions/_dualsensectl
%{_datadir}/rules/70-dualsensectl.rules
%license LICENSE
%doc README.md


%changelog
* Sat May 06 2023 12:20:30 nowrep@gmail.com; martin.von.reichenberg@proton.me
- Added a new monitoring command for scheduling/adjusting certain (even multiple) events [David Rosca <nowrep@gmail.com>]
- Added a new  `dualsense -d DEVICE`  option for listing of multiple connected devices and their identification [David Rosca <nowrep@gmail.com>]
- Fixed battery level status not showing correct percents (%) when plugged in [David Rosca <nowrep@gmail.com>]
- Added installable packages for Debian/Ubuntu - `DEB` [Martin Stibor <martin.von.reichenberg@protonmail.com>]
- Added installable packages for openSUSE/Fedora - `RPM` [Martin Stibor <martin.von.reichenberg@protonmail.com>]
- Added installable packages for Arch Linux - `PKG.TAR.ZST` [Martin Stibor <martin.von.reichenberg@protonmail.com>]
- More coming 'soon' . . .

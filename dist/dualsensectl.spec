Name:           dualsensectl
Version:        0.7
Release:        1%{?dist}
Summary:        DualSense Control

%global forgeurl https://github.com/nowrep/dualsensectl
%global tag v%{version}
%forgemeta

License:        GPL-2.0-or-later
URL:            %forgeurl
Source:         %forgesource

BuildRequires:  gcc
BuildRequires:  meson
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(hidapi-hidraw)
BuildRequires:  pkgconfig(libudev)

%description
Command-line tool for Sony DualSense controllers.

%prep
%forgeautosetup

%build
%meson
%meson_build

%install
%meson_install
install -m 644 -D -t %{buildroot}/%{_defaultdocdir}/%{name} README.md

%files
%{_bindir}/%{name}
%{_defaultdocdir}/%{name}

%changelog
* Mon Feb 10 2025 Alexander Kapshuna <kapsh@kap.sh> - 0.7-1
- Updated to 0.7.
* Wed Apr 10 2024 Alexander Kapshuna <kapsh@kap.sh> - 0.5-1
- Initial spec.

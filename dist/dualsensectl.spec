Name:           dualsensectl
Version:        0.5
Release:        1%{?dist}
Summary:        DualSense Control

%global forgeurl https://github.com/nowrep/dualsensectl
%global tag v%{version}
%forgemeta

License:        GPL-2.0-or-later
URL:            %forgeurl
Source:         %forgesource

BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(hidapi-hidraw)
BuildRequires:  pkgconfig(libudev)

%description
Command-line tool for Sony DualSense controllers

%prep
%forgeautosetup
sed -i Makefile -e "/^CFLAGS/ s|-s||"

%build
%make_build

%install
%make_install
install -m 644 -D -t %{buildroot}/%{_defaultdocdir}/%{name} README.md

%files
%{_bindir}/%{name}
%{_defaultdocdir}/%{name}
%{bash_completions_dir}/%{name}
%{zsh_completions_dir}/_%{name}

%changelog
* Wed Apr 10 2024 Alexander Kapshuna <kapsh@kap.sh> - 0.5-1
- Initial spec.

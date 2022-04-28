Name:    bpctl
Version: %{__version}
Release: %{__release}%{?dist}

License: GNU AGPLv3
URL: https://github.com/redBorder/bpctl
Source0: %{name}-%{version}.tar.gz

Requires: bpctl-dkms 

Summary: Silicom Linux Bypass-SD Control Utility
Group:   Development/Utilities

%description
%{summary}

%prep
%setup -qn %{name}-%{version}

%build
make

%install
mkdir -p %{buildroot}/usr/bin
INSTALL_MOD_PATH=%{buildroot}/usr make install

%clean
rm -rf %{buildroot}

%pre

%post
%postun

%files
%defattr(755,root,root)
/usr/bin/bpctl_util
/usr/bin/bpctl_start                                                                             
/usr/bin/bpctl_stop                                                                              
%defattr(644,root,root)

%changelog
* Wed Apr 25 2022 David Vanhoucke <dvanhoucke@redborder.com>
- split bp utility and dkms kernel module
* Wed Apr 13 2022 David Vanhoucke <dvanhoucke@redborder.com>
- first spec version

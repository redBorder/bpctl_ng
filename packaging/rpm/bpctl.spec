Name:    bpctl
Version: %{__version}
Release: %{__release}%{?dist}

License: GNU AGPLv3
URL: https://github.com/redBorder/bpctl
Source0: %{name}-%{version}.tar.gz

BuildRequires: kernel-devel kernel-headers

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

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%defattr(755,root,root)
/usr/bin/bpctl_util
/usr/bin/bpctl_start                                                                             
/usr/bin/bpctl_stop                                                                              
/usr/lib/modules/%{__kernel}.el7.x86_64/modules.alias                                       
/usr/lib/modules/%{__kernel}.el7.x86_64/modules.alias.bin
/usr/lib/modules/%{__kernel}.el7.x86_64/modules.builtin.bin
/usr/lib/modules/%{__kernel}.el7.x86_64/modules.dep    
/usr/lib/modules/%{__kernel}.el7.x86_64/modules.dep.bin    
/usr/lib/modules/%{__kernel}.el7.x86_64/modules.devname                                     
/usr/lib/modules/%{__kernel}.el7.x86_64/modules.softdep                                     
/usr/lib/modules/%{__kernel}.el7.x86_64/modules.symbols                                       
/usr/lib/modules/%{__kernel}.el7.x86_64/modules.symbols.bin                                   
/usr/lib/modules/%{__kernel}.el7.x86_64/updates/drivers/net/ethernet/intel/bpctl_mod/bpctl_mod.ko

%defattr(644,root,root)

%changelog
* Wed Apr 13 2022 David Vanhoucke <dvanhoucke@redborder.com>
- first spec version

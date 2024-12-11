%{?!module_name: %{error: You did not specify a module name (%%module_name)}}
%{?!version: %{error: You did not specify a module version (%%version)}}
%{?!kernel_versions: %{error: You did not specify kernel versions (%%kernel_version)}}
%{?!packager: %define packager DKMS <dkms-devel@lists.us.dell.com>}
%{?!license: %define license Unknown}
%{?!_dkmsdir: %define _dkmsdir /var/lib/dkms}
%{?!_srcdir: %define _srcdir %_prefix/src}
%{?!_datarootdir: %define _datarootdir %{_datadir}}

Summary:	%{module_name} %{version} dkms package
Name:		%{module_name}-dkms
Version:	%{version}
License:	%license
Release:	%{release}
BuildArch:	noarch
Group:		System/Kernel
Requires: 	dkms >= 1.958
AutoReqProv: 	no
BuildRequires: 	dkms kernel-devel
Requires:       kernel-headers kernel-devel make
BuildRoot: 	%{_tmppath}/%{name}-%{version}-%{release}-root/

%description
Kernel modules for %{module_name} %{version} in a DKMS wrapper.

%prep
if [ "%mktarball_line" != "none" ]; then
        /usr/sbin/dkms mktarball -m %module_name -v %version %mktarball_line --archive `basename %{module_name}-%{version}.dkms.tar.gz`
        cp -af %{_dkmsdir}/%{module_name}/%{version}/tarball/`basename %{module_name}-%{version}.dkms.tar.gz` %{module_name}-%{version}.dkms.tar.gz
fi

# Execution order:
# install:    pre -> (copy) -> post
# upgrade:    pre -> (copy) -> post -> preun (old) -> (delete old) -> postun (old)
# un-install:                          preun       -> (delete)     -> postun

%pre
case "$1" in
	1)
	# install
	;;
	2)
	# upgrade
	dkms remove -m %{module_name} -v %{version} --all --rpm_safe_upgrade
	;;
esac
\/bin/rm -f /lib/modules/*/weak-updates/bpctl_mod.ko*
\/bin/rm -f /lib/modules/*/extra/bpctl_mod.ko*
\/bin/rm -fr /var/lib/dkms/%{module_name}

%install
if [ "$RPM_BUILD_ROOT" != "/" ]; then
        echo "Deleting build root directory: $RPM_BUILD_ROOT"
        rm -rf $RPM_BUILD_ROOT
fi
mkdir -p $RPM_BUILD_ROOT%{_srcdir}
mkdir -p $RPM_BUILD_ROOT%{_datarootdir}/%{module_name}

if [ -d %{_sourcedir}/%{module_name}-%{version} ]; then
        cp -Lpr %{_sourcedir}/%{module_name}-%{version} $RPM_BUILD_ROOT%{_srcdir}
fi

if [ -f %{module_name}-%{version}.dkms.tar.gz ]; then
        install -m 644 %{module_name}-%{version}.dkms.tar.gz $RPM_BUILD_ROOT%{_datarootdir}/%{module_name}
fi

%post
case "$1" in
	1)
	# install
	;;

	2)
	# upgrade
	;;
esac

if [ -f "%{_datarootdir}/%{module_name}/%{module_name}-%{version}.dkms.tar.gz" ]; then
    if ! dkms ldtarball --archive "%{_datarootdir}/%{module_name}/%{module_name}-%{version}.dkms.tar.gz"; then
        echo ""
        echo ""
        echo "Unable to load DKMS tarball %{_datarootdir}/%{module_name}/%{module_name}-%{version}.dkms.tar.gz."
        echo "Common causes include: "
        echo " - You must be using DKMS 2.1.0.0 or later to support binaries only"
        echo "   distribution specific archives."
        echo " - Corrupt distribution specific archive"
        echo ""
        echo ""
        exit 2
    fi

    if ! dkms install -m %{module_name} -v %{version}; then
      echo "ERROR: Failed to install DKMS module %{module_name} version %{version}"
      exit 1
    fi
elif [ -d "%{_sourcedir}/%{module_name}-%{version}" ]; then
    occurrences=/usr/sbin/dkms status | grep "%{module_name}" | grep "%{version}" | wc -l
    if [ ! occurrences > 0 ];
    then
      if ! dkms add -m %{module_name} -v %{version}; then
        echo "ERROR: Failed to add DKMS module %{module_name} version %{version}"
        exit 1
      fi
    fi

    if ! dkms build -m %{module_name} -v %{version}; then
      echo "ERROR: Failed to build DKMS module %{module_name} version %{version}"
      exit 1
    fi

    if ! dkms install -m %{module_name} -v %{version}; then
        echo "ERROR: Failed to install DKMS module %{module_name} version %{version}"
        exit 1
    fi
fi


%preun
echo -e
echo -e "Uninstall of %{module_name} module (version %{version}) beginning:"

case "$1" in
	0)
	# un-install
	dkms remove -m %{module_name} -v %{version} --all --rpm_safe_upgrade
	;;
	1)
	# upgrade
	;;
esac
exit 0

%postun
case "$1" in
	0)
	# un-install
	\/bin/rm -f /lib/modules/*/weak-updates/bpctl_mod.ko*
	\/bin/rm -f /lib/modules/*/extra/bpctl_mod.ko*
	\/bin/rm -fr /var/lib/dkms/%{module_name}
	;;
	1)
	# upgrade
	;;
esac

%clean
if [ "$RPM_BUILD_ROOT" != "/" ]; then
        rm -rf $RPM_BUILD_ROOT
fi

%files
%defattr(-,root,root)
%{_srcdir}
%{_datarootdir}/%{module_name}/

%changelog
* %(date "+%a %b %d %Y") %packager %{version}-%{release}
- Automatic build by DKMS


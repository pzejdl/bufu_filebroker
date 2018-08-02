%{!?ver:	  %define ver	   0.0}
%{!?rel:	  %define rel      0}
%{!?name:	  %define name     ferolconfig}

# Prevents stripping debug symbols
%global _enable_debug_package 0
%global debug_package %{nil}
%global __os_install_post /usr/lib/rpm/brp-compress %{nil}

Name: %{name}
Summary: BUFU FileName Broker Server Service
Version: %{ver}
Release: %{rel}
License: GPL
Group: CMS/System
Vendor: Petr Zejdl <petr.zejdl@cern.ch>
Packager: Petr Zejdl <petr.zejdl@cern.ch>
Requires: systemd
Source0: %{name}-%{version}.tgz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
## Because we ship a busybox binary, we are not anymore noarch
#BuildArch: noarch
BuildRequires: systemd
# Disable automatic detection of requirements
AutoReqProv: no

%description
The package installs the BUFU FileName Broker service.

%prep
%setup -q

%build

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p ${RPM_BUILD_ROOT}/opt/bufu_filebroker/bin
mkdir -p ${RPM_BUILD_ROOT}/opt/bufu_filebroker/lib
mkdir -p ${RPM_BUILD_ROOT}/opt/bufu_filebroker/scripts
mkdir -p ${RPM_BUILD_ROOT}/opt/bufu_filebroker/systemd
mkdir -p ${RPM_BUILD_ROOT}/usr/lib/systemd/system

install -m 755 bin/bufu_filebroker $RPM_BUILD_ROOT/opt/bufu_filebroker/bin/bufu_filebroker
install -m 644 lib/libboost_chrono.so.1.67.0 $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/libboost_chrono.so.1.67.0
install -m 644 lib/libboost_filesystem.so.1.67.0 $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/libboost_filesystem.so.1.67.0
install -m 644 lib/libboost_regex.so.1.67.0 $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/libboost_regex.so.1.67.0
install -m 644 lib/libboost_system.so.1.67.0 $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/libboost_system.so.1.67.0
install -m 644 lib/libgcc_s.so $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/libgcc_s.so
install -m 644 lib/libgcc_s.so.1 $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/libgcc_s.so.1

#install -m 644 lib/libstdc++.so $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/libstdc++.so
cp -a lib/libstdc++.so $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/
#install -m 644 lib/libstdc++.so.6 $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/libstdc++.so.6
cp -a lib/libstdc++.so.6 $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/

install -m 644 lib/libstdc++.so.6.0.22 $RPM_BUILD_ROOT/opt/bufu_filebroker/lib/libstdc++.so.6.0.22
install -m 644 README $RPM_BUILD_ROOT/opt/bufu_filebroker/README
install -m 755 scripts/bufu_filebroker.sh $RPM_BUILD_ROOT/opt/bufu_filebroker/scripts/bufu_filebroker.sh
install -m 755 scripts/crash_notify.sh $RPM_BUILD_ROOT/opt/bufu_filebroker/scripts/crash_notify.sh
install -m 755 scripts/f3mon_notify.sh $RPM_BUILD_ROOT/opt/bufu_filebroker/scripts/f3mon_notify.sh
install -m 755 systemd/install_service.sh $RPM_BUILD_ROOT/opt/bufu_filebroker/systemd/install_service.sh
install -m 644 systemd/README $RPM_BUILD_ROOT/opt/bufu_filebroker/systemd/README
install -m 755 systemd/uninstall_service.sh $RPM_BUILD_ROOT/opt/bufu_filebroker/systemd/uninstall_service.sh
install -m 644 systemd/bufu_filebroker_crash_notify.service $RPM_BUILD_ROOT/opt/bufu_filebroker/systemd/bufu_filebroker_crash_notify.service
install -m 644 systemd/bufu_filebroker.service $RPM_BUILD_ROOT/opt/bufu_filebroker/systemd/bufu_filebroker.service

install -m 644 systemd/bufu_filebroker_crash_notify.service $RPM_BUILD_ROOT/usr/lib/systemd/system/bufu_filebroker_crash_notify.service
install -m 644 systemd/bufu_filebroker.service $RPM_BUILD_ROOT/usr/lib/systemd/system/bufu_filebroker.service

%post
%systemd_post bufu_filebroker.service

%preun
%systemd_preun bufu_filebroker.service

%postun
%systemd_postun_with_restart bufu_filebroker.service

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%dir /opt/bufu_filebroker
%dir /opt/bufu_filebroker/bin
%dir /opt/bufu_filebroker/lib
%dir /opt/bufu_filebroker/scripts
%dir /opt/bufu_filebroker/systemd
/opt/bufu_filebroker/bin/bufu_filebroker
/opt/bufu_filebroker/lib/libboost_chrono.so.1.67.0
/opt/bufu_filebroker/lib/libboost_filesystem.so.1.67.0
/opt/bufu_filebroker/lib/libboost_regex.so.1.67.0
/opt/bufu_filebroker/lib/libboost_system.so.1.67.0
/opt/bufu_filebroker/lib/libgcc_s.so
/opt/bufu_filebroker/lib/libgcc_s.so.1
/opt/bufu_filebroker/lib/libstdc++.so
/opt/bufu_filebroker/lib/libstdc++.so.6
/opt/bufu_filebroker/lib/libstdc++.so.6.0.22
/opt/bufu_filebroker/README
/opt/bufu_filebroker/scripts/bufu_filebroker.sh
/opt/bufu_filebroker/scripts/crash_notify.sh
/opt/bufu_filebroker/scripts/f3mon_notify.sh
/opt/bufu_filebroker/systemd/install_service.sh
/opt/bufu_filebroker/systemd/README
/opt/bufu_filebroker/systemd/uninstall_service.sh
/opt/bufu_filebroker/systemd/bufu_filebroker_crash_notify.service
/opt/bufu_filebroker/systemd/bufu_filebroker.service
/usr/lib/systemd/system/bufu_filebroker_crash_notify.service
/usr/lib/systemd/system/bufu_filebroker.service

%doc

%changelog

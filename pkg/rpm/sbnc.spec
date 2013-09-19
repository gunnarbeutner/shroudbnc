Summary: a modular IRC proxy written in C++/TCL
Name: sbnc
Version: 1.3beta7
Release: 2
License: GPL
Group: Productivity/Networking/IRC
Source: https://sourceforge.net/projects/sbnc/files/1.3/sbnc-1.3beta7.tar.gz
URL: http://www.shroudbnc.info/
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

BuildRequires: tcl-devel
BuildRequires: openssl-devel
BuildRequires: gcc-c++

%description
shroudBNC is a modular IRC proxy written in C++.
It is capable of proxying IRC connections for
multiple users. Using TCL scripts it can be extended.

%prep
%setup -n %{name}-%{version}

%build
./configure --prefix=/usr --enable-tcl --enable-ssl --enable-ipv6 --disable-identd
make %{?_smp_mflags} all

%install
make DESTDIR=$RPM_BUILD_ROOT install

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && [ -d $RPM_BUILD_ROOT ] && rm -rf $RPM_BUILD_ROOT;

%define _sharedir %{_prefix}/share
%define _libdir %{_prefix}/lib

%files
%defattr(-,root,root)
%{_bindir}/sbnc
%dir %{_libdir}/sbnc
%{_libdir}/sbnc/libbnctcl.a
%{_libdir}/sbnc/libbnctcl.la
%{_libdir}/sbnc/libbnctcl.so
%{_libdir}/sbnc/libbnctcl.so.0
%{_libdir}/sbnc/libbnctcl.so.0.0.0
%dir %{_sharedir}/sbnc
%dir %{_sharedir}/sbnc/scripts
%{_sharedir}/sbnc/scripts/account.tcl
%{_sharedir}/sbnc/scripts/alltools.tcl
%{_sharedir}/sbnc/scripts/auth.tcl
%{_sharedir}/sbnc/scripts/bind.tcl
%{_sharedir}/sbnc/scripts/botnet.tcl
%{_sharedir}/sbnc/scripts/channel.tcl
%{_sharedir}/sbnc/scripts/contact.tcl
%{_sharedir}/sbnc/scripts/defaultsettings.tcl
%{_sharedir}/sbnc/scripts/iface.tcl
%{_sharedir}/sbnc/scripts/iface2.tcl
%{_sharedir}/sbnc/scripts/ifacecmds.tcl
%{_sharedir}/sbnc/scripts/itype.tcl
%{_sharedir}/sbnc/scripts/lock.tcl
%{_sharedir}/sbnc/scripts/misc.tcl
%{_sharedir}/sbnc/scripts/namespace.tcl
%{_sharedir}/sbnc/scripts/partyline.tcl
%{_sharedir}/sbnc/scripts/prowl.tcl
%{_sharedir}/sbnc/scripts/pushmode.tcl
%{_sharedir}/sbnc/scripts/qauth.tcl
%{_sharedir}/sbnc/scripts/sbnc.tcl.dist
%{_sharedir}/sbnc/scripts/socket.tcl
%{_sharedir}/sbnc/scripts/stubs.tcl
%{_sharedir}/sbnc/scripts/tcl.tcl
%{_sharedir}/sbnc/scripts/telnet.tcl
%{_sharedir}/sbnc/scripts/telnetclient.tcl
%{_sharedir}/sbnc/scripts/timers.tcl
%{_sharedir}/sbnc/scripts/usys.tcl
%{_sharedir}/sbnc/scripts/variables.tcl
%{_sharedir}/sbnc/scripts/vhost.tcl
%{_sharedir}/sbnc/scripts/virtual.tcl

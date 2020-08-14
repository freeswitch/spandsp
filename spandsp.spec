# BUILD using: rpmbuild -ba spandsp.spec
# DOC: https://fedoraproject.org/wiki/Packaging:SourceURL
%global commit f995f452ee79d03c4afa90b10e86a5f9c553920c

Summary: A DSP library for telephony.
Name: spandsp3
Version: 3.0.0
Release: 1
License: LGPLv2 and GPLv2
Group: System Environment/Libraries
URL: http://www.soft-switch.org
%undefine _disable_source_fetch
Source0: https://github.com/freeswitch/spandsp/archive/%{commit}.tar.gz
BuildRoot: %{_tmppath}/spandsp-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: libtiff-devel%{?_isa}
BuildRequires: libjpeg-turbo-devel%{?_isa}
BuildRequires: libxml2-devel%{?_isa}
BuildRequires: libsndfile-devel%{?_isa}
BuildRequires: doxygen
BuildRequires: libxslt
BuildRequires: docbook-style-xsl

%description
SpanDSP is a library of DSP functions for telephony, in the 8000
sample per second world of E1s, T1s, and higher order PCM channels. It
contains low level functions, such as basic filters. It also contains
higher level functions, such as cadenced supervisory tone detection,
and a complete software FAX machine. The software has been designed to
avoid intellectual property issues, using mature techniques where all
relevant patents have expired. See the file DueDiligence for important
information about these intellectual property issues.

%package devel
Summary: SpanDSP development files
Group: Development/Libraries
Conflicts: spandsp-devel
Requires: spandsp3%{?_isa} = %{version}-%{release}
Requires: libtiff-devel%{?_isa}
Requires: libjpeg-turbo-devel%{?_isa}

%description devel
SpanDSP development files.

%package apidoc
Summary: SpanDSP API documentation
Group: Development/Libraries

%description apidoc
SpanDSP API documentation.

%prep
%autosetup -n spandsp-%{commit}

%build
autoreconf -i
%configure --enable-doc --with-pic --prefix=/usr
make
find doc/api -type f | xargs touch -r configure

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}
rm %{buildroot}%{_libdir}/libspandsp.la
mkdir -p %{buildroot}%{_datadir}/spandsp

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%doc DueDiligence ChangeLog AUTHORS COPYING NEWS README

%{_libdir}/libspandsp.so.*

%{_datadir}/spandsp

%files devel
%defattr(-,root,root,-)
%{_includedir}/spandsp.h
%{_includedir}/spandsp
%{_libdir}/libspandsp.so
%{_libdir}/libspandsp.a
%{_libdir}/pkgconfig/spandsp.pc

%files apidoc
%defattr(-,root,root,-)
%doc doc/api/html/*

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%changelog
* Fri Aug 14 2020 FreeSWITCH Solutions <packages@freeswitch.com> 3.0.0-1
- Initial release for RPM based distros

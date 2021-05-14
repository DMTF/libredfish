Name:		libredfish
Version:	1.3.1
Release:	1%{?dist}
Summary:	libRedfish is a C client library that allows for Creation of Entities (POST), Read of Entities (GET), Update of Entities (PATCH), Deletion of Entities (DELETE), running Actions (POST), receiving events, and providing some basic query abilities.

License:	BSD-3
URL:		https://github.com/DMTF/libredfish
Source0:	https://github.com/DMTF/libredfish/archive/%{version}.tar.gz

BuildRequires:  cmake
BuildRequires:  libcurl-devel
BuildRequires:  jansson-devel
BuildRequires:  readline-devel
Requires:       libcurl 
Requires:       jansson
Requires:       readline

%description

%package        devel
Summary:        Headers for building apps that use libredfish
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}

%description    devel
This package contains headers required to build applications that use libredfish.

%prep
%autosetup

%build
%cmake .
make

%install
install -d -m755 $RPM_BUILD_ROOT%{_bindir}
install -d -m755 $RPM_BUILD_ROOT%{_libdir}
install -d -m755 $RPM_BUILD_ROOT%{_includedir}/%{name}/entities
install -m755 bin/redfishtest $RPM_BUILD_ROOT%{_bindir}/
install -m755 lib/libredfish.so $RPM_BUILD_ROOT%{_libdir}/
install include/*.h $RPM_BUILD_ROOT%{_includedir}/%{name}/
install include/entities/* $RPM_BUILD_ROOT%{_includedir}/%{name}/entities/

%files
%{_bindir}/*
%{_libdir}/*

%files devel
%defattr(-,root,root,-)
%{_includedir}/%{name}

%changelog



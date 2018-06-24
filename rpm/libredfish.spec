Name:		libredfish
Version:	1.0.2
Release:	1%{?dist}
Summary:	libRedfish is a C client library that allows for Creation of Entities (POST), Read of Entities (GET), Update of Entities (PATCH), Deletion of Entities (DELETE), running Actions (POST), receiving events, and providing some basic query abilities.

License:	BSD-3
URL:		https://github.com/DMTF/libredfish
Source0:	https://github.com/DMTF/libredfish/archive/1.0.2.tar.gz

BuildRequires:  cmake
BuildRequires:  libcurl-devel
BuildRequires:  jansson-devel
Requires:       libcurl 
Requires:       jansson

%description

%prep
%autosetup

%build
%cmake .
make

%install
install -d -m755 $RPM_BUILD_ROOT%{_bindir}
install -d -m755 $RPM_BUILD_ROOT%{_libdir}
install -m755 bin/redfishtest $RPM_BUILD_ROOT%{_bindir}/
install -m755 lib/libredfish.so $RPM_BUILD_ROOT%{_libdir}/

%files
%{_bindir}/*
%{_libdir}/*

%changelog



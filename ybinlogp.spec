%define name ybinlogp
%define version 0.6
%define release 1

Summary: Library, program, and python bindings for parsing MySQL binlogs
Name: %{name}
Version: %{version}
Release: %{release}
Source0: %{name}-%{version}.tar.gz
License: BSD
Group: Development/Libraries
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Prefix: %{_prefix}
BuildArch: x86_64
Vendor: Yelp <yelplabs@yelp.com>

%description
Library, program, and python bindings for parsing MySQL binlogs

%prep
%setup

%build
make

%install
make install

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/include/ybinlogp.h
/usr/sbin/ybinlogp
/usr/lib64/libybinlogp.so.1
/usr/lib64/libybinlogp.so
/usr/lib64/libybinlogp.la
/usr/lib64/python2.6/site-packages/ybinlogp
%defattr(-,root,root)

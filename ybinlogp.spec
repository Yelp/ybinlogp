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
install -D -m 444 src/ybinlogp.h $RPM_BUILD_ROOT/usr/include/ybinlogp.h
install -D -m 755 build/ybinlogp $RPM_BUILD_ROOT/usr/sbin/ybinlogp
install -D -m 555 build/libybinlogp.so.1 $RPM_BUILD_ROOT/usr/lib64/libybinlogp.so.1
install -D -m 555 build/libybinlogp.so $RPM_BUILD_ROOT/usr/lib64/libybinlogp.so
install -D -d src/ybinlogp $RPM_BUILD_ROOT/usr/lib64/python2.6/site-packages/ybinlogp

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/include/ybinlogp.h
/usr/sbin/ybinlogp
/usr/lib64/libybinlogp.so.1
/usr/lib64/libybinlogp.so
/usr/lib64/python2.6/site-packages/ybinlogp
%defattr(-,root,root)

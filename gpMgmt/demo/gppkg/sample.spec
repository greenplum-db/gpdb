Name:       sample
Version:    0.0.1
Release:    noop
Summary:    template rpm
License:    apache 
Source0:    sample-sources.tar.gz
BuildArch:  x86_64
# Prefix must be specified as gppkg runs rpm install with --prefix flag
# This means the rpm was built with an overridable prefix
Prefix:     /
# We have to claim to provide /bin/sh as the rpm installation will fail
Provides: /bin/sh


%description
template rpm generation

%prep
%setup -c -q -T -D -a 0

%build

%install
cd $RPM_BUILD_ROOT
pwd
mkdir -p bin include lib share/postgresql
touch bin/sample
touch lib/sample.so
touch include/sample.h
touch share/postgresql/sample.sql

%files
/bin/sample
/lib/sample.so
/include/sample.h
/share/postgresql/sample.sql

%changelog

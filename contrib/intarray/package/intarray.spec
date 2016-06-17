Summary:        Functions and GiST index support for integer arrays
License:        PostgreSQL License
Name:           intarray
Version:        %{intarray_ver}
Release:        %{intarray_rel}
Group:          Development/Tools
Prefix:         /temp
AutoReq:        no
AutoProv:       no
Provides:       intarray = %{intarray_ver} 

%description
The intarray package provides integer array functions and GiST index support for Greenplum Database

%install
mkdir -p %{buildroot}/temp
make -C %{intarray_dir} install prefix=%{buildroot}/temp

%files
/temp

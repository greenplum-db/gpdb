## Setting up Conan

Conan requires python 2.7 or higher to run.  To install Conan run the command:

```
	pip install conan
```

This will install all the required python packages to use Conan on your system.  Once Conan is installed, please add the Greenplum remote Conan repository.  This is where we are hosting the packages and the pre-build binaries for GPDB and associated projects.  Anonymous access is supported for package downloading and to configure access please run the command:

```
	conan remote add <REMOTE_NAME> https://api.bintray.com/conan/greenplum-db/gpdb-oss 
```


## Using Conan

The simplest way to run Conan is to run the command `conan install --build=missing` from this directory.  It will read the conanfile.txt and copy the dependant files into the directory structure rooted here.

Some packages may have pre-built binaries for specific operating system (e.g ORCA 2.32.0 has binaries built for Linux).  If you are trying to install on a system and run into problems with `conan install` and see an error similar too:

	WARN: Can't find a 'gpxerces/v3.1.2-p1@gpdb/stable' package for the specified options and settings:
	- Settings: arch=x86_64, build_type=Release, compiler=apple-clang, compiler.libcxx=libc++, compiler.version=8.1, os=Macos
	- Options: shared=True

	ERROR: Missing prebuilt package for 'gpxerces/v3.1.2-p1@gpdb/stable'
	Try to build it from sources with "--build gpxerces"
	Or read "http://docs.conan.io/en/latest/faq/troubleshooting.html#error-missing-prebuilt-package"

This may be because the binaries do not exist for your system and the package is not configured to build automatically if it is missing (add the `--build=missing` flag).

Conan will **not** install files into standard locations such as `/usr/local` nor will it interaction with autoconf or GNU make in any way at this time.  You will need to either install these files into an appropriate location in your files system such as `/usr/local/lib` or else update the `CFLAGS` variable to pass to configure:

```	
	CFLAGS="-I./depends/include" LDFLAGS="-L./depends/lib" ./configure
```

Afterwards you will need to either install the libraries in a standard location or update the `LD_LIBARARY_PATH` (or similar) so the gpdb binaries can find the libraries


## Contributing

We are tracking the package definitions in a separate [Conan github repository](http://github.com/greenplum-db/conan).  Instructions on how you can contribute and update dependencies can be found there.
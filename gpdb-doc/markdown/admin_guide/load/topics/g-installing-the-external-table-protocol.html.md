---
title: Installing the External Table Protocol 
---

To use the example external table protocol, you use the C compiler `cc` to compile and link the source code to create a shared object that can be dynamically loaded by Greenplum Database. The commands to compile and link the source code on a Linux system are similar to this:

`cc -fpic -c gpextprotocal.c cc -shared -o gpextprotocal.so gpextprotocal.o`

The option `-fpic` specifies creating position-independent code \(PIC\) and the `-c` option compiles the source code without linking and creates an object file. The object file needs to be created as position-independent code \(PIC\) so that it can be loaded at any arbitrary location in memory by Greenplum Database.

The flag `-shared` specifies creating a shared object \(shared library\) and the `-o` option specifies the shared object file name `gpextprotocal.so`. Refer to the GCC manual for more information on the `cc` options.

The header files that are declared as include files in `gpextprotocal.c` are located in subdirectories of `$GPHOME/include/postgresql/.`

For more information on compiling and linking dynamically-loaded functions and examples of compiling C source code to create a shared library on other operating systems, see the PostgreSQL documentation at [https://www.postgresql.org/docs/9.4/xfunc-c.html\#DFUNC](https://www.postgresql.org/docs/9.4/xfunc-c.html#DFUNC).

The manual pages for the C compiler `cc` and the link editor `ld` for your operating system also contain information on compiling and linking source code on your system.

The compiled code \(shared object file\) for the custom protocol must be placed in the same location on every host in your Greenplum Database array \(master and all segments\). This location must also be in the `LD_LIBRARY_PATH` so that the server can locate the files. It is recommended to locate shared libraries either relative to `$libdir` \(which is located at `$GPHOME/lib`\) or through the dynamic library path \(set by the `dynamic_library_path` server configuration parameter\) on all master segment instances in the Greenplum Database array. You can use the Greenplum Database utilities gpssh and `gpscp` to update segments.

-   **[gpextprotocal.c](../../load/topics/g-gpextprotocal.c.html)**  


**Parent topic:**[Example Custom Data Access Protocol](../../load/topics/g-example-custom-data-access-protocol.html)


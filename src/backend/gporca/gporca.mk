# GPDB_12_MERGE_FIXME: I had to add these to make "make install" work
# with --with--lvm. I don't undertand what's going on, but I found this
# cure at https://github.com/rdkit/rdkit/issues/2192. Seems to be the
# same issue, whatever it is.
COMPILE.cxx.bc = $(CLANG) -xc++ -Wno-ignored-attributes $(BITCODE_CXXFLAGS) $(CPPFLAGS) -emit-llvm -c
%.bc : %.cpp
	$(COMPILE.cxx.bc) -o $@ $<
	$(LLVM_BINPATH)/opt -module-summary -f $@ -o $@

override CPPFLAGS := -I$(top_srcdir)/src/backend/gporca/libgpos/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_srcdir)/src/backend/gporca/libgpopt/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_srcdir)/src/backend/gporca/libnaucrates/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_srcdir)/src/backend/gporca/libgpdbcost/include $(CPPFLAGS)
# Do not omit frame pointer. Even with RELEASE builds, it is used for
# backtracing.
override CXXFLAGS := -Werror -Wextra -Wpedantic -fno-omit-frame-pointer $(CXXFLAGS)

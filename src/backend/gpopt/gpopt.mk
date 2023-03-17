override CPPFLAGS := -I$(top_srcdir)/src/backend/gporca/libgpos/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_srcdir)/src/backend/gporca/libgpopt/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_srcdir)/src/backend/gporca/libnaucrates/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_srcdir)/src/backend/gporca/libgpdbcost/include $(CPPFLAGS)

# orca is not accessed in JIT (executor stage), avoid the generation of .bc here
# NOTE: accordingly we MUST avoid them in install step (install-postgres-bitcode
# in src/backend/Makefile)
with_llvm = no

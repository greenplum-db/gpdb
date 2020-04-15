override CPPFLAGS := -I$(top_builddir)/src/backend/gporca/libgpos/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_builddir)/src/backend/gporca/libgpopt/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_builddir)/src/backend/gporca/libnaucrates/include $(CPPFLAGS)
override CPPFLAGS := -I$(top_builddir)/src/backend/gporca/libgpdbcost/include $(CPPFLAGS)
# Do not omit frame pointer. Even with RELEASE builds, it is used for
# backtracing.
override CXXFLAGS := -Werror -Wextra -Wpedantic -Wno-variadic-macros -fno-omit-frame-pointer $(CXXFLAGS)
# FIXME: this really should be done in autoconf
override CXXFLAGS := -std=gnu++14 $(CXXFLAGS)

top_builddir=../..
ifneq "$(wildcard $(top_builddir)/src/Makefile.global)" ""
include $(top_builddir)/src/Makefile.global
endif

install:
	@# Copy everything except the makefile
	find . -not -path '*Makefile' -type f -exec cp -- {} $(prefix)/sbin/{} \;


clean distclean:
	@rm -f *.pyc

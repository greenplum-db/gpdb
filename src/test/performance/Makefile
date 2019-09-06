subdir = src/test/performance
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

# where to find psql for testing an existing installation
PSQLDIR = $(bindir)

# how many times to insert dataset file into the base table
NUM_COPIES ?= 10000
GPFDIST_PORT ?= 9001

pg_regress.o:
	$(MAKE) -C $(top_builddir)/src/test/regress

perf-setup:
	./gen_and_host_data.sh $(NUM_COPIES) $(GPFDIST_PORT)

perf-ao-load: pg_regress.o perf-setup
	$(top_builddir)/src/test/regress/pg_regress --init-file=$(top_builddir)/src/test/regress/init_file --inputdir=$(srcdir) --schedule=$(srcdir)/performance_load_schedule | tee perf_results.out

	# Parse the results.out into as a CSV for loading into a results table or spreadsheet
	python parse_perf_results.py perf_results.out $(NUM_COPIES)

	# Make sure we kill the gpfdist process we brought up
	killall gpfdist

clean:
	rm -rf results $(MASTER_DATA_DIRECTORY)/perfdataset
	rm -f perf_results.* expected/setup.out sql/setup.sql

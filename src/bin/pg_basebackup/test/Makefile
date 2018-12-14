cmockery_dir = ../../../test/unit/cmockery
libpgfe_dir = '../../../interfaces/libpq'
postgres_dir = '../../../include'

replication_slot:
	cd ..; make replication_slot

replication_slots_test: replication_slots_test.c replication_slot
	$(CC) replication_slots_test.c -I $(cmockery_dir) -I $(libpgfe_dir) \
		-I $(postgres_dir) -L $(libpgfe_dir) $(cmockery_dir)/cmockery.o \
		../replicationslot.o -o replication_slots_test.o

check: replication_slots_test
	./replication_slots_test.o

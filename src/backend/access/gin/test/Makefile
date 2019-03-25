root_dir=../../../../..
cmockery_dir=$(root_dir)/src/test/unit/cmockery
gpdb_include_dir=$(root_dir)/src/include/
gin_dir=$(root_dir)/src/backend/access/gin


check-unittest:
	make -C $(gin_dir);
	gcc -g varbyte_test.c \
		$(cmockery_dir)/cmockery.c \
		-I $(cmockery_dir) \
		-I $(gpdb_include_dir) \
		$(gin_dir)/ginpostinglist.o \
		-o varbyte_test.o
	./varbyte_test.o

root_dir=../../../../..
cmockery_dir=$(root_dir)/src/test/unit/cmockery
gpdb_include_dir=$(root_dir)/src/include/
gin_dir=$(root_dir)/src/backend/access/gin


clean:
	rm -f ginpostinglist_test.o


install:
	make -C $(gin_dir);
	gcc -g ginpostinglist_test.c \
		debugger.c \
		ginpostinglist_fakes.c \
		$(cmockery_dir)/cmockery.c \
		-Wall \
		-I $(cmockery_dir) \
		-I $(gpdb_include_dir) \
		-I $(gin_dir)/test/ \
		$(gin_dir)/ginpostinglist.o \
		-o ginpostinglist_test.o


unittest-check: install
	./ginpostinglist_test.o


check: unittest-check


debug: install
	gdb --eval-command='run' ./ginpostinglist_test.o

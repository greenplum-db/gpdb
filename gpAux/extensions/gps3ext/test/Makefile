# Include
include ../include/makefile.inc

# Options
ARCH = $(shell uname -s)

# Flags
CPP = g++
INCLUDES = -I../src -I../include -I../lib
LDFLAGS = $(COMMON_LINK_OPTIONS)
CPPFLAGS = $(COMMON_CPP_FLAGS) -g3 -DS3_STANDALONE -fprofile-arcs -ftest-coverage

ifeq "$(ARCH)" "Darwin"
	LDFLAGS += -coverage
else
	LDFLAGS += -lgcov
endif

all: test

# Google TEST
TEST_SRC = $(TEST_OBJS:.o=.cpp)
TEST_APP = s3test
gtest_filter ?= *

DEP_FILES := $(patsubst %.o,%.d,$(TEST_OBJS))
TEST_SRC = $(TEST_OBJS:.o=.cpp)
-include $(DEP_FILES)

GMOCK_DIR = ../../gmock
GTEST_DIR = ../../gtest

$(TEST_OBJS) gtest_main.o gtest-all.o gmock-all.o: INCLUDES += -I$(GTEST_DIR)/ -I$(GMOCK_DIR)/ -I$(GTEST_DIR)/src -I$(GMOCK_DIR)/src -I$(GTEST_DIR)/include -I$(GMOCK_DIR)/include

gmock-all.o :
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $(GMOCK_DIR)/src/gmock-all.cc

gtest-all.o :
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $(GTEST_DIR)/src/gtest-all.cc

gtest_main.o :
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $(GTEST_DIR)/src/gtest_main.cc

gtest_main.a : gtest_main.o gtest-all.o gmock-all.o
	$(AR) -rv $@ $^

buildtest: $(TEST_APP)

# Keep gtest_main.a at end, otherwise linker will report undefined symbol error.
$(TEST_APP): $(TEST_OBJS) gtest_main.a
	$(CPP) $^ -o $(TEST_APP) $(LDFLAGS)

%.o: %.cpp
	$(CPP) $(CPPFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

test: $(TEST_APP)
	@-rm -f *.gcda test/*.gcda # workaround for XCode/Clang
	@-./$(TEST_APP) --gtest_filter=$(gtest_filter)

coverage: test
	@gcov $(TEST_SRC) | grep -A 1 "src/.*.cpp"

clean:
	rm -f *.o *.d *.a *.gcov *.gcda *.gcno $(TEST_APP)

.PHONY: buildtest test coverage clean

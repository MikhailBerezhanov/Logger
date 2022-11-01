# Using compilers from environment settings
#CXX=g++
#CC=gcc
CFLAGS=-Wall -O2

# Paths
SRC_DIR = .
INCLUDES = -I$(SRC_DIR)
C_DIR = $(SRC_DIR)/c_src
CPP_DIR = $(SRC_DIR)/cpp_src

TESTS_DIR=./tests

C_TEST_BIN=$(TESTS_DIR)/logger-c.test
CPP_TEST_BIN=$(TESTS_DIR)/logger-cpp.test

.PHONY : clean

all: clean prep logger-cpp

# Prepare directories for output
prep:
	@if test ! -d $(TESTS_DIR); then mkdir $(TESTS_DIR); fi

logger-cpp: prep
	@$(CXX) $(CFLAGS) $(CPP_DIR)/logger.cpp -D_LOGGER_TEST -o $(CPP_TEST_BIN) -lpthread

logger-c: prep
	@$(CC) $(CFLAGS) $(C_DIR)/logger.c -D_LOGGER_TEST -o $(C_TEST_BIN)

clean:
	@rm -rf $(TESTS_DIR)
	@rm -f *.log*

mem_check-cpp:	OUTPUT_FILE = $(TESTS_DIR)/valgrind-out.txt

mem_check-cpp: logger-cpp
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=$(OUTPUT_FILE) $(CPP_TEST_BIN)
	@cat $(OUTPUT_FILE)
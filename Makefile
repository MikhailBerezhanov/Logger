CCPP=g++
CC=gcc
CFLAGS=-Wall -Wno-format-security -Wno-unused-variable -Wno-unused-function

# PATHS
SRC_DIR = .
INCLUDES = -I$(SRC_DIR)
C_DIR = $(SRC_DIR)/c_src
CPP_DIR = $(SRC_DIR)/cpp_src

LLIBS=-lpthread

TARGET_BIN_DIR = ./bin
TARGET_OBJ_DIR = ./obj
TESTS_DIR=./tests

C_TEST_BIN=$(TESTS_DIR)/logger-c.test
CPP_TEST_BIN=$(TESTS_DIR)/logger-cpp.test

.PHONY : clean

all: clean prep logger-cpp

# Prepare directories for output
prep:
	@if test ! -d $(TESTS_DIR); then mkdir $(TESTS_DIR); fi

logger-cpp: prep
	@$(CCPP) $(CPP_DIR)/logger.cpp -D_UNIT_TEST -o $(CPP_TEST_BIN) -lpthread

logger-c: prep
	@$(CC) $(C_DIR)/logger.c -D_UNIT_TEST -D_GNU_SOURCE=1 -o $(C_TEST_BIN)

clean:
	@rm -f $(C_TEST_BIN) $(CPP_TEST_BIN)

mem_check-cpp:	OUTPUT_FILE = $(TESTS_DIR)/valgrind-out.txt

mem_check-cpp: logger-cpp
	@valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=$(OUTPUT_FILE) $(CPP_TEST_BIN)
	@cat $(OUTPUT_FILE)
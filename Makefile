CCPP=g++
CC=gcc
CFLAGS=-Wall -Wno-format-security -Wno-unused-variable -Wno-unused-function

# PATHS
SRC_DIR = .
INCLUDES = -I$(SRC_DIR)
C_DIR = $(SRC_DIR)/c_api
CPP_DIR = $(SRC_DIR)/cpp_api

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

logger-cpp:
	@$(CCPP) $(CPP_DIR)/logger.cpp -D_UNIT_TEST -o $(CPP_TEST_BIN) -lpthread

logger-c:
	@$(CC) $(C_DIR)/logger.c -D_UNIT_TEST -D_GNU_SOURCE=1 -o $(C_TEST_BIN)

clean:
	@rm -f $(C_TEST_BIN) $(CPP_TEST_BIN)
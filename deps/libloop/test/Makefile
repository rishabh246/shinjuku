CI_ROOT := /home/m8/shinjuku-develop/compiler-interrupt
CI_LIB_HOME := $(CI_ROOT)/lib

CC = gcc
CC_FLAGS = ../ci_loop_lib.a $(CI_LIB_HOME)/libci.a -I$(CI_ROOT)/src/ -I../

all: clean build

build:
	$(CC) -o test test.c $(CC_FLAGS)

clean:
	$(RM) test
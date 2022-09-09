ifneq ($(shell command -v llvm-config-9 2> /dev/null),)
  VERSION=-9
else ifneq ($(shell command -v llvm-config-12 2> /dev/null),)
  VERSION=-12
else ifneq ($(shell command -v llvm-config-11 2> /dev/null),)
  VERSION=-11
else ifneq ($(shell command -v llvm-config-10 2> /dev/null),)
  VERSION=-10
else ifneq ($(shell command -v llvm-config 2> /dev/null),)
  VERSION=
else
  $(error Unable to locate the LLVM toolchain for supported versions 9-12)
endif

LLVM_CONFIG = llvm-config$(VERSION)
LLVM_LINK = llvm-link$(VERSION)
OPT = opt$(VERSION)

ifneq ($(shell command -v clang$(VERSION) 2> /dev/null),)
  CC = clang++$(VERSION)
else
  $(error Please install clang$(VERSION))
endif

ifneq ($(shell command -v clang-format 2> /dev/null),)
  CLANG_FORMAT = clang-format
else
  $(error Please install clang-format)
endif

LLVM_VERSION := $(shell $(LLVM_CONFIG) --version | cut -d '.' -f 1)

ifeq ($(shell uname), Darwin)
  LOADABLE_MODULE_OPTIONS = -bundle -undefined dynamic_lookup
else
  LOADABLE_MODULE_OPTIONS = -shared -O1
endif

ifndef VERBOSE
  QUIET := @
endif

CI_ROOT := $(shell pwd)/../../compiler-interrupt
CI_LIB_HOME := $(CI_ROOT)/lib
CI_PASS := $(CI_LIB_HOME)/CompilerInterrupt.so
CI_LIB := $(CI_LIB_HOME)/libci.so
INC := -I$(CI_ROOT)/src

CI_CONFIG = -load $(CI_PASS) -logicalclock -clock-type 0 -push-intv 5 -commit-intv 5 -inst-gran 19
DEPENDENCY_CONFIG = -postdomtree -mem2reg -indvars -loop-simplify -branch-prob -scalar-evolution

CC := clang-9
CC_FLAGS := -emit-llvm -g -S -O3 -I. -c 


all: clean
	$(CC) $(CC_FLAGS) $(CI_INC) loop.c -o loop_lib.ll
	$(OPT) -S $(DEPENDENCY_CONFIG) < loop_lib.ll > loop_lib.opt.ll
	$(CC)  -g -c loop_lib.opt.ll -O3 -o loop_lib.a
	$(OPT) -S $(CI_CONFIG) < loop_lib.opt.ll > ci_loop_lib.opt.ll
	$(CC) $(CI_INC) -g -c ci_loop_lib.opt.ll -O3 -o ci_loop_lib.a

# clean
clean:
	$(RM) ci_loop_lib.a loop_lib.bc loop_lib.ll loop_lib.opt.ll ci_loop_lib.opt.ll loop_lib.a

# Quick & dirty makefile for test "suite"
#

# DIRECTORIES & CONFIG
OC_SRC_DIR = ../o_c_REV/
BUILD_DIR = ./build/

DEFINES = TESTING

RM    = rm -f
RMDIR = rmdir
MKDIR = mkdir -p
CXX   = g++
LD    = g++
AR    = ar -r

CPPFLAGS += -I$(OC_SRC_DIR) -I$(GTEST_DIR)include -Wall -Werror -std=c++11

CPPFLAGS += $(addprefix -D, $(DEFINES))

# GTEST
GTEST_DIR = ./gtest/googletest/
LIBGTEST = $(BUILD_DIR)libgtest.a

# SOURCE FILES
OC_CPP_FILES = $(OC_SRC_DIR)braids_quantizer.cpp \
	$(OC_SRC_DIR)/src/util/util_settings.cpp

VPATH = . $(OC_SRC_DIR) $(OC_SRC_DIR)/src/util
CPP_FILES = $(notdir $(wildcard *.cpp)) $(notdir $(OC_CPP_FILES))
OBJ_FILES = $(CPP_FILES:.cpp=.o)
OBJS      = $(patsubst %,$(BUILD_DIR)%,$(OBJ_FILES))

EXE = $(BUILD_DIR)oc_tests

# COMPILER RULES
$(BUILD_DIR)%.o: %.cpp
	$(CXX) -c $(CCFLAGS) $(CPPFLAGS) $< -o $@

# TARGETS
.PHONY: all
all: runtests

.PHONY: runtests
runtests: $(EXE)
	@$(EXE)

$(EXE): $(BUILD_DIR) $(LIBGTEST) $(OBJS)
	@echo "Linking $(EXE)..."
	@$(LD) $(LDFLAGS) -o $(EXE) $(OBJS) $(LIBGTEST)

$(BUILD_DIR):
	@$(MKDIR) $(BUILD_DIR)

$(LIBGTEST): $(BUILD_DIR)
	@$(CXX) -isystem $(GTEST_DIR)include -I$(GTEST_DIR) -pthread -c $(GTEST_DIR)src/gtest-all.cc -o $(BUILD_DIR)gtest-all.o
	@$(AR) $(LIBGTEST) $(BUILD_DIR)gtest-all.o

.PHONY: clean
clean:
	@$(RM) $(LIBGTEST) $(OBJS) $(EXE)

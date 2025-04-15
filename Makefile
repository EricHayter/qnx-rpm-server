ARTIFACT = qnx-rpm-server

#Build architecture/variant string, possible values: x86, armv7le, etc...
PLATFORM ?= x86_64

#Build profile, possible values: release, debug, profile, coverage
BUILD_PROFILE ?= debug

CONFIG_NAME ?= $(PLATFORM)-$(BUILD_PROFILE)
OUTPUT_DIR = build/$(CONFIG_NAME)
TARGET = $(OUTPUT_DIR)/$(ARTIFACT)

#Compiler definitions
CC = qcc -Vgcc_nto$(PLATFORM)
CXX = q++ -Vgcc_nto$(PLATFORM)_cxx
LD = $(CXX)

#User defined include/preprocessor flags and libraries
INCLUDES += -Iinclude
INCLUDES += -I.

#LIBS += -L/path/to/my/lib/$(PLATFORM)/usr/lib -lmylib
LIBS += -lsocket
LIBS += -llogin
LIBS += -ljson

#Compiler flags for build profiles
CCFLAGS_release += -O2
CCFLAGS_debug += -g -O0 -fno-builtin
CCFLAGS_coverage += -g -O0 -ftest-coverage -fprofile-arcs
LDFLAGS_coverage += -ftest-coverage -fprofile-arcs
CCFLAGS_profile += -g -O0 -finstrument-functions
LIBS_profile += -lprofilingS

#Generic compiler flags (which include build type flags)
CCFLAGS_all += -Wall -fmessage-length=0 -std=c++17
CCFLAGS_all += $(CCFLAGS_$(BUILD_PROFILE))

#Shared library has to be compiled with -fPIC
#CCFLAGS_all += -fPIC
LDFLAGS_all += $(LDFLAGS_$(BUILD_PROFILE))
LIBS_all += $(LIBS_$(BUILD_PROFILE))
DEPS = -Wp,-MMD,$(@:%.o=%.d),-MT,$@

#Source files
CPP_SRCS = $(wildcard src/*.cpp)
C_SRCS   = $(wildcard src/*.c) # Keep in case any C files remain or are added

#Object files
OBJS = $(addprefix $(OUTPUT_DIR)/,$(notdir $(CPP_SRCS:.cpp=.o))) \
       $(addprefix $(OUTPUT_DIR)/,$(notdir $(C_SRCS:.c=.o)))

#Compiling rules
$(OUTPUT_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(DEPS) -o $@ $(INCLUDES) $(CCFLAGS_all) $(CCFLAGS) $<

$(OUTPUT_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) -c $(DEPS) -o $@ $(INCLUDES) $(CCFLAGS_all) $(CCFLAGS) $<

#Linking rule
$(TARGET): $(OBJS)
	$(LD) -o $(TARGET) $(LDFLAGS_all) $(LDFLAGS) $(OBJS) $(LIBS_all) $(LIBS)

#Rules section for default compilation and linking
all: $(TARGET)

clean:
	rm -fr $(OUTPUT_DIR)

rebuild: clean all

#Inclusion of dependencies (object files to source and includes)
-include $(OBJS:%.o=%.d)

ARTIFACT = qnx-rpm-server

#Build architecture/variant string, possible values: x86, armv7le, etc...
PLATFORM ?= x86_64

#Build profile, possible values: release, debug, profile, coverage
BUILD_PROFILE ?= debug

CONFIG_NAME ?= $(PLATFORM)-$(BUILD_PROFILE)
OUTPUT_DIR = build/$(CONFIG_NAME)
SRC_DIR = src
INCLUDE_DIR = include
TARGET = $(OUTPUT_DIR)/$(ARTIFACT)

#Compiler definitions
CXX = q++ -Vgcc_nto$(PLATFORM)_cxx
LD = $(CXX)

#User defined include/preprocessor flags and libraries
INCLUDES += -I$(INCLUDE_DIR)

#LIBS += -L/path/to/my/lib/$(PLATFORM)/usr/lib -lmylib
LIBS += -lsocket -llogin -ljson -lstdc++

#Compiler flags for build profiles
CXXFLAGS_release += -O2 -Werror
CXXFLAGS_debug += -g -O0 -fno-builtin
CXXFLAGS_coverage += -g -O0 -ftest-coverage -fprofile-arcs
LDFLAGS_coverage += -ftest-coverage -fprofile-arcs
CXXFLAGS_profile += -g -O0 -finstrument-functions
LIBS_profile += -lprofilingS

#Generic compiler flags (which include build type flags)
CXXFLAGS_all += -Wall -fmessage-length=0 -std=c++17
CXXFLAGS_all += $(CXXFLAGS_$(BUILD_PROFILE))

#Shared library has to be compiled with -fPIC
#CXXFLAGS_all += -fPIC
LDFLAGS_all += $(LDFLAGS_$(BUILD_PROFILE))
LIBS_all += $(LIBS_$(BUILD_PROFILE))
DEPS = -Wp,-MMD,$(@:%.o=%.d),-MT,$@

#Source files
SERVER_SRCS = $(addprefix server/, JsonHandler.cpp main.cpp \
			  ProcessControl.cpp ProcessCore.cpp ProcessGroup.cpp \
			  ProcessHistory.cpp SocketServer.cpp)
SERVER_OBJS = $(addprefix $(OUTPUT_DIR)/, $(SERVER_SRCS:.cpp=.o))

SHARED_SRCS = $(addprefix shared/, Authenticator.cpp)
SHARED_OBJS = $(addprefix $(OUTPUT_DIR)/, $(SHARED_SRCS:.cpp=.o))


#Compiling rules
$(OUTPUT_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(DEPS) -o $@ $(INCLUDES) $(CXXFLAGS_all) $(CXXFLAGS) $<

#Linking rule
$(TARGET): $(SERVER_OBJS) $(SHARED_OBJS)
	$(LD) -o $(TARGET) $(LDFLAGS_all) $(LDFLAGS) $(SHARED_OBJS) $(SERVER_OBJS) $(LIBS_all) $(LIBS)

#Rules section for default compilation and linking
all: $(TARGET) ## Build the main target artifact

# Format all C++ and header files using clang-format
.PHONY: format
format: ## Format source code using clang-format
	@echo "Running clang-format on source files..."
	@clang-format -i $(wildcard $(SRC_DIR)/*.cpp) $(wildcard $(INCLUDE_DIR)/*.hpp)

clean: ## Remove build artifacts
	rm -fr $(OUTPUT_DIR)

rebuild: clean all ## Clean and rebuild the target

.PHONY: upload run help
upload: $(TARGET) ## Upload the target artifact to the QNX device (root@192.168.153.137)
	@echo "Uploading $(TARGET) to root@192.168.153.137:~"
	@sshpass -p "root" scp $(TARGET) root@192.168.153.137:~

run: ## Run the target artifact on the QNX device (root@192.168.153.137)
	@echo "Running $(ARTIFACT) on root@192.168.153.137"
	@sshpass -p "root" ssh root@192.168.153.137 "/root/$(ARTIFACT)"

help: ## Display this help message
	@echo "Available targets:"
	@awk -F ':|##' '/^[a-zA-Z0-9_-]+:.*##/ { printf "  %-20s %s\n", $$1, $$3 }' $(MAKEFILE_LIST) | sort

#Inclusion of dependencies (object files to source and includes)
-include $(OBJS:%.o=%.d)

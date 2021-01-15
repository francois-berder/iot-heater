BUILDTYPE ?= release

BUILDDIR := build
OBJDIR := $(BUILDDIR)/$(BUILDTYPE)/obj
DEPDIR := $(BUILDDIR)/$(BUILDTYPE)/dep
BINDIR := $(BUILDDIR)/$(BUILDTYPE)/bin

CFLAGS += -Wall -Wextra -std=c++11
CFLAGS += -I src
LDFLAGS += -lmicrohttpd
DEPFLAGS = -MMD -MP -MF $(@:$(OBJDIR)/%.o=$(DEPDIR)/%.d)

ifeq ($(BUILDTYPE),release)
CFLAGS += -O2
LDFLAGS += -O2
else ifeq ($(BUILDTYPE),debug)
CFLAGS += -ggdb -g3 -gz
LDFLAGS += -ggdb -g3 -gz
endif

SRCS := src/device_server.cpp \
		src/base_station.cpp \
		src/logger.cpp \
		src/main.cpp \
		src/sms_sender.cpp \
		src/sms_receiver.cpp \
		src/timer.cpp \
		src/web_server.cpp
OBJS := $(SRCS:%.cpp=$(OBJDIR)/%.o)
DEPS := $(SRCS:%.cpp=$(DEPDIR)/%.d)

LDFLAGS += -lpthread

TARGET := base_station-$(BUILDTYPE)

# Build information
GIT_HASH := $(shell git rev-parse HEAD | head -c 12)$(shell git diff --quiet || echo '-dirty')
BUILD_TIME := $(shell date +%F-%T)
CFLAGS += -DGIT_HASH=\"$(GIT_HASH)\" -DBUILD_TIME=\"$(BUILD_TIME)\"

.PHONY: all
all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJS)
	mkdir -p $(@D)
	$(CXX) $^ $(LDFLAGS) -o $@

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@mkdir -p $(DEPDIR)/$(<D)
	$(CXX) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILDDIR)/$(BUILDTYPE)

.PHONY: distclean
distclean:
	rm -rf $(BUILDDIR)

-include $(DEPS)

BUILDTYPE ?= release

BUILDDIR := build
OBJDIR := $(BUILDDIR)/$(BUILDTYPE)/obj
DEPDIR := $(BUILDDIR)/$(BUILDTYPE)/dep
BINDIR := $(BUILDDIR)/$(BUILDTYPE)/bin

CFLAGS += -Wall -Wextra -std=c++11
CFLAGS += -I src
DEPFLAGS = -MMD -MP -MF $(@:$(OBJDIR)/%.o=$(DEPDIR)/%.d)

ifeq ($(BUILDTYPE),release)
CFLAGS += -O2
LDFLAGS += -O2
else ifeq ($(BUILDTYPE),debug)
CFLAGS += -ggdb -g3 -gz
LDFLAGS += -ggdb -g3 -gz
endif

SRCS := src/device.cpp src/device_feature.cpp src/device_manager.cpp src/device_server.cpp \
		src/heater_feature.cpp src/logger.cpp src/main.cpp
OBJS := $(SRCS:%.cpp=$(OBJDIR)/%.o)
DEPS := $(SRCS:%.cpp=$(DEPDIR)/%.d)

LDFLAGS += -lpthread

TARGET := homegateway-$(BUILDTYPE)

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

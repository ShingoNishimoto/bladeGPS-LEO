# Makefile for Linux etc.

.PHONY: all lib clean install FORCE
all: bladegps

SHELL=/bin/bash
CC=gcc

# libbladeRF from pkg-config by default; override e.g.
#   make BLADERF_CFLAGS=-I<bladeRF>/host/libraries/libbladeRF/include BLADERF_LIBS="-L<bladeRF>/host/build/output -lbladeRF"
BLADERF_CFLAGS ?= $(shell pkg-config --cflags libbladeRF 2>/dev/null)
BLADERF_LIBS ?= $(shell pkg-config --libs libbladeRF 2>/dev/null || echo -lbladeRF)

# Default location of gps_table.csv and antenna/ (overridden by -D option or $BLADEGPS_DATA_DIR)
GNSS_DATA_DIR ?= $(CURDIR)/gnss

GIT_COMMIT_ID := $(shell git rev-parse --short=8 HEAD 2>/dev/null || echo unknown)
GIT_COMMIT_DATE := $(shell git log -1 --format=%cd 2>/dev/null)

# -MMD -MP: rebuild objects when a header changes
CFLAGS += -O3 -Wall -fPIC -MMD -MP $(BLADERF_CFLAGS)
CFLAGS += -DGNSS_DATA_DIR=\"$(GNSS_DATA_DIR)\"
CFLAGS += -DGIT_COMMIT_ID=\"$(GIT_COMMIT_ID)\" -DGIT_COMMIT_DATE="\"$(GIT_COMMIT_DATE)\""
LDLIBS = $(BLADERF_LIBS) -lm -lpthread

OBJS = bladegps.o gpssim.o gpssatellite.o eci.o

# Rebuild when the build settings (data directory, commit, flags) change
BUILD_FLAGS = $(CC) $(CFLAGS)
.build_flags: FORCE
	@echo '$(BUILD_FLAGS)' | cmp -s - $@ || echo '$(BUILD_FLAGS)' > $@
main.o $(OBJS): .build_flags

bladegps: main.o $(OBJS)
	$(CC) $^ $(LDLIBS) -o $@

# Shared library exposing bladegps_main() for embedding (e.g. gnss_sim_receiver)
lib: libbladegps.so
libbladegps.so: $(OBJS)
	$(CC) -shared $^ $(LDLIBS) -o $@

clean:
	rm -f *.o *.d *.so bladegps .build_flags

FORCE:

-include $(wildcard *.d)

install: bladegps
	install bladegps /usr/local/bin

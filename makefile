# ----------------------------
# Makefile Options
# ----------------------------

NAME = lwFTP
DESCRIPTION = CE FTP client with lwIP

APP_NAME = lwFTP
APP_VERSION = 5.0.0.0000

CFLAGS = -Wall -Wextra -Oz -I src/include
CXXFLAGS = -Wall -Wextra -Oz -I src/include
OUTPUT_MAP = NO
HAS_LIBC = YES

# BSSHEAP_LOW ?= D052C6
# BSSHEAP_LOW ?= D11FD8
# BSSHEAP_HIGH ?= D13FD8
# ----------------------------

# Include standard allocator from toolchain
EXTRA_ASM_SOURCES = $(CEDEV)/lib/libc/allocator_standard.c.src

include app_tools/makefile


# defining a build rule for the generation of a function table
# to ensure all modules are built into lwip
HEADER_DIRS := src/include/lwip/
EXCLUDE_LIST := src/include/lwip/debug.h
FUNCTABLE_FILE := src/functable.h
HELPER_FILES := $(FUNCTABLE_FILE) tmp/headers.tmp



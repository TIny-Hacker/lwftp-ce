# ----------------------------
# Makefile Options
# ----------------------------

NAME = LWFTP
DESCRIPTION = "FTP client"
COMPRESSED = YES
ARCHIVED = YES
BSSHEAP_LOW = 0xD072C6

CFLAGS = -Wall -Wextra -Oz -I$(RELEASE_ROOT) -I..
CXXFLAGS = -Wall -Wextra -Oz -I$(RELEASE_ROOT) -I..

# ----------------------------

include $(shell cedev-config --makefile)

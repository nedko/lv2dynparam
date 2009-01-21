lib_LTLIBRARIES = liblv2dynparamplugin1.la
liblv2dynparamplugin1_la_SOURCES = plugin.c group.c parameter.c ../audiolock.c ../log.c ../memory_atomic.c ../helpers.c ../hint_set.c internal.h plugin.h
liblv2dynparamplugin1_la_LDFLAGS = -version-info 0:0:0
AM_CFLAGS = -Wall

if DEV_VERSION
AM_CFLAGS += -Werror
endif

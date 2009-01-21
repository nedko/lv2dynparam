lib_LTLIBRARIES = liblv2dynparamhost1.la
liblv2dynparamhost1_la_SOURCES = host.c host_callbacks.c ../audiolock.c ../log.c ../memory_atomic.c ../helpers.c ../hint_set.c host.h host_callbacks.h internal.h
liblv2dynparamhost1_la_LDFLAGS = -version-info 1:0:0
AM_CFLAGS = -Wall

if DEV_VERSION
AM_CFLAGS += -Werror
endif

SUBDIRS = host plugin

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lv2dynparamhost-1.pc lv2dynparamplugin-1.pc

lv2dynparam_includedir = $(includedir)/lv2dynparam-1/lv2dynparam
lv2dynparam_include_HEADERS = audiolock.h lv2.h lv2dynparam.h lv2_rtmempool.h host/host.h plugin/plugin.h

pkginclude_HEADERS = $(lv2dynparam_include_HEADERS)

EXTRA_DIST = $(lv2dynparam_include_HEADERS) list.h helpers.h hint_set.h log.h memory_atomic.h

# adding doxygen support
include $(top_srcdir)/aminclude.am

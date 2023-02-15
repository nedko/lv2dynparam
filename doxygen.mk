PROJECT=lv2dynparam
SRCDIR=./ 
DOCDIR=./doc

export PROJECT
export SRCDIR
export DOCDIR

.PHONY: build_doxygen
build_doxygen: | $(DOCDIR)
	 doxygen doxygen.cfg

$(DOCDIR):
	mkdir -vp $@

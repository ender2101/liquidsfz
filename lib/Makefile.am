AM_CXXFLAGS = $(SNDFILE_CFLAGS)

lib_LTLIBRARIES = libliquidsfz.la
libliquidsfz_la_LDFLAGS = -static
libliquidsfz_la_LIBTOOLFLAGS = --tag=disable-shared
libliquidsfz_la_SOURCES = liquidsfz.cc liquidsfz.hh loader.cc loader.hh log.hh log.cc \
			  synth.hh synth.cc voice.hh voice.cc utils.hh utils.cc samplecache.hh \
			  envelope.hh curve.hh

liquidsfzincludedir = $(includedir)
liquidsfzinclude_HEADERS = liquidsfz.hh

INCLUDES =						\
	-DPREFIX=\"$(prefix)\"				\
	-DSYSCONFDIR=\"$(sysconfdir)\"			\
	-DDATADIR=\"$(datadir)\"			\
	-DLIBDIR=\"$(libdir)\"				\
	-DG_LOG_DOMAIN=\"EBook\"			\
	-I$(top_srcdir) 				\
	-I$(top_srcdir)/camel				\
	-I$(top_builddir)/shell				\
	-I$(top_srcdir)/shell				\
        $(EVOLUTION_ADDRESSBOOK_CFLAGS)

privsolib_LTLIBRARIES = libeabutil.la

libeabutil_la_SOURCES =					\
	eab-marshal.c					\
	eab-book-util.c					\
	eab-book-util.h

libeabutil_la_LDFLAGS = $(NO_UNDEFINED)

libeabutil_la_LIBADD =					\
	$(EVOLUTION_ADDRESSBOOK_LIBS)			\
	$(top_builddir)/widgets/misc/libemiscwidgets.la \
	$(top_builddir)/e-util/libeutil.la

MARSHAL_GENERATED = eab-marshal.c eab-marshal.h
@EVO_MARSHAL_RULE@

BUILT_SOURCES = $(MARSHAL_GENERATED)
CLEANFILES    = $(BUILT_SOURCES)

EXTRA_DIST = 		\
	eab-marshal.list

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

INCLUDES =							\
	-DG_LOG_DOMAIN=\"evolution-addressbook\"		\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/widgets					\
	-I$(top_srcdir)/shell					\
	-I$(top_builddir)/shell					\
	-I$(top_srcdir)/widgets/misc				\
	-I$(top_srcdir)/addressbook/util			\
	-I$(top_srcdir)/addressbook/gui/contact-editor		\
	-I$(top_srcdir)/addressbook/gui/contact-list-editor	\
	-I$(top_srcdir)/addressbook/gui/widgets			\
	-I$(top_srcdir)/a11y/addressbook			\
	-DEVOLUTION_GLADEDIR=\""$(gladedir)"\"			\
	-DEVOLUTION_UIDIR=\""$(evolutionuidir)"\"		\
	-DPREFIX=\""$(prefix)"\"				\
	$(LDAP_CFLAGS)						\
	$(EVOLUTION_ADDRESSBOOK_CFLAGS)

component_LTLIBRARIES = libevolution-addressbook.la

libevolution_addressbook_la_SOURCES = 	\
	addressbook-component.c		\
	addressbook-component.h		\
	addressbook-config.c		\
	addressbook-config.h		\
	addressbook-migrate.c		\
	addressbook-migrate.h		\
	autocompletion-config.c		\
	autocompletion-config.h		\
	addressbook.c			\
	addressbook.h			\
	addressbook-view.c		\
	addressbook-view.h		\
	component-factory.c

#	$(top_builddir)/addressbook/printing/libecontactprint.la

if ENABLE_SMIME
SMIME_LIB=$(top_builddir)/smime/gui/libevolution-smime.la
endif

libevolution_addressbook_la_LIBADD =							\
	$(SMIME_LIB)									\
	$(top_builddir)/e-util/libeutil.la						\
	$(top_builddir)/addressbook/printing/libecontactprint.la			\
	$(top_builddir)/shell/libeshell.la						\
	$(top_builddir)/addressbook/gui/merging/libeabbookmerging.la			\
	$(top_builddir)/addressbook/gui/widgets/libeabwidgets.la			\
	$(top_builddir)/filter/libfilter.la						\
	$(top_builddir)/addressbook/util/libeabutil.la					\
	$(top_builddir)/addressbook/gui/contact-editor/libecontacteditor.la		\
	$(top_builddir)/addressbook/gui/contact-list-editor/libecontactlisteditor.la	\
	$(top_builddir)/widgets/table/libetable.la					\
	$(top_builddir)/widgets/text/libetext.la					\
	$(top_builddir)/widgets/misc/libemiscwidgets.la					\
	$(top_builddir)/widgets/misc/libefilterbar.la					\
	$(top_builddir)/widgets/menus/libmenus.la					\
	$(top_builddir)/a11y/addressbook/libevolution-addressbook-a11y.la		\
	$(top_builddir)/addressbook/importers/libevolution-addressbook-importers.la	\
	$(EVOLUTION_ADDRESSBOOK_LIBS) $(LDAP_LIBS)


libevolution_addressbook_la_LDFLAGS = -module -avoid-version $(NO_UNDEFINED)

# GConf schemas

schemadir   = $(GCONF_SCHEMA_FILE_DIR)
schema_in_files = apps_evolution_addressbook.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)

@INTLTOOL_SCHEMAS_RULE@

if OS_WIN32
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			(echo set GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE); \
			 echo $(GCONFTOOL) --makefile-install-rule $$p) >_temp.bat; \
			cmd /c _temp.bat; \
		done \
	fi
else
install-data-local:
	if test -z "$(DESTDIR)" ; then \
		for p in $(schema_DATA) ; do \
			GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) $(GCONFTOOL) --makefile-install-rule $$p; \
		done \
	fi
endif

server_in_files = GNOME_Evolution_Addressbook.server.in.in
server_DATA = $(server_in_files:.server.in.in=.server)
@EVO_SERVER_RULE@
@INTLTOOL_SERVER_RULE@

glade_DATA =			\
	ldap-config.glade

BUILT_SOURCES = $(server_DATA)
CLEANFILES    = $(BUILT_SOURCES)

DISTCLEANFILES = $(schema_DATA)

EXTRA_DIST =			\
	$(glade_DATA)		\
	$(schema_in_files)	\
	$(server_in_files)	\
	openldap-extract.h

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

if ENABLE_PURIFY
PLINK = $(LIBTOOL) --mode=link $(PURIFY) $(CCLD) $(AM_CFLAGS) $(CFLAGS) $(LDFLAGS) -o $@

all-local: evolution-addressbook.pure

evolution-addressbook.pure: evolution-addressbook
	@rm -f evolution-addressbook.pure
	$(PLINK) $(evolution_addressbook_LDFLAGS) $(evolution_addressbook_OBJECTS) $(evolution_addressbook_LDADD) $(LIBS)

endif


# for debug
#A11Y_CFLAGS += -pedantic -ansi -DACC_DEBUG -Werror

if OS_WIN32
WIN32_BOOTSTRAP_LIBS = $(top_builddir)/win32/libemiscwidgets.la
endif

privsolib_LTLIBRARIES = libevolution-widgets-a11y.la

INCLUDES =						\
	-DG_LOG_DOMAIN=\"evolution-a11y\"		\
	-I$(top_srcdir)					\
	-I$(top_srcdir)/a11y				\
	-I$(top_srcdir)/widgets				\
	$(E_WIDGETS_CFLAGS)

libevolution_widgets_a11y_la_SOURCES =		\
	ea-calendar-item.c			\
	ea-calendar-item.h			\
	ea-calendar-cell.c			\
	ea-calendar-cell.h			\
	ea-combo-button.c			\
	ea-combo-button.h			\
	ea-expander.c				\
	ea-expander.h				\
	ea-widgets.c				\
	ea-widgets.h

libevolution_widgets_a11y_la_LDFLAGS = $(NO_UNDEFINED)

libevolution_widgets_a11y_la_LIBADD =			\
	$(WIN32_BOOTSTRAP_LIBS)				\
        $(top_builddir)/a11y/libevolution-a11y.la	\
	$(top_builddir)/e-util/libeutil.la		\
	$(E_WIDGETS_LIBS)

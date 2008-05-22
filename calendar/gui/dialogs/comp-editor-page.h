/* Evolution calendar - Base class for calendar component editor pages
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef COMP_EDITOR_PAGE_H
#define COMP_EDITOR_PAGE_H

#include <time.h>
#include <gtk/gtkwidget.h>
#include <libecal/e-cal-component.h>
#include <libecal/e-cal.h>

G_BEGIN_DECLS



#define TYPE_COMP_EDITOR_PAGE            (comp_editor_page_get_type ())
#define COMP_EDITOR_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_COMP_EDITOR_PAGE, CompEditorPage))
#define COMP_EDITOR_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_COMP_EDITOR_PAGE,	CompEditorPageClass))
#define IS_COMP_EDITOR_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_COMP_EDITOR_PAGE))
#define IS_COMP_EDITOR_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), TYPE_COMP_EDITOR_PAGE))

typedef struct {
	ECalComponentDateTime *start;
	ECalComponentDateTime *end;
	ECalComponentDateTime *due;
	struct icaltimetype *complete;
} CompEditorPageDates;

typedef enum {
	COMP_EDITOR_PAGE_NEW_ITEM = 1<<0,
	COMP_EDITOR_PAGE_MEETING = 1<<1,
	COMP_EDITOR_PAGE_DELEGATE = 1<<2,
	COMP_EDITOR_PAGE_USER_ORG = 1<<3,
	COMP_EDITOR_PAGE_IS_ASSIGNED = 1<<4,
	COMP_EDITOR_PAGE_IS_SHARED = 1<<5
} CompEditorPageFlags;

typedef struct {
	GObject object;

	/* Some of the pages need the ECal to access timezone data. Also,
	 * the event page needs to know it to fill the source option menu. */
	ECal *client;

	/* The GtkAccelGroup for the page. We install this when the page is
	   mapped, and uninstall when it is unmapped. libglade would do this
	   normally, but we create our pages individually so have to do it
	   ourselves. */
	GtkAccelGroup *accel_group;

	CompEditorPageFlags flags;

} CompEditorPage;

typedef struct {
	GObjectClass parent_class;

	/* Notification signals */

	void (* changed)    (CompEditorPage *page);
	void (* needs_send) (CompEditorPage *page);

	void (* summary_changed) (CompEditorPage *page, const char *summary);
	void (* dates_changed)   (CompEditorPage *page, const char *dates);
	void (* client_changed)  (CompEditorPage *page, ECal *client);
	void (* focus_in) 	 (CompEditorPage *page, GtkWidget *widget);
	void (* focus_out)	 (CompEditorPage *page, GtkWidget *widget);
	/* Virtual methods */

	GtkWidget *(* get_widget) (CompEditorPage *page);
	void (* focus_main_widget) (CompEditorPage *page);

	gboolean (* fill_widgets) (CompEditorPage *page, ECalComponent *comp);
	gboolean (* fill_component) (CompEditorPage *page, ECalComponent *comp);
	gboolean (* fill_timezones) (CompEditorPage *page, GHashTable *timezones);

	void (* set_summary) (CompEditorPage *page, const char *summary);
	void (* set_dates) (CompEditorPage *page, CompEditorPageDates *dates);
} CompEditorPageClass;

GType      comp_editor_page_get_type               (void);
GtkWidget *comp_editor_page_get_widget             (CompEditorPage      *page);
void       comp_editor_page_focus_main_widget      (CompEditorPage      *page);
void       comp_editor_page_unset_focused_widget   (CompEditorPage *page, GtkWidget *widget);
void       comp_editor_page_set_focused_widget     (CompEditorPage *page, GtkWidget *widget);
gboolean   comp_editor_page_fill_widgets           (CompEditorPage      *page,
						    ECalComponent        *comp);
gboolean   comp_editor_page_fill_component         (CompEditorPage      *page,
						    ECalComponent        *comp);
gboolean   comp_editor_page_fill_timezones         (CompEditorPage      *page,
						    GHashTable          *timezones);
void       comp_editor_page_set_e_cal              (CompEditorPage      *page,
						    ECal           *client);
void       comp_editor_page_set_summary            (CompEditorPage      *page,
						    const char          *summary);
void       comp_editor_page_set_dates              (CompEditorPage      *page,
						    CompEditorPageDates *dates);
void       comp_editor_page_notify_changed         (CompEditorPage      *page);
void       comp_editor_page_notify_needs_send      (CompEditorPage      *page);
void       comp_editor_page_notify_summary_changed (CompEditorPage      *page,
						    const char          *summary);
void       comp_editor_page_notify_dates_changed   (CompEditorPage      *page,
						    CompEditorPageDates *dates);
void       comp_editor_page_notify_client_changed  (CompEditorPage      *page,
						    ECal                *client);
void       comp_editor_page_display_validation_error (CompEditorPage      *page,
						      const char          *msg,
						      GtkWidget           *field);




G_END_DECLS

#endif

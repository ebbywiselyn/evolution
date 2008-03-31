/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Alarm page of the calendar component dialogs
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *          Hans Petter Jansson <hpj@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkfilechooserbutton.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <gtk/gtktogglebutton.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-widget.h>
#include <glade/glade.h>
#include <libedataserver/e-time-utils.h>
#include "e-util/e-dialog-widgets.h"
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-util-private.h"
#include <libebook/e-destination.h>
#include <libedataserverui/e-name-selector.h>
#include <libical/icalattach.h>
#include "../calendar-config.h"
#include "comp-editor-util.h"
#include "alarm-dialog.h"



typedef struct {
	/* Glade XML data */
	GladeXML *xml;

	/* The alarm  */
	ECalComponentAlarm *alarm;

	/* The client */
	ECal *ecal;

	/* Toplevel */
	GtkWidget *toplevel;

	GtkWidget *action;
	GtkWidget *interval_value;
	GtkWidget *value_units;
	GtkWidget *relative;
	GtkWidget *time;

	/* Alarm repeat widgets */
	GtkWidget *repeat_toggle;
	GtkWidget *repeat_group;
	GtkWidget *repeat_quantity;
	GtkWidget *repeat_value;
	GtkWidget *repeat_unit;

	GtkWidget *option_notebook;

	/* Display alarm widgets */
	GtkWidget *dalarm_group;
	GtkWidget *dalarm_message;
	GtkWidget *dalarm_description;

	/* Audio alarm widgets */
	GtkWidget *aalarm_group;
	GtkWidget *aalarm_sound;
	GtkWidget *aalarm_file_chooser;

	/* Mail alarm widgets */
	const char *email;
	GtkWidget *malarm_group;
	GtkWidget *malarm_address_group;
	GtkWidget *malarm_addresses;
	GtkWidget *malarm_addressbook;
	GtkWidget *malarm_message;
	GtkWidget *malarm_description;

	/* Procedure alarm widgets */
	GtkWidget *palarm_group;
	GtkWidget *palarm_program;
	GtkWidget *palarm_args;

	/* Addressbook name selector */
	ENameSelector *name_selector;
} Dialog;

static const char *section_name = "Send To";

/* "relative" types */
enum {
	BEFORE,
	AFTER
};

/* Time units */
enum {
	MINUTES,
	HOURS,
	DAYS
};

/* Option menu maps */
static const int action_map[] = {
	E_CAL_COMPONENT_ALARM_DISPLAY,
	E_CAL_COMPONENT_ALARM_AUDIO,
	E_CAL_COMPONENT_ALARM_PROCEDURE,
	E_CAL_COMPONENT_ALARM_EMAIL,
	-1
};

static const char *action_map_cap[] = {
	CAL_STATIC_CAPABILITY_NO_DISPLAY_ALARMS,
	CAL_STATIC_CAPABILITY_NO_AUDIO_ALARMS,
        CAL_STATIC_CAPABILITY_NO_PROCEDURE_ALARMS,
	CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS
};

static const int value_map[] = {
	MINUTES,
	HOURS,
	DAYS,
	-1
};

static const int relative_map[] = {
	BEFORE,
	AFTER,
	-1
};

static const int time_map[] = {
	E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START,
	E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END,
	-1
};

enum duration_units {
	DUR_MINUTES,
	DUR_HOURS,
	DUR_DAYS
};

static const int duration_units_map[] = {
	DUR_MINUTES,
	DUR_HOURS,
	DUR_DAYS,
	-1
};

static void populate_widgets_from_alarm (Dialog *dialog);
static void action_selection_done_cb (GtkMenuShell *menu_shell, gpointer data);

/* Fills the widgets with default values */
static void
clear_widgets (Dialog *dialog)
{
	/* Sane defaults */
	e_dialog_option_menu_set (dialog->action, E_CAL_COMPONENT_ALARM_DISPLAY, action_map);
	e_dialog_spin_set (dialog->interval_value, 15);
	e_dialog_option_menu_set (dialog->value_units, MINUTES, value_map);
	e_dialog_option_menu_set (dialog->relative, BEFORE, relative_map);
	e_dialog_option_menu_set (dialog->time, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, time_map);

	gtk_widget_set_sensitive (dialog->repeat_group, FALSE);
	gtk_widget_set_sensitive (dialog->dalarm_group, FALSE);
	gtk_widget_set_sensitive (dialog->aalarm_group, FALSE);
	gtk_widget_set_sensitive (dialog->malarm_group, FALSE);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (dialog->option_notebook), 0);
}

/* fill_widgets handler for the alarm page */
static void
alarm_to_dialog (Dialog *dialog)
{
	GtkWidget *menu;
	GList *l;
	gboolean repeat;
	ECalComponentAlarmAction action;
	char *email;
	int i;

	/* Clean the page */
	clear_widgets (dialog);

	/* Alarm types */
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (dialog->action));
	for (i = 0, l = GTK_MENU_SHELL (menu)->children; action_map[i] != -1; i++, l = l->next) {
		if (e_cal_get_static_capability (dialog->ecal, action_map_cap[i]))
			gtk_widget_set_sensitive (l->data, FALSE);
		else
			gtk_widget_set_sensitive (l->data, TRUE);
	}

	/* Set a default address if possible */
	if (!e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS)
		&& !e_cal_component_alarm_has_attendees (dialog->alarm)
	    && e_cal_get_alarm_email_address (dialog->ecal, &email, NULL)) {
		ECalComponentAttendee *a;
		GSList attendee_list;

		a = g_new0 (ECalComponentAttendee, 1);
		a->value = email;
		attendee_list.data = a;
		attendee_list.next = NULL;
		e_cal_component_alarm_set_attendee_list (dialog->alarm, &attendee_list);
		g_free (email);
		g_free (a);
	}

	/* If we can repeat */
	repeat = !e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_ALARM_REPEAT);
	gtk_widget_set_sensitive (dialog->repeat_toggle, repeat);

	/* if we are editing a exiting alarm */
	e_cal_component_alarm_get_action (dialog->alarm, &action);

	if (action)
		populate_widgets_from_alarm (dialog);
}

static void
alarm_to_repeat_widgets (Dialog *dialog, ECalComponentAlarm *alarm)
{
	ECalComponentAlarmRepeat repeat;

	e_cal_component_alarm_get_repeat (dialog->alarm, &repeat);

	if ( repeat.repetitions ) {
		e_dialog_toggle_set (dialog->repeat_toggle, TRUE);
		e_dialog_spin_set (dialog->repeat_quantity, repeat.repetitions);
	} else
		return;

	if ( repeat.duration.minutes ) {
		e_dialog_option_menu_set (dialog->repeat_unit, DUR_MINUTES, duration_units_map);
		e_dialog_spin_set (dialog->repeat_value, repeat.duration.minutes);
	}

	if ( repeat.duration.hours ) {
		e_dialog_option_menu_set (dialog->repeat_unit, DUR_HOURS, duration_units_map);
		e_dialog_spin_set (dialog->repeat_value, repeat.duration.hours);
	}

	if ( repeat.duration.days ) {
		e_dialog_option_menu_set (dialog->repeat_unit, DUR_DAYS, duration_units_map);
		e_dialog_spin_set (dialog->repeat_value, repeat.duration.days);
	}
}

static void
repeat_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	ECalComponentAlarmRepeat repeat;

	if (!e_dialog_toggle_get (dialog->repeat_toggle)) {
		repeat.repetitions = 0;

		e_cal_component_alarm_set_repeat (alarm, repeat);
		return;
	}

	repeat.repetitions = e_dialog_spin_get_int (dialog->repeat_quantity);

	memset (&repeat.duration, 0, sizeof (repeat.duration));
	switch (e_dialog_option_menu_get (dialog->repeat_unit, duration_units_map)) {
	case DUR_MINUTES:
		repeat.duration.minutes = e_dialog_spin_get_int (dialog->repeat_value);
		break;

	case DUR_HOURS:
		repeat.duration.hours = e_dialog_spin_get_int (dialog->repeat_value);
		break;

	case DUR_DAYS:
		repeat.duration.days = e_dialog_spin_get_int (dialog->repeat_value);
		break;

	default:
		g_return_if_reached ();
	}

	e_cal_component_alarm_set_repeat (alarm, repeat);

}

/* Fills the audio alarm data with the values from the widgets */
static void
aalarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	char *url;
	icalattach *attach;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->aalarm_sound)))
		return;

	url = gtk_file_chooser_get_uri (
		GTK_FILE_CHOOSER (dialog->aalarm_file_chooser));
	attach = icalattach_new_from_url (url ? url : "");
	g_free (url);

	e_cal_component_alarm_set_attach (alarm, attach);
	icalattach_unref (attach);
}

/* Fills the widgets with audio alarm data */
static void
alarm_to_aalarm_widgets (Dialog *dialog, ECalComponentAlarm *alarm)
{
	const char *url;
	icalattach *attach;

	e_cal_component_alarm_get_attach (alarm, (&attach));
	url = icalattach_get_url (attach);
	icalattach_unref (attach);

	if ( !(url && *url) )
		return;

	e_dialog_toggle_set (dialog->aalarm_sound, TRUE);
	gtk_file_chooser_set_uri (
		GTK_FILE_CHOOSER (dialog->aalarm_file_chooser), url);
}

/* Fills the widgets with display alarm data */
static void
alarm_to_dalarm_widgets (Dialog *dialog, ECalComponentAlarm *alarm )
{
	ECalComponentText description;
	GtkTextBuffer *text_buffer;

	e_cal_component_alarm_get_description (alarm, &description);

	if (description.value) {
		e_dialog_toggle_set (dialog->dalarm_message, TRUE);
		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
		gtk_text_buffer_set_text (text_buffer, description.value, -1);
	}
}

/* Fills the display alarm data with the values from the widgets */
static void
dalarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	char *str;
	ECalComponentText description;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->dalarm_message)))
		return;

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	description.value = str;
	description.altrep = NULL;

	e_cal_component_alarm_set_description (alarm, &description);
	g_free (str);

	/* remove the X-EVOLUTION-NEEDS-DESCRIPTION property, so that
	 * we don't re-set the alarm's description */
	icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	while (icalprop) {
		const char *x_name;

		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
			icalcomponent_remove_property (icalcomp, icalprop);
			break;
		}

		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}
}

/* Fills the mail alarm data with the values from the widgets */
static void
malarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	char *str;
	ECalComponentText description;
	GSList *attendee_list = NULL;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	ENameSelectorModel *name_selector_model;
	EDestinationStore *destination_store;
	GList *destinations;
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	GList *l;

	/* Attendees */
	name_selector_model = e_name_selector_peek_model (dialog->name_selector);
	e_name_selector_model_peek_section (name_selector_model, section_name, NULL, &destination_store);
	destinations = e_destination_store_list_destinations (destination_store);

	for (l = destinations; l; l = g_list_next (l)) {
		EDestination *dest;
		ECalComponentAttendee *a;

		dest = l->data;

		a = g_new0 (ECalComponentAttendee, 1);
		a->value = e_destination_get_email (dest);
		a->cn = e_destination_get_name (dest);

		attendee_list = g_slist_append (attendee_list, a);
	}

	e_cal_component_alarm_set_attendee_list (alarm, attendee_list);

	e_cal_component_free_attendee_list (attendee_list);
	g_list_free (destinations);

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->malarm_message)))
		return;

	/* Description */
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->malarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	description.value = str;
	description.altrep = NULL;

	e_cal_component_alarm_set_description (alarm, &description);
	g_free (str);

	/* remove the X-EVOLUTION-NEEDS-DESCRIPTION property, so that
	 * we don't re-set the alarm's description */
	icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
	icalprop = icalcomponent_get_first_property(icalcomp, ICAL_X_PROPERTY);
	while (icalprop) {
		const char *x_name;

		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
			icalcomponent_remove_property (icalcomp, icalprop);
			break;
		}

		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}
}

/* Fills the widgets from mail alarm data */
static void
alarm_to_malarm_widgets (Dialog *dialog, ECalComponentAlarm *alarm )
{
    ENameSelectorModel *name_selector_model;
    EDestinationStore *destination_store;
    ECalComponentText description;
    GtkTextBuffer *text_buffer;
    GSList *attendee_list, *l;
    int len;

    /* Attendees */
    name_selector_model = e_name_selector_peek_model (dialog->name_selector);
    e_name_selector_model_peek_section (name_selector_model, section_name, NULL, &destination_store);

    e_cal_component_alarm_get_attendee_list (alarm, &attendee_list);
    len = g_slist_length (attendee_list);
    if (len > 0) {
        for (l = attendee_list; l; l = g_slist_next (l)) {
            ECalComponentAttendee *a = l->data;
            EDestination *dest;

            dest = e_destination_new ();
            if (a->cn != NULL && *a->cn)
                e_destination_set_name (dest, a->cn);
            if (a->value != NULL && *a->value) {
                if (!strncasecmp (a->value, "MAILTO:", 7))
                    e_destination_set_email (dest, a->value + 7);
                else
                    e_destination_set_email (dest, a->value);
            }
            e_destination_store_append_destination (destination_store, dest);
            g_object_unref(GTK_OBJECT (dest));
        }
        e_cal_component_free_attendee_list (attendee_list);
    }

    /* Description */
    e_cal_component_alarm_get_description (alarm, &description);
    if (description.value) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->malarm_message), TRUE);
        text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->malarm_description));
        gtk_text_buffer_set_text (text_buffer, description.value, -1);
    }
}

/* Fills the widgets from procedure alarm data */
static void
alarm_to_palarm_widgets (Dialog *dialog, ECalComponentAlarm *alarm)
{
	ECalComponentText description;
	const char *url;
	icalattach *attach;

	e_cal_component_alarm_get_attach (alarm, (&attach));
	url = icalattach_get_url (attach);
	icalattach_unref (attach);

	if ( !(url && *url) )
		return;

	e_dialog_editable_set (dialog->palarm_program, url);
	e_cal_component_alarm_get_description (alarm, &description);

	e_dialog_editable_set (dialog->palarm_args, description.value);
}

/* Fills the procedure alarm data with the values from the widgets */
static void
palarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	char *program;
	icalattach *attach;
	char *str;
	ECalComponentText description;
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	program = e_dialog_editable_get (dialog->palarm_program);
	attach = icalattach_new_from_url (program ? program : "");
	g_free (program);

	e_cal_component_alarm_set_attach (alarm, attach);
	icalattach_unref (attach);

	str = e_dialog_editable_get (dialog->palarm_args);

		description.value = str;
		description.altrep = NULL;

		e_cal_component_alarm_set_description (alarm, &description);

	g_free (str);

	/* remove the X-EVOLUTION-NEEDS-DESCRIPTION property, so that
	 * we don't re-set the alarm's description */
	icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	while (icalprop) {
		const char *x_name;

		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
			icalcomponent_remove_property (icalcomp, icalprop);
			break;
		}

		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}
}

static void
populate_widgets_from_alarm (Dialog *dialog)
{
	ECalComponentAlarmTrigger *trigger;
	ECalComponentAlarmAction *action;

	action = g_new0 (ECalComponentAlarmAction, 1);
	e_cal_component_alarm_get_action (dialog->alarm, action);
	g_return_if_fail ( action != NULL );

	trigger = g_new0 (ECalComponentAlarmTrigger, 1);
	e_cal_component_alarm_get_trigger (dialog->alarm, trigger);
	g_return_if_fail ( trigger != NULL );

	if ( *action == E_CAL_COMPONENT_ALARM_NONE )
		return;

	gtk_window_set_title (GTK_WINDOW (dialog->toplevel),_("Edit Alarm"));

	/* Alarm Types */
	switch ( trigger->type ) {
 	case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START:
		e_dialog_option_menu_set (dialog->time, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, time_map);
		break;

	case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END:
		e_dialog_option_menu_set (dialog->time, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END, time_map);
		break;
        default:
                g_warning ("%s: Unexpected alarm type (%d)", G_STRLOC, trigger->type);
	}

	switch ( trigger->u.rel_duration.is_neg ){
	case 1:
		e_dialog_option_menu_set (dialog->relative, BEFORE, relative_map);
		break;

	case 0:
		e_dialog_option_menu_set (dialog->relative, AFTER, relative_map);
		break;
	}

	if ( trigger->u.rel_duration.hours ) {
		e_dialog_option_menu_set (dialog->value_units, HOURS, value_map);
		e_dialog_spin_set (dialog->interval_value, trigger->u.rel_duration.hours);
	}

	if ( trigger->u.rel_duration.minutes ){
		e_dialog_option_menu_set (dialog->value_units, MINUTES, value_map);
		e_dialog_spin_set (dialog->interval_value, trigger->u.rel_duration.minutes);
	}

	if ( trigger->u.rel_duration.days ){
		e_dialog_option_menu_set (dialog->value_units, DAYS, value_map);
		e_dialog_spin_set (dialog->interval_value, trigger->u.rel_duration.days);
	}

	/* Repeat options */
	alarm_to_repeat_widgets (dialog, dialog->alarm);

	/* Alarm options */
	e_dialog_option_menu_set (dialog->action, *action, action_map);
	action_selection_done_cb (GTK_MENU_SHELL (dialog->action), dialog);

	switch (*action) {
	case E_CAL_COMPONENT_ALARM_AUDIO:
		alarm_to_aalarm_widgets (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
		alarm_to_dalarm_widgets (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL:
		alarm_to_malarm_widgets (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE:
		alarm_to_palarm_widgets (dialog, dialog->alarm);
		break;
        default:
                g_warning ("%s: Unexpected alarm action (%d)", G_STRLOC, *action);
	}
}

/* fill_component handler for the alarm page */
static void
dialog_to_alarm (Dialog *dialog)
{
	ECalComponentAlarmTrigger trigger;
	ECalComponentAlarmAction action;

	/* Fill out the alarm */
	memset (&trigger, 0, sizeof (ECalComponentAlarmTrigger));
	trigger.type = e_dialog_option_menu_get (dialog->time, time_map);
	if (e_dialog_option_menu_get (dialog->relative, relative_map) == BEFORE)
		trigger.u.rel_duration.is_neg = 1;
	else
		trigger.u.rel_duration.is_neg = 0;

	switch (e_dialog_option_menu_get (dialog->value_units, value_map)) {
	case MINUTES:
		trigger.u.rel_duration.minutes =
			e_dialog_spin_get_int (dialog->interval_value);
		break;

	case HOURS:
		trigger.u.rel_duration.hours =
			e_dialog_spin_get_int (dialog->interval_value);
		break;

	case DAYS:
		trigger.u.rel_duration.days =
			e_dialog_spin_get_int (dialog->interval_value);
		break;

	default:
		g_return_if_reached ();
	}
	e_cal_component_alarm_set_trigger (dialog->alarm, trigger);

	action = e_dialog_option_menu_get (dialog->action, action_map);
	e_cal_component_alarm_set_action (dialog->alarm, action);

	/* Repeat stuff */
	repeat_widgets_to_alarm (dialog, dialog->alarm);

	/* Options */
	switch (action) {
	case E_CAL_COMPONENT_ALARM_NONE:
		g_return_if_reached ();
		break;

	case E_CAL_COMPONENT_ALARM_AUDIO:
		aalarm_widgets_to_alarm (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
		dalarm_widgets_to_alarm (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL:
		malarm_widgets_to_alarm (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE:
		palarm_widgets_to_alarm (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_UNKNOWN:
		break;

	default:
		g_return_if_reached ();
	}
}

/* Gets the widgets from the XML file and returns TRUE if they are all available. */
static gboolean
get_widgets (Dialog *dialog)
{
#define GW(name) glade_xml_get_widget (dialog->xml, name)

	dialog->toplevel = GW ("alarm-dialog");
	if (!dialog->toplevel)
		return FALSE;

	dialog->action = GW ("action");
	dialog->interval_value = GW ("interval-value");
	dialog->value_units = GW ("value-units");
	dialog->relative = GW ("relative");
	dialog->time = GW ("time");

	dialog->repeat_toggle = GW ("repeat-toggle");
	dialog->repeat_group = GW ("repeat-group");
	dialog->repeat_quantity = GW ("repeat-quantity");
	dialog->repeat_value = GW ("repeat-value");
	dialog->repeat_unit = GW ("repeat-unit");

	dialog->option_notebook = GW ("option-notebook");

	dialog->dalarm_group = GW ("dalarm-group");
	dialog->dalarm_message = GW ("dalarm-message");
	dialog->dalarm_description = GW ("dalarm-description");

	dialog->aalarm_group = GW ("aalarm-group");
	dialog->aalarm_sound = GW ("aalarm-sound");
	dialog->aalarm_file_chooser = GW ("aalarm-file-chooser");

	dialog->malarm_group = GW ("malarm-group");
	dialog->malarm_address_group = GW ("malarm-address-group");
	dialog->malarm_addressbook = GW ("malarm-addressbook");
	dialog->malarm_message = GW ("malarm-message");
	dialog->malarm_description = GW ("malarm-description");

	dialog->palarm_group = GW ("palarm-group");
	dialog->palarm_program = GW ("palarm-program");
	dialog->palarm_args = GW ("palarm-args");

#undef GW

	return (dialog->action
		&& dialog->interval_value
		&& dialog->value_units
		&& dialog->relative
		&& dialog->time
		&& dialog->repeat_toggle
		&& dialog->repeat_group
		&& dialog->repeat_quantity
		&& dialog->repeat_value
		&& dialog->repeat_unit
		&& dialog->option_notebook
		&& dialog->dalarm_group
		&& dialog->dalarm_message
		&& dialog->dalarm_description
		&& dialog->aalarm_group
		&& dialog->aalarm_sound
		&& dialog->aalarm_file_chooser
		&& dialog->malarm_group
		&& dialog->malarm_address_group
		&& dialog->malarm_addressbook
		&& dialog->malarm_message
		&& dialog->malarm_description
		&& dialog->palarm_group
		&& dialog->palarm_program
		&& dialog->palarm_args);
}

#if 0
/* Callback used when the alarm options button is clicked */
static void
show_options (Dialog *dialog)
{
	gboolean repeat;
	char *email;

	e_cal_component_alarm_set_action (dialog->alarm,
					e_dialog_option_menu_get (dialog->action, action_map));

	repeat = !e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_ALARM_REPEAT);

	if (e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS)
	    || e_cal_get_alarm_email_address (dialog->ecal, &email, NULL)) {
		if (!alarm_options_dialog_run (dialog->toplevel, dialog->alarm, email, repeat))
			g_message (G_STRLOC ": not create the alarm options dialog");
	}
}
#endif

static void
addressbook_clicked_cb (GtkWidget *widget, gpointer data)
{
	Dialog *dialog = data;
	ENameSelectorDialog *name_selector_dialog;

	name_selector_dialog = e_name_selector_peek_dialog (dialog->name_selector);
	gtk_widget_show (GTK_WIDGET (name_selector_dialog));
}

static void
addressbook_response_cb (GtkWidget *widget, gint response, gpointer data)
{
	Dialog *dialog = data;
	ENameSelectorDialog *name_selector_dialog;

	name_selector_dialog = e_name_selector_peek_dialog (dialog->name_selector);
	gtk_widget_hide (GTK_WIDGET (name_selector_dialog));
}

static gboolean
setup_select_names (Dialog *dialog)
{
	ENameSelectorModel *name_selector_model;
	ENameSelectorDialog *name_selector_dialog;

	dialog->name_selector = e_name_selector_new ();
	name_selector_model = e_name_selector_peek_model (dialog->name_selector);

	e_name_selector_model_add_section (name_selector_model, section_name, section_name, NULL);

	dialog->malarm_addresses =
		GTK_WIDGET (e_name_selector_peek_section_entry (dialog->name_selector, section_name));
	gtk_widget_show (dialog->malarm_addresses);
	gtk_box_pack_end_defaults (GTK_BOX (dialog->malarm_address_group), dialog->malarm_addresses);

	g_signal_connect (G_OBJECT (dialog->malarm_addressbook), "clicked",
			  G_CALLBACK (addressbook_clicked_cb), dialog);

	name_selector_dialog = e_name_selector_peek_dialog (dialog->name_selector);
	g_signal_connect (name_selector_dialog, "response",
			  G_CALLBACK (addressbook_response_cb), dialog);

	return TRUE;
}

/* Callback used when the repeat toggle button is toggled.  We sensitize the
 * repeat group options as appropriate.
 */
static void
repeat_toggle_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	Dialog *dialog = data;
	gboolean active;

	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (dialog->repeat_group, active);
}

static void
check_custom_sound (Dialog *dialog)
{
	char *str, *dir;
	gboolean sens;

	str = gtk_file_chooser_get_filename (
		GTK_FILE_CHOOSER (dialog->aalarm_file_chooser));

	if ( str && *str ) {
		dir = g_path_get_dirname (str);
		if ( dir && *dir ) {
			calendar_config_set_dir_path (dir);
		}
	}

	sens = e_dialog_toggle_get (dialog->aalarm_sound) ? str && *str : TRUE;
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog->toplevel), GTK_RESPONSE_OK, sens);

	g_free (str);
}

static void
aalarm_sound_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	Dialog *dialog = data;
	gboolean active;

	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (dialog->aalarm_group, active);
	check_custom_sound (dialog);
}

static void
aalarm_attach_changed_cb (GtkWidget *widget, gpointer data)
{
	Dialog *dialog = data;

	check_custom_sound (dialog);
}

static void
check_custom_message (Dialog *dialog)
{
	char *str;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	gboolean sens;

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	sens = e_dialog_toggle_get (dialog->dalarm_message) ? str && *str : TRUE;
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog->toplevel), GTK_RESPONSE_OK, sens);

	g_free (str);
}

static void
dalarm_message_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	Dialog *dialog = data;
	gboolean active;

	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (dialog->dalarm_group, active);
	check_custom_message (dialog);
}

static void
dalarm_description_changed_cb (GtkWidget *widget, gpointer data)
{
	Dialog *dialog = data;

	check_custom_message (dialog);
}

static void
check_custom_program (Dialog *dialog)
{
	char *str;
	gboolean sens;

	str = e_dialog_editable_get (dialog->palarm_program);

	sens = str && *str;
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog->toplevel), GTK_RESPONSE_OK, sens);
}

static void
palarm_program_changed_cb (GtkWidget *widget, gpointer data)
{
	Dialog *dialog = data;

	check_custom_program (dialog);
}

static void
check_custom_email (Dialog *dialog)
{
	char *str;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	ENameSelectorModel *name_selector_model;
	EDestinationStore *destination_store;
	GList *destinations;
	gboolean sens;

	name_selector_model = e_name_selector_peek_model (dialog->name_selector);
	e_name_selector_model_peek_section (name_selector_model, section_name, NULL, &destination_store);
	destinations = e_destination_store_list_destinations (destination_store);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->malarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	sens = (destinations != NULL) && (e_dialog_toggle_get (dialog->malarm_message) ? str && *str : TRUE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog->toplevel), GTK_RESPONSE_OK, sens);

	g_list_free (destinations);
}

static void
malarm_addresses_changed_cb  (GtkWidget *editable,
			      gpointer   data)
{
	Dialog *dialog = data;

	check_custom_email (dialog);
}

static void
malarm_message_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	Dialog *dialog = data;
	gboolean active;

	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (dialog->malarm_group, active);
	check_custom_email (dialog);
}

static void
malarm_description_changed_cb (GtkWidget *widget, gpointer data)
{
	Dialog *dialog = data;

	check_custom_email (dialog);
}

static void
action_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	Dialog *dialog = data;
	char *dir;
	ECalComponentAlarmAction action;
	int page = 0, i;

	action = e_dialog_option_menu_get (dialog->action, action_map);
	for (i = 0; action_map[i] != -1 ; i++) {
		if (action == action_map[i]) {
			page = i;
			break;
		}
	}

	gtk_notebook_set_current_page (
		GTK_NOTEBOOK (dialog->option_notebook), page);

	switch (action) {
	case E_CAL_COMPONENT_ALARM_AUDIO:
		dir = calendar_config_get_dir_path ();
		if ( dir && *dir )
			gtk_file_chooser_set_current_folder (
				GTK_FILE_CHOOSER (dialog->aalarm_file_chooser),
				dir);
		g_free (dir);
		check_custom_sound (dialog);
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
		check_custom_message (dialog);
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL:
		check_custom_email (dialog);
		break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE:
		check_custom_program (dialog);
		break;
	default:
		g_return_if_reached ();
		return;
	}
}

/* Hooks the widget signals */
static void
init_widgets (Dialog *dialog)
{
	GtkWidget *menu;
	GtkTextBuffer *text_buffer;

	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (dialog->action));
	g_signal_connect (menu, "selection_done",
			  G_CALLBACK (action_selection_done_cb),
			  dialog);

	g_signal_connect (G_OBJECT (dialog->repeat_toggle), "toggled",
			  G_CALLBACK (repeat_toggle_toggled_cb), dialog);

	/* Handle custom sounds */
	g_signal_connect (G_OBJECT (dialog->aalarm_sound), "toggled",
			  G_CALLBACK (aalarm_sound_toggled_cb), dialog);
	g_signal_connect (G_OBJECT (dialog->aalarm_file_chooser), "selection-changed",
			  G_CALLBACK (aalarm_attach_changed_cb), dialog);

	/* Handle custom messages */
	g_signal_connect (G_OBJECT (dialog->dalarm_message), "toggled",
			  G_CALLBACK (dalarm_message_toggled_cb), dialog);
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
	g_signal_connect (G_OBJECT (text_buffer), "changed",
			  G_CALLBACK (dalarm_description_changed_cb), dialog);

	/* Handle program */
	g_signal_connect (G_OBJECT (dialog->palarm_program), "changed",
			  G_CALLBACK (palarm_program_changed_cb), dialog);

	/* Handle custom email */
	g_signal_connect (G_OBJECT (dialog->malarm_message), "toggled",
			  G_CALLBACK (malarm_message_toggled_cb), dialog);
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->malarm_description));
	g_signal_connect (G_OBJECT (text_buffer), "changed",
			  G_CALLBACK (malarm_description_changed_cb), dialog);

	g_signal_connect (dialog->malarm_addresses, "changed",
			  G_CALLBACK (malarm_addresses_changed_cb), dialog);
}

gboolean
alarm_dialog_run (GtkWidget *parent, ECal *ecal, ECalComponentAlarm *alarm)
{
	Dialog dialog;
	int response_id;
	GList *icon_list;
	char *gladefile;

	g_return_val_if_fail (alarm != NULL, FALSE);

	dialog.alarm = alarm;
	dialog.ecal = ecal;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "alarm-dialog.glade",
				      NULL);
	dialog.xml = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);
	if (!dialog.xml) {
		g_message (G_STRLOC ": Could not load the Glade XML file!");
		return FALSE;
	}

	if (!get_widgets (&dialog)) {
		g_object_unref(dialog.xml);
		return FALSE;
	}

	if (!setup_select_names (&dialog)) {
  		g_object_unref (dialog.xml);
  		return FALSE;
  	}

	init_widgets (&dialog);

	alarm_to_dialog (&dialog);

	gtk_widget_ensure_style (dialog.toplevel);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog.toplevel)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog.toplevel)->action_area), 12);

	icon_list = e_icon_factory_get_icon_list ("stock_calendar");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (dialog.toplevel), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}

	gtk_window_set_transient_for (GTK_WINDOW (dialog.toplevel),
				      GTK_WINDOW (parent));

	response_id = gtk_dialog_run (GTK_DIALOG (dialog.toplevel));

	if (response_id == GTK_RESPONSE_OK)
		dialog_to_alarm (&dialog);

	gtk_widget_destroy (dialog.toplevel);
	g_object_unref (dialog.xml);

	return response_id == GTK_RESPONSE_OK ? TRUE : FALSE;
}

 /* Evolution Email custom header options
 * Copyright (C) 2008 Novell, Inc.
 *
 * Authors: Ashish <shashish@novell.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, MA 02110-1301.
 */

#ifndef __EMAIL_CUSTOM_HEADEROPTIONS_DIALOG_H__
#define __EMAIL_CUSTOM_HEADEROPTIONS_DIALOG_H__

#include <gtk/gtkwidget.h>

#define EMAIL_CUSTOM_HEADER_OPTIONS_DIALOG       (epech_dialog_get_type ())
#define EMAIL_CUSTOM_HEADEROPTIONS_DIALOG(obj)       (GTK_CHECK_CAST ((obj), EMAIL_CUSTOM_HEADER_OPTIONS_DIALOG, CustomHeaderOptionsDialog))
#define EMAIL_CUSTOM_HEADEROPTIONS_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), EMAIL_CUSTOM_HEADER_OPTIONS_DIALOG, CustomHeaderOptionsDialogClass))
#define EMAIL_CUSTOM_HEADER_OPTIONS_IS_DIALOG(obj)    (GTK_CHECK_TYPE ((obj), EMAIL_CUSTOM_HEADER_OPTIONS_DIALOG))
#define EMAIL_CUSTOM_HEADER_OPTIONS_IS_DIALOG_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EMAIL_CUSTOM_HEADER_OPTIONS_DIALOG))

typedef struct _CustomHeaderOptionsDialog		CustomHeaderOptionsDialog;
typedef struct _CustomHeaderOptionsDialogClass		CustomHeaderOptionsDialogClass;
typedef struct _EmailCustomHeaderOptionsDialogPrivate	EmailCustomHeaderOptionsDialogPrivate;

struct _CustomHeaderOptionsDialog {
	GObject object;
	/* Private data */
	EmailCustomHeaderOptionsDialogPrivate *priv;
};

typedef struct {
	gint number_of_header;
	gint number_of_subtype_header;
	GString *header_type_value;
	GArray *sub_header_type_value;
} EmailCustomHeaderDetails;

typedef struct {
	GString *sub_header_string_value;
} CustomSubHeader;

typedef struct {
        GtkWidget *header_value_combo_box;
} HeaderValueComboBox;


struct _CustomHeaderOptionsDialogClass {
	GObjectClass parent_class;
	void (* emch_response) (CustomHeaderOptionsDialog *esd, gint status);
};

typedef struct _EmailCustomHeaderWindow
{
        GdkWindow *epech_window;
        CustomHeaderOptionsDialog *epech_dialog;
}EmailCustomHeaderWindow;

enum {
        MCH_RESPONSE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};
static GObjectClass *parent_class = NULL;

GType  epech_dialog_get_type     (void);
CustomHeaderOptionsDialog *epech_dialog_new (void);
static gboolean epech_dialog_run (CustomHeaderOptionsDialog *mch, GtkWidget *parent);
static void epech_get_header_list (CustomHeaderOptionsDialog *mch);
static void epech_load_from_gconf (GConfClient *client,const char *path,CustomHeaderOptionsDialog *mch);
#endif

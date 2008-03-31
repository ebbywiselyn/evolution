/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell-settings-dialog.c
 *
 * Copyright (C) 2002  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "e-shell-settings-dialog.h"

#include "e-corba-config-page.h"
#include <e-util/e-icon-factory.h>

#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-exception.h>

#include <bonobo-activation/bonobo-activation.h>

#include <string.h>

struct _EShellSettingsDialogPrivate {
	GHashTable *types;
};

G_DEFINE_TYPE (EShellSettingsDialog, e_shell_settings_dialog, E_TYPE_MULTI_CONFIG_DIALOG)


/* FIXME ugly hack to work around that sizing of invisible widgets is broken
   with Bonobo.  */

static void
set_dialog_size (EShellSettingsDialog *dialog)
{
	PangoLayout *layout;
	PangoContext *context;
	PangoFontMetrics *metrics;
	int width, height;

	layout = gtk_widget_create_pango_layout (GTK_WIDGET (dialog), "M");
	context = pango_layout_get_context (layout);
	metrics = pango_context_get_metrics (context,
					     gtk_widget_get_style (GTK_WIDGET (dialog))->font_desc,
					     pango_context_get_language (context));

	pango_layout_get_pixel_size (layout, &width, NULL);

	width *= 60;
	height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics)
			       + pango_font_metrics_get_descent (metrics)) * 30;

	gtk_window_set_default_size((GtkWindow *)dialog, width, height);
	g_object_unref (layout);
	pango_font_metrics_unref (metrics);
}


/* Page handling.  */

struct _Page {
	char *title;
	char *description;
	GdkPixbuf *icon;
	Bonobo_ActivationProperty *type;
	int priority;
	EConfigPage *page_widget;
};
typedef struct _Page Page;

static Page *
page_new (const char *title,
	  const char *description,
	  GdkPixbuf *icon,
	  Bonobo_ActivationProperty *type,
	  int priority,
	  EConfigPage *page_widget)
{
	Page *page;

	if (icon != NULL)
		g_object_ref (icon);

	page = g_new (Page, 1);
	page->title       = g_strdup (title);
	page->description = g_strdup (description);
	page->icon        = icon;
	page->type        = type;
	page->priority    = priority;
	page->page_widget = page_widget;

	return page;
}

static void
page_free (Page *page)
{
	g_free (page->title);
	g_free (page->description);

	if (page->icon != NULL)
		g_object_unref (page->icon);

	g_free (page);
}

static int
compare_page_func (const void *a,
		   const void *b)
{
	const Page *page_a;
	const Page *page_b;

	page_a = (const Page *) a;
	page_b = (const Page *) b;

	if (page_a->priority == page_b->priority)
		return strcmp (page_a->title, page_b->title);

	return page_a->priority - page_b->priority;
}

static GList *
sort_page_list (GList *list)
{
	return g_list_sort (list, compare_page_func);
}

static void
load_pages (EShellSettingsDialog *dialog)
{
	EShellSettingsDialogPrivate *priv;
	Bonobo_ServerInfoList *control_list;
	const gchar * const *language_names;
	CORBA_Environment ev;
	GSList *languages = NULL;
	GList *page_list;
	GList *p;
	int i, j;

	priv = dialog->priv;

	CORBA_exception_init (&ev);

	control_list = bonobo_activation_query ("repo_ids.has('IDL:GNOME/Evolution/ConfigControl:" BASE_VERSION "')", NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || control_list == NULL) {
		g_warning ("Cannot load configuration pages -- %s", BONOBO_EX_REPOID (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	language_names = g_get_language_names ();
	while (*language_names != NULL)
		languages = g_slist_append (languages, (gpointer)(*language_names++));

	page_list = NULL;
	for (i = 0; i < control_list->_length; i ++) {
		CORBA_Object corba_object;
		Bonobo_ServerInfo *info;
		const char *title;
		const char *description;
		const char *icon_path;
		const char *priority_string;
		Bonobo_ActivationProperty *type;
		int priority;
		GdkPixbuf *icon;

		CORBA_exception_init (&ev);

		info = & control_list->_buffer[i];

		title       	= bonobo_server_info_prop_lookup (info, "evolution2:config_item:title", languages);
		description 	= bonobo_server_info_prop_lookup (info, "evolution2:config_item:description", languages);
		icon_path   	= bonobo_server_info_prop_lookup (info, "evolution2:config_item:icon_name", NULL);
		type            = bonobo_server_info_prop_find   (info, "evolution2:config_item:type");
		priority_string = bonobo_server_info_prop_lookup (info, "evolution2:config_item:priority", NULL);

		if (icon_path == NULL) {
			icon = NULL;
		} else {
			if (g_path_is_absolute (icon_path)) {
				icon = gdk_pixbuf_new_from_file (icon_path, NULL);
			} else {
				icon = e_icon_factory_get_icon (icon_path, E_ICON_SIZE_DIALOG);
			}
		}

		if (type != NULL && type->v._d != Bonobo_ACTIVATION_P_STRINGV)
			type = NULL;
		if (priority_string == NULL)
			priority = 0xffff;
		else
			priority = atoi (priority_string);

		corba_object = bonobo_activation_activate_from_id ((char *) info->iid, 0, NULL, &ev);

		if (! BONOBO_EX (&ev)) {
			Page *page;

			page = page_new (title, description, icon, type, priority,
					 E_CONFIG_PAGE (e_corba_config_page_new_from_objref (corba_object)));

			page_list = g_list_prepend (page_list, page);
		} else {
			char *bonobo_ex_text = bonobo_exception_get_text (&ev);
			g_warning ("Cannot activate %s -- %s", info->iid, bonobo_ex_text);
			g_free (bonobo_ex_text);
		}

		if (icon != NULL)
			g_object_unref (icon);

		CORBA_exception_free (&ev);
	}
	g_slist_free(languages);

	page_list = sort_page_list (page_list);
	for (p = page_list, i = 0; p != NULL; p = p->next, i++) {
		Page *page;

		page = (Page *) p->data;

		e_multi_config_dialog_add_page (E_MULTI_CONFIG_DIALOG (dialog),
						page->title,
						page->description,
						page->icon,
						page->page_widget);

		if (page->type != NULL) {
			Bonobo_StringList list = page->type->v._u.value_stringv;

			for (j = 0; j < list._length; j++) {
				if (g_hash_table_lookup (priv->types, list._buffer[j]) == NULL)
					g_hash_table_insert (priv->types, g_strdup (list._buffer[j]),
							     GINT_TO_POINTER (i));
			}
		}


		page_free (page);
	}

	g_list_free (page_list);
	CORBA_free (control_list);
}


/* GtkObject methods.  */

static void
impl_finalize (GObject *object)
{
	EShellSettingsDialog *dialog;
	EShellSettingsDialogPrivate *priv;

	dialog = E_SHELL_SETTINGS_DIALOG (object);
	priv = dialog->priv;

	g_hash_table_destroy (priv->types);

	g_free (priv);

	(* G_OBJECT_CLASS (e_shell_settings_dialog_parent_class)->finalize) (object);
}


static void
e_shell_settings_dialog_class_init (EShellSettingsDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = impl_finalize;
}

static void
e_shell_settings_dialog_init (EShellSettingsDialog *dialog)
{
	EShellSettingsDialogPrivate *priv;

	priv = g_new (EShellSettingsDialogPrivate, 1);
	priv->types = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

	dialog->priv = priv;

	load_pages (dialog);
	set_dialog_size (dialog);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Evolution Preferences"));
	gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
}


GtkWidget *
e_shell_settings_dialog_new (void)
{
	EShellSettingsDialog *new;

	new = g_object_new (e_shell_settings_dialog_get_type (), NULL);

	return GTK_WIDGET (new);
}

void
e_shell_settings_dialog_show_type (EShellSettingsDialog *dialog, const char *type)
{
	EShellSettingsDialogPrivate *priv;
	gpointer key, value;
	int page;

	g_return_if_fail (dialog != NULL);
	g_return_if_fail (E_IS_SHELL_SETTINGS_DIALOG (dialog));
	g_return_if_fail (type != NULL);

	priv = dialog->priv;

	if (!g_hash_table_lookup_extended (priv->types, type, &key, &value)) {
		char *slash, *supertype;

		slash = strchr (type, '/');
		if (slash) {
			supertype = g_strndup (type, slash - type);
			value = g_hash_table_lookup (priv->types, type);
			g_free (supertype);
		} else
			value = NULL;
	}
	page = GPOINTER_TO_INT (value);

	e_multi_config_dialog_show_page (E_MULTI_CONFIG_DIALOG (dialog), page);
}



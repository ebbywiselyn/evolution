/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Copyright (C) 2000-2002 Ximian Inc.
 *
 *  Authors: Not Zed <notzed@lostzed.mmc.com.au>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "e-util/e-util-private.h"

#include "em-filter-editor.h"
#include "em-filter-rule.h"

#define d(x)

static FilterRule *create_rule (RuleEditor *re);

static void em_filter_editor_class_init (EMFilterEditorClass *klass);
static void em_filter_editor_init (EMFilterEditor *fe);
static void em_filter_editor_finalise (GObject *obj);


static RuleEditorClass *parent_class = NULL;


GtkType
em_filter_editor_get_type (void)
{
	static GtkType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMFilterEditorClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_filter_editor_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMFilterEditor),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_filter_editor_init,
		};

		type = g_type_register_static (RULE_TYPE_EDITOR, "EMFilterEditor", &info, 0);
	}

	return type;
}

static void
em_filter_editor_class_init (EMFilterEditorClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	RuleEditorClass *re_class = (RuleEditorClass *) klass;

	parent_class = g_type_class_ref (rule_editor_get_type ());

	gobject_class->finalize = em_filter_editor_finalise;

	/* override methods */
	re_class->create_rule = create_rule;
}

static void
em_filter_editor_init (EMFilterEditor *fe)
{
	;
}

static void
em_filter_editor_finalise (GObject *obj)
{
        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * em_filter_editor_new:
 *
 * Create a new EMFilterEditor object.
 *
 * Return value: A new #EMFilterEditor object.
 **/
EMFilterEditor *
em_filter_editor_new (EMFilterContext *fc, const EMFilterSource *source_names)
{
	EMFilterEditor *fe = (EMFilterEditor *) g_object_new (em_filter_editor_get_type(), NULL);
	GladeXML *gui;
	char *gladefile;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "filter.glade",
				      NULL);
	gui = glade_xml_new (gladefile, "rule_editor", NULL);
	g_free (gladefile);

	em_filter_editor_construct (fe, fc, gui, source_names);
	g_object_unref (gui);

	return fe;
}

static void
select_source (GtkMenuItem *mi, EMFilterEditor *fe)
{
	char *source;

	source = g_object_get_data(G_OBJECT(mi), "source");
	g_return_if_fail (source);

	rule_editor_set_source ((RuleEditor *)fe, source);
}

void
em_filter_editor_construct (EMFilterEditor *fe, EMFilterContext *fc, GladeXML *gui, const EMFilterSource *source_names)
{
	GtkWidget *menu, *item, *omenu;
	int i;

        omenu = glade_xml_get_widget (gui, "filter_source");
	gtk_option_menu_remove_menu (GTK_OPTION_MENU (omenu));
	menu = gtk_menu_new ();

	for (i = 0; source_names[i].source; i++) {
		item = gtk_menu_item_new_with_label(source_names[i].name);
		g_object_set_data_full((GObject *)item, "source", g_strdup(source_names[i].source), g_free);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show (item);
		g_signal_connect (item, "activate", G_CALLBACK (select_source), fe);
	}
	gtk_option_menu_set_menu (GTK_OPTION_MENU (omenu), menu);
	gtk_widget_show (omenu);

	rule_editor_construct ((RuleEditor *) fe, (RuleContext *) fc, gui, source_names[0].source, _("_Filter Rules"));
}

static FilterRule *
create_rule (RuleEditor *re)
{
	FilterRule *rule = filter_rule_new ();
	FilterPart *part;

	/* create a rule with 1 part & 1 action in it */
	rule = (FilterRule *)em_filter_rule_new ();
	part = rule_context_next_part (re->context, NULL);
	filter_rule_add_part (rule, filter_part_clone (part));
	part = em_filter_context_next_action ((EMFilterContext *)re->context, NULL);
	em_filter_rule_add_action ((EMFilterRule *)rule, filter_part_clone (part));

	return rule;
}

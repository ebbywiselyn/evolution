/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-addressbook-export.c
 *
 * Copyright (C) 2003 Ximian, Inc.
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
 *
 * Author: Gilbert Fang <gilbert.fang@sun.com>
 *
 */

#include <config.h>

#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-main.h>
#include <gnome.h>

#include <libebook/e-book.h>

#include "evolution-addressbook-export.h"

/* Command-Line Options */
static gchar *opt_output_file = NULL;
static gboolean opt_list_folders_mode = FALSE;
static gchar *opt_output_format = NULL;
static gchar *opt_addressbook_folder_uri = NULL;
static gboolean opt_async_mode = FALSE;
static gint opt_file_size = 0;
static gchar **opt_remaining = NULL;

static GOptionEntry entries[] = {
	{ "output", '\0', G_OPTION_FLAG_FILENAME,
	  G_OPTION_ARG_STRING, &opt_output_file,
	  N_("Specify the output file instead of standard output"),
	  N_("OUTPUTFILE") },
	{ "list-addressbook-folders", 'l', 0,
	  G_OPTION_ARG_NONE, &opt_list_folders_mode,
	  N_("List local addressbook folders") },
	{ "format", '\0', 0,
	  G_OPTION_ARG_STRING, &opt_output_format,
	  N_("Show cards as vcard or csv file"),
	  N_("[vcard|csv]") },
	{ "async", 'a', 0,
	  G_OPTION_ARG_NONE, &opt_async_mode,
	  N_("Export in asynchronous mode") },
	{ "size", '\0', 0,
	  G_OPTION_ARG_INT, &opt_file_size,
	  N_("The number of cards in one output file in asynchronous mode, "
	     "default size 100."),
	  N_("NUMBER") },
	{ G_OPTION_REMAINING, '\0', 0,
	  G_OPTION_ARG_STRING_ARRAY, &opt_remaining },
	{ NULL }
};

int
main (int argc, char **argv)
{
	ActionContext actctx;
	GnomeProgram *program;
	GOptionContext *context;

	int current_action = ACTION_NOTHING;
	int IsCSV = FALSE;
	int IsVCard = FALSE;

	/*i18n-lize */
	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	program = gnome_program_init (
		PACKAGE, VERSION, GNOME_BONOBO_MODULE, argc, argv,
		GNOME_PARAM_GOPTION_CONTEXT, context,
		GNOME_PARAM_NONE);

	/* Parsing Parameter */
	if (opt_remaining && g_strv_length (opt_remaining) > 0)
		opt_addressbook_folder_uri = g_strdup (opt_remaining[0]);

	if (opt_list_folders_mode != FALSE) {
		current_action = ACTION_LIST_FOLDERS;
		/* check there should not be addressbook-folder-uri , and async and size , output_format */
		if (opt_addressbook_folder_uri != NULL || opt_async_mode != FALSE || opt_output_format != NULL || opt_file_size != 0) {
			g_warning (_("Command line arguments error, please use --help option to see the usage."));
			exit (-1);
		}
	} else {

		current_action = ACTION_LIST_CARDS;

		/* check the output format */
		if (opt_output_format == NULL) {
			IsVCard = TRUE;
		} else {
			IsCSV = !strcmp (opt_output_format, "csv");
			IsVCard = !strcmp (opt_output_format, "vcard");
			if (IsCSV == FALSE && IsVCard == FALSE) {
				g_warning (_("Only support csv or vcard format."));
				exit (-1);
			}
		}

		/*check async and output file */
		if (opt_async_mode == TRUE) {
			/* check have to output file , set default file_size */
			if (opt_output_file == NULL) {
				g_warning (_("In async mode, output must be file."));
				exit (-1);
			}
			if (opt_file_size == 0)
				opt_file_size = DEFAULT_SIZE_NUMBER;
		} else {
			/*check no file_size */
			if (opt_file_size != 0) {
				g_warning (_("In normal mode, there is no need for the size option."));
				exit (-1);
			}
		}
	}

	/* do actions */
	if (current_action == ACTION_LIST_FOLDERS) {
		actctx.action_type = current_action;
		if (opt_output_file == NULL) {
			actctx.action_list_folders.output_file = NULL;
		} else {
			actctx.action_list_folders.output_file = g_strdup (opt_output_file);
		}
		action_list_folders_init (&actctx);
	} else if (current_action == ACTION_LIST_CARDS) {
		actctx.action_type = current_action;
		if (opt_output_file == NULL) {
			actctx.action_list_cards.output_file = NULL;
		} else {
			actctx.action_list_cards.output_file = g_strdup (opt_output_file);
		}
		actctx.action_list_cards.IsCSV = IsCSV;
		actctx.action_list_cards.IsVCard = IsVCard;
		actctx.action_list_cards.addressbook_folder_uri = g_strdup (opt_addressbook_folder_uri);
		actctx.action_list_cards.async_mode = opt_async_mode;
		actctx.action_list_cards.file_size = opt_file_size;

		action_list_cards_init (&actctx);

	} else {
		g_warning (_("Unhandled error"));
		exit (-1);
	}

	/*FIXME:should free actctx's some char* field, such as output_file! but since the program will end, so that will not cause mem leak.  */

	exit (0);
}

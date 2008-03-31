/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002-2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __EM_MAILER_PREFS_H__
#define __EM_MAILER_PREFS_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <gtk/gtkvbox.h>
#include <shell/Evolution.h>

struct _ESignature;
struct _GtkToggleButton;
struct _GtkOptionMenu;
struct _GdkPixbuf;
struct _GtkWidget;
struct _GladeXML;
struct _GtkFileChooserbutton;
struct _GtkFontButton;
struct _GConfClient;
struct _GtkButton;
struct _GtkTreeView;
struct _GtkWindow;

#define EM_MAILER_PREFS_TYPE        (em_mailer_prefs_get_type ())
#define EM_MAILER_PREFS(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), EM_MAILER_PREFS_TYPE, EMMailerPrefs))
#define EM_MAILER_PREFS_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), EM_MAILER_PREFS_TYPE, EMMailerPrefsClass))
#define EM_IS_MAILER_PREFS(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), EM_MAILER_PREFS_TYPE))
#define EM_IS_MAILER_PREFS_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), EM_MAILER_PREFS_TYPE))

typedef struct _EMMailerPrefs EMMailerPrefs;
typedef struct _EMMailerPrefsClass EMMailerPrefsClass;
typedef struct _EMMailerPrefsHeader EMMailerPrefsHeader;

struct _EMMailerPrefsHeader {
	char *name;
	guint enabled:1;
	guint is_default:1;
};

struct _EMMailerPrefs {
	GtkVBox parent_object;

	GNOME_Evolution_Shell shell;

	struct _GladeXML *gui;
	struct _GConfClient *gconf;

	/* General tab */

	/* Message Display */
	struct _GtkToggleButton *timeout_toggle;
	struct _GtkSpinButton *timeout;
	struct _GtkToggleButton *address_toggle;
	struct _GtkSpinButton *address_count;
	struct _GtkToggleButton *mlimit_toggle;
	struct _GtkSpinButton *mlimit_count;
	struct _GtkOptionMenu *charset;
	struct _GtkToggleButton *citation_highlight;
	struct _GtkColorButton *citation_color;
	struct _GtkToggleButton *enable_search_folders;
	struct _GtkToggleButton *magic_spacebar;

	/* Deleting Mail */
	struct _GtkToggleButton *empty_trash;
	struct _GtkOptionMenu *empty_trash_days;
	struct _GtkToggleButton *confirm_expunge;

	/* HTML Mail tab */
	struct _GtkFontButton *font_variable;
	struct _GtkFontButton *font_fixed;
	struct _GtkToggleButton *font_share;

	/* Loading Images */
	struct _GtkToggleButton *images_always;
	struct _GtkToggleButton *images_sometimes;
	struct _GtkToggleButton *images_never;

	struct _GtkToggleButton *show_animated;
	struct _GtkToggleButton *autodetect_links;
	struct _GtkToggleButton *prompt_unwanted_html;

	/* Labels and Colours tab */
	struct _GtkWidget *label_add;
	struct _GtkWidget *label_edit;
	struct _GtkWidget *label_remove;
	struct _GtkWidget *label_tree;
	struct _GtkListStore *label_list_store;
	guint labels_change_notify_id; /* mail_config's notify id */

	/* Headers tab */
	struct _GtkButton *add_header;
	struct _GtkButton *remove_header;
	struct _GtkEntry *entry_header;
	struct _GtkTreeView *header_list;
	struct _GtkListStore *header_list_store;
	struct _GtkToggleButton *photo_show;
	struct _GtkToggleButton *photo_local;

	/* Junk prefs */
	struct _GtkToggleButton *check_incoming;
	struct _GtkToggleButton *empty_junk;
	struct _GtkOptionMenu *empty_junk_days;
	
	struct _GtkToggleButton *sa_local_tests_only;
	struct _GtkToggleButton *sa_use_daemon;
	struct _GtkComboBox *default_junk_plugin;
	struct _GtkLabel *plugin_status;
	struct _GtkImage *plugin_image;

	struct _GtkToggleButton *junk_header_check;
	struct _GtkTreeView *junk_header_tree;
	struct _GtkListStore *junk_header_list_store;	
	struct _GtkButton *junk_header_add;
	struct _GtkButton *junk_header_remove;
	struct _GtkToggleButton *junk_book_lookup;

};

struct _EMMailerPrefsClass {
	GtkVBoxClass parent_class;

	/* signals */

};

GtkType em_mailer_prefs_get_type (void);
GtkWidget * create_combo_text_widget (void);

struct _GtkWidget *em_mailer_prefs_new (void);

EMMailerPrefsHeader *em_mailer_prefs_header_from_xml(const char *xml);
char *em_mailer_prefs_header_to_xml(EMMailerPrefsHeader *header);
void em_mailer_prefs_header_free(EMMailerPrefsHeader *header);

/* needed by global config */
#define EM_MAILER_PREFS_CONTROL_ID "OAFIID:GNOME_Evolution_Mail_MailerPrefs_ConfigControl:" BASE_VERSION

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EM_MAILER_PREFS_H__ */

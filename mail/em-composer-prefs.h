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

#ifndef __EM_COMPOSER_PREFS_H__
#define __EM_COMPOSER_PREFS_H__

#include <gtk/gtk.h>

/* Standard GObject macros */
#define EM_TYPE_COMPOSER_PREFS \
	(em_composer_prefs_get_type ())
#define EM_COMPOSER_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), EM_TYPE_COMPOSER_PREFS, EMComposerPrefs))
#define EM_COMPOSER_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), EM_TYPE_COMPOSER_PREFS, EMComposerPrefsClass))
#define EM_IS_COMPOSER_PREFS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), EM_TYPE_COMPOSER_PREFS))
#define EM_IS_COMPOSER_PREFS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), EM_TYPE_COMPOSER_PREFS))
#define EM_COMPOSER_PREFS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), EM_TYPE_COMPOSER_PREFS, EMComposerPrefsClass))

G_BEGIN_DECLS

typedef struct _EMComposerPrefs EMComposerPrefs;
typedef struct _EMComposerPrefsClass EMComposerPrefsClass;

struct _ESignature;
struct _GladeXML;

struct _EMComposerPrefs {
	GtkVBox parent;

	struct _GladeXML *gui;

	/* General tab */

	/* Default Behavior */
	GtkOptionMenu *charset;

	GtkColorButton *color;
	GtkTreeModel *language_model;

	/* Forwards and Replies */
	GtkOptionMenu *forward_style;
	GtkOptionMenu *reply_style;

	/* Keyboard Shortcuts */
	GtkOptionMenu *shortcuts_type;

	/* Signatures */
	GtkTreeView *sig_list;
	GHashTable *sig_hash;
	GtkButton *sig_add;
	GtkButton *sig_add_script;
	GtkButton *sig_edit;
	GtkButton *sig_delete;
	struct _GtkHTML *sig_preview;

	struct _GladeXML *sig_script_gui;
	GtkWidget *sig_script_dialog;

	guint sig_added_id;
	guint sig_removed_id;
	guint sig_changed_id;
};

struct _EMComposerPrefsClass {
	GtkVBoxClass parent_class;
};

GType		em_composer_prefs_get_type	(void);
GtkWidget *	em_composer_prefs_new		(void);
void		em_composer_prefs_new_signature (GtkWindow *parent,
						 gboolean html_mode);

/* needed by global config */
#define EM_COMPOSER_PREFS_CONTROL_ID \
	"OAFIID:GNOME_Evolution_Mail_ComposerPrefs_ConfigControl:" BASE_VERSION

G_END_DECLS

#endif /* __EM_COMPOSER_PREFS_H__ */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Vivek Jain <jvivek@novell.com>
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

#ifndef __SHARE_FOLDER_H__
#define __SHARE_FOLDER_H__

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtk.h>
#include <camel/camel-store.h>
#include <e-gw-connection.h>
#include <libedataserverui/e-name-selector.h>

#define _SHARE_FOLDER_TYPE    	      (share_folder_get_type ())
#define SHARE_FOLDER(o)          (G_TYPE_CHECK_INSTANCE_CAST ((o), SHARE_FOLDER, ShareFolder))
#define SHARE_FOLDER_CLASS(k)    (G_TYPE_CHECK_CLASS_CAST ((k), SHARE_FOLDER_TYPE, ShareFolder))
#define IS_SHARE_FOLDER(o)       (G_TYPE_CHECK_INSTANCE_TYPE ((o), SHARE_FOLDER_TYPE))
#define IS_SHARE_FOLDER_CLASS(k) (G_TYPE_CHECK_CLASS_TYPE ((k), SHARE_FOLDER_TYPE))

typedef struct _ShareFolder ShareFolder;
typedef struct _ShareFolderClass ShareFolderClass;

struct _GtkWidget;
struct _GladeXML;
struct _GtkButton;
struct _GtkTreeView;
struct _GtkLabel;
struct _GtkEntry;
struct _GtkWindow;
struct _GtkRadioButton;
struct _GtkListStore;
struct _GtkCellRenderer;
struct _GtkTreeViewColumn;
struct _GtkFrame;
struct _GtkVBox;

struct _ShareFolder {
	GtkVBox parent_object;

	struct _GladeXML *xml;

	/* General tab */

	/* Default Behavior */
	struct _GtkTreeView *user_list;
	struct _GtkTextView *message;
	struct _GtkButton *add_button;
	struct _GtkButton *remove;
	struct _GtkButton *add_book;
	struct _GtkButton *notification;
	struct _GtkEntry *name;
	struct _GtkEntry *subject;
	struct _GtkRadioButton *shared;
	struct _GtkRadioButton *not_shared;
	struct _GtkWidget *scrolled_window;
	struct _GtkListStore *model;
	struct _GtkCellRenderer *cell;
	struct _GtkTreeViewColumn *column;
	struct _GtkVBox  *vbox;
	struct _GtkVBox  *table;
	struct _GtkWidget *window;

	GList *users_list;
	EGwContainer *gcontainer;
	gint users;
	gboolean byme;
	gboolean tome;
	gint flag_for_ok;
	gchar *email;
	gboolean is_shared;
	EGwConnection *cnc;
	gchar *container_id;
	gchar *sub;
	gchar *mesg;
	GList *container_list;
	GtkTreeIter iter;
	ENameSelector *name_selector;

};

struct _ShareFolderClass {
	GtkVBoxClass parent_class;

};

GType share_folderget_type (void);
struct _ShareFolder * share_folder_new (EGwConnection *ccnc, gchar *id);
void share_folder(struct _ShareFolder *sf);
gchar * get_container_id (EGwConnection *cnc, gchar *fname);
EGwConnection * get_cnc (CamelStore *store);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __SHARE_FOLDER_H__ */

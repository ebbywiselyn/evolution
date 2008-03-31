/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <libxml/parser.h>

#include <libedataserver/e-xml-utils.h>
#include <libedataserver/e-data-server-util.h>

#include <e-util/e-mktemp.h>

#include <glib/gi18n.h>

#include <camel/camel-file-utils.h>

#include "mail-config.h"
#include "mail-session.h"
#include "mail-tools.h"
#include "mail-mt.h"

/* sigh, these 2 only needed for outbox total count checking - a mess */
#include "mail-component.h"
#include "mail-folder-cache.h"

#include "em-utils.h"

#include <camel/camel-folder.h>
#include <camel/camel-vee-store.h>

#include "em-marshal.h"
#include "em-folder-tree-model.h"

#define u(x)			/* unread count debug */
#define d(x)

static GType col_types[] = {
	G_TYPE_STRING,   /* display name */
	G_TYPE_POINTER,  /* store object */
	G_TYPE_STRING,   /* path */
	G_TYPE_STRING,   /* uri */
	G_TYPE_UINT,     /* unread count */
	G_TYPE_UINT,     /* flags */
	G_TYPE_BOOLEAN,  /* is a store node */
	G_TYPE_BOOLEAN,  /* has not-yet-loaded subfolders */
};

/* GObject virtual method overrides */
static void em_folder_tree_model_class_init (EMFolderTreeModelClass *klass);
static void em_folder_tree_model_init (EMFolderTreeModel *model);
static void em_folder_tree_model_finalize (GObject *obj);

/* interface init methods */
static void tree_model_iface_init (GtkTreeModelIface *iface);
static void tree_sortable_iface_init (GtkTreeSortableIface *iface);

static void account_changed (EAccountList *accounts, EAccount *account, gpointer user_data);
static void account_removed (EAccountList *accounts, EAccount *account, gpointer user_data);

enum {
	LOADING_ROW,
	LOADED_ROW,
	FOLDER_ADDED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };
static GtkTreeStoreClass *parent_class = NULL;

GType
em_folder_tree_model_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMFolderTreeModelClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_folder_tree_model_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMFolderTreeModel),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_folder_tree_model_init,
		};
		static const GInterfaceInfo tree_model_info = {
			(GInterfaceInitFunc) tree_model_iface_init,
			NULL,
			NULL
		};
		static const GInterfaceInfo sortable_info = {
			(GInterfaceInitFunc) tree_sortable_iface_init,
			NULL,
			NULL
		};

		type = g_type_register_static (GTK_TYPE_TREE_STORE, "EMFolderTreeModel", &info, 0);

		g_type_add_interface_static (type, GTK_TYPE_TREE_MODEL,
					     &tree_model_info);
		g_type_add_interface_static (type, GTK_TYPE_TREE_SORTABLE,
					     &sortable_info);
	}

	return type;
}


static void
em_folder_tree_model_class_init (EMFolderTreeModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (GTK_TYPE_TREE_STORE);

	object_class->finalize = em_folder_tree_model_finalize;

	/* signals */
	signals[LOADING_ROW] =
		g_signal_new ("loading-row",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, loading_row),
			      NULL, NULL,
			      em_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);

	signals[LOADED_ROW] =
		g_signal_new ("loaded-row",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, loaded_row),
			      NULL, NULL,
			      em_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2,
			      G_TYPE_POINTER,
			      G_TYPE_POINTER);

	signals[FOLDER_ADDED] =
		g_signal_new ("folder-added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeModelClass, folder_added),
			      NULL, NULL,
			      em_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);
}

static int
sort_cb (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	extern CamelStore *vfolder_store;
	char *aname, *bname;
	CamelStore *store;
	gboolean is_store;
	guint32 aflags, bflags;
	int rv = -2;

	gtk_tree_model_get (model, a, COL_BOOL_IS_STORE, &is_store,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_DISPLAY_NAME, &aname, COL_UINT_FLAGS, &aflags, -1);
	gtk_tree_model_get (model, b, COL_STRING_DISPLAY_NAME, &bname, COL_UINT_FLAGS, &bflags, -1);

	if (is_store) {
		/* On This Computer is always first and Search Folders is always last */
		if (!strcmp (aname, _("On This Computer")))
			rv = -1;
		else if (!strcmp (bname, _("On This Computer")))
			rv = 1;
		else if (!strcmp (aname, _("Search Folders")))
			rv = 1;
		else if (!strcmp (bname, _("Search Folders")))
			rv = -1;
	} else if (store == vfolder_store) {
		/* UNMATCHED is always last */
		if (aname && !strcmp (aname, _("UNMATCHED")))
			rv = 1;
		else if (bname && !strcmp (bname, _("UNMATCHED")))
			rv = -1;
	} else {
		/* Inbox is always first */
		if ((aflags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
			rv = -1;
		else if ((bflags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
			rv = 1;
	}

	if (aname == NULL) {
		if (bname == NULL)
			rv = 0;
	} else if (bname == NULL)
		rv = 1;

	if (rv == -2)
		rv = g_utf8_collate (aname, bname);

	g_free (aname);
	g_free (bname);

	return rv;
}

static void
store_info_free (struct _EMFolderTreeModelStoreInfo *si)
{
	camel_object_remove_event (si->store, si->created_id);
	camel_object_remove_event (si->store, si->deleted_id);
	camel_object_remove_event (si->store, si->renamed_id);
	camel_object_remove_event (si->store, si->subscribed_id);
	camel_object_remove_event (si->store, si->unsubscribed_id);

	g_free (si->display_name);
	camel_object_unref (si->store);
	gtk_tree_row_reference_free (si->row);
	g_hash_table_destroy (si->full_hash);
	g_free (si);
}

static void
emft_model_unread_count_changed (GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreeIter parent_iter;
	GtkTreeIter child_iter = *iter;

	g_signal_handlers_block_by_func (
		model, emft_model_unread_count_changed, NULL);

	/* Folders are displayed with a bold weight to indicate that
	   they contain unread messages.  We signal that parent rows
	   have changed here to update them. */

	while (gtk_tree_model_iter_parent (model, &parent_iter, &child_iter)) {
		GtkTreePath *parent_path;

		parent_path = gtk_tree_model_get_path (model, &parent_iter);
		gtk_tree_model_row_changed (model, parent_path, &parent_iter);
		gtk_tree_path_free (parent_path);
		child_iter = parent_iter;
	}

	g_signal_handlers_unblock_by_func (
		model, emft_model_unread_count_changed, NULL);
}

static void
em_folder_tree_model_init (EMFolderTreeModel *model)
{
	model->store_hash = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) store_info_free);

	model->uri_hash = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) gtk_tree_row_reference_free);

	gtk_tree_sortable_set_default_sort_func ((GtkTreeSortable *) model, sort_cb, NULL, NULL);

	model->accounts = mail_config_get_accounts ();
	model->account_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	model->account_changed_id = g_signal_connect (model->accounts, "account-changed", G_CALLBACK (account_changed), model);
	model->account_removed_id = g_signal_connect (model->accounts, "account-removed", G_CALLBACK (account_removed), model);
	//g_signal_connect (model, "row-changed", G_CALLBACK (emft_model_unread_count_changed), NULL);
}

static void
em_folder_tree_model_finalize (GObject *obj)
{
	EMFolderTreeModel *model = (EMFolderTreeModel *) obj;

	g_free (model->filename);
	if (model->state)
		xmlFreeDoc (model->state);

	g_hash_table_destroy (model->store_hash);
	g_hash_table_destroy (model->uri_hash);

	g_hash_table_destroy (model->account_hash);
	g_signal_handler_disconnect (model->accounts, model->account_changed_id);
	g_signal_handler_disconnect (model->accounts, model->account_removed_id);

	G_OBJECT_CLASS (parent_class)->finalize (obj);
}


static void
tree_model_iface_init (GtkTreeModelIface *iface)
{
	;
}

static void
tree_sortable_iface_init (GtkTreeSortableIface *iface)
{
	;
}


static void
em_folder_tree_model_load_state (EMFolderTreeModel *model, const char *filename)
{
	xmlNodePtr root, node;

	if (model->state)
		xmlFreeDoc (model->state);

	if ((model->state = e_xml_parse_file (filename)) != NULL) {
		node = xmlDocGetRootElement (model->state);
		if (!node || strcmp ((char *)node->name, "tree-state") != 0) {
			/* it is not expected XML file, thus free it and use the default */
			xmlFreeDoc (model->state);
		} else
			return;
	}

	/* setup some defaults - expand "Local Folders" and "Search Folders" */
	model->state = xmlNewDoc ((const unsigned char *)"1.0");
	root = xmlNewDocNode (model->state, NULL, (const unsigned char *)"tree-state", NULL);
	xmlDocSetRootElement (model->state, root);

	node = xmlNewChild (root, NULL, (const unsigned char *)"node", NULL);
	xmlSetProp (node, (const unsigned char *)"name", (const unsigned char *)"local");
	xmlSetProp (node, (const unsigned char *)"expand", (const unsigned char *)"true");

	node = xmlNewChild (root, NULL, (const unsigned char *)"node", NULL);
	xmlSetProp (node, (const unsigned char *)"name", (const unsigned char *)"vfolder");
	xmlSetProp (node, (const unsigned char *)"expand", (const unsigned char *)"true");
}


EMFolderTreeModel *
em_folder_tree_model_new (const char *evolution_dir)
{
	EMFolderTreeModel *model;
	char *filename;

	model = g_object_new (EM_TYPE_FOLDER_TREE_MODEL, NULL);
	gtk_tree_store_set_column_types ((GtkTreeStore *) model, NUM_COLUMNS, col_types);
	gtk_tree_sortable_set_sort_column_id ((GtkTreeSortable *) model,
					      GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
					      GTK_SORT_ASCENDING);

	filename = g_build_filename (evolution_dir, "mail", "config", "folder-tree-expand-state.xml", NULL);
	em_folder_tree_model_load_state (model, filename);
	model->filename = filename;

	return model;
}


static void
account_changed (EAccountList *accounts, EAccount *account, gpointer user_data)
{
	EMFolderTreeModel *model = user_data;
	struct _EMFolderTreeModelStoreInfo *si;
	CamelProvider *provider;
	CamelStore *store;
	CamelException ex;
	char *uri;

	if (!(si = g_hash_table_lookup (model->account_hash, account)))
		return;

	em_folder_tree_model_remove_store (model, si->store);

	/* check if store needs to be added at all*/
	if (!account->enabled ||!(uri = account->source->url))
		return;

	camel_exception_init (&ex);
	if (!(provider = camel_provider_get(uri, &ex))) {
		camel_exception_clear (&ex);
		return;
	}

	/* make sure the new store belongs in the tree */
	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		camel_exception_clear (&ex);
		return;
	}

	em_folder_tree_model_add_store (model, store, account->name);
	camel_object_unref (store);
}

static void
account_removed (EAccountList *accounts, EAccount *account, gpointer user_data)
{
	EMFolderTreeModel *model = user_data;
	struct _EMFolderTreeModelStoreInfo *si;

	if (!(si = g_hash_table_lookup (model->account_hash, account)))
		return;

	em_folder_tree_model_remove_store (model, si->store);
}

void
em_folder_tree_model_set_folder_info (EMFolderTreeModel *model, GtkTreeIter *iter,
				      struct _EMFolderTreeModelStoreInfo *si,
				      CamelFolderInfo *fi, int fully_loaded)
{
	GtkTreeRowReference *uri_row, *path_row;
	unsigned int unread;
	GtkTreePath *path;
	GtkTreeIter sub;
	gboolean load = FALSE;
	struct _CamelFolder *folder;
	gboolean emitted = FALSE;
	const char *name;
	guint32 flags;

	if (!fully_loaded)
		load = fi->child == NULL && !(fi->flags & (CAMEL_FOLDER_NOCHILDREN | CAMEL_FOLDER_NOINFERIORS));

	path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
	uri_row = gtk_tree_row_reference_new ((GtkTreeModel *) model, path);
	path_row = gtk_tree_row_reference_copy (uri_row);
	gtk_tree_path_free (path);

	g_hash_table_insert (model->uri_hash, g_strdup (fi->uri), uri_row);
	g_hash_table_insert (si->full_hash, g_strdup (fi->full_name), path_row);

	/* HACK: if we have the folder, and its the outbox folder, we need the total count, not unread */
	/* HACK2: We do the same to the draft folder */
	/* This is duplicated in mail-folder-cache too, should perhaps be functionised */
	unread = fi->unread;
	if (mail_note_get_folder_from_uri(fi->uri, &folder) && folder) {
		if (folder == mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX)) {
			int total;

			if ((total = camel_folder_get_message_count (folder)) > 0) {
				int deleted = camel_folder_get_deleted_message_count (folder);

				if (deleted != -1)
					total -= deleted;
			}

			unread = total > 0 ? total : 0;
		}
		if (folder == mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_DRAFTS)) {
			int total;

			if ((total = camel_folder_get_message_count (folder)) > 0) {
				int deleted = camel_folder_get_deleted_message_count (folder);

				if (deleted != -1)
					total -= deleted;
			}

			unread = total > 0 ? total : 0;
		}
		camel_object_unref(folder);

	}

	/* TODO: maybe this should be handled by mail_get_folderinfo (except em-folder-tree doesn't use it, duh) */
	flags = fi->flags;
	name = fi->name;
	if (si->store == mail_component_peek_local_store(NULL)) {
		if (!strcmp(fi->full_name, "Drafts")) {
			name = _("Drafts");
		} else if (!strcmp(fi->full_name, "Inbox")) {
			flags = (flags & ~CAMEL_FOLDER_TYPE_MASK) | CAMEL_FOLDER_TYPE_INBOX;
			name = _("Inbox");
		} else if (!strcmp(fi->full_name, "Outbox")) {
			flags = (flags & ~CAMEL_FOLDER_TYPE_MASK) | CAMEL_FOLDER_TYPE_OUTBOX;
			name = _("Outbox");
		} else if (!strcmp(fi->full_name, "Sent")) {
			name = _("Sent");
			flags = (flags & ~CAMEL_FOLDER_TYPE_MASK) | CAMEL_FOLDER_TYPE_SENT;
		}
	}

	gtk_tree_store_set ((GtkTreeStore *) model, iter,
			    COL_STRING_DISPLAY_NAME, name,
			    COL_POINTER_CAMEL_STORE, si->store,
			    COL_STRING_FULL_NAME, fi->full_name,
			    COL_STRING_URI, fi->uri,
			    COL_UINT_FLAGS, flags,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_BOOL_LOAD_SUBDIRS, load,
			    -1);

	if (unread != ~0)
		gtk_tree_store_set ((GtkTreeStore *) model, iter, COL_UINT_UNREAD, unread, -1);

	if (load) {
		/* create a placeholder node for our subfolders... */
		gtk_tree_store_append ((GtkTreeStore *) model, &sub, iter);
		gtk_tree_store_set ((GtkTreeStore *) model, &sub,
				    COL_STRING_DISPLAY_NAME, _("Loading..."),
				    COL_POINTER_CAMEL_STORE, NULL,
				    COL_STRING_FULL_NAME, NULL,
				    COL_BOOL_LOAD_SUBDIRS, FALSE,
				    COL_BOOL_IS_STORE, FALSE,
				    COL_STRING_URI, NULL,
				    COL_UINT_UNREAD, 0,
				    -1);

		path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
		g_signal_emit (model, signals[LOADING_ROW], 0, path, iter);
		gtk_tree_path_free (path);
		return;
	}

	if (fi->child) {
		fi = fi->child;

		do {
			gtk_tree_store_append ((GtkTreeStore *) model, &sub, iter);

			if (!emitted) {
				path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
				g_signal_emit (model, signals[LOADED_ROW], 0, path, iter);
				gtk_tree_path_free (path);
				emitted = TRUE;
			}

			em_folder_tree_model_set_folder_info (model, &sub, si, fi, fully_loaded);
			fi = fi->next;
		} while (fi);
	}

	if (!emitted) {
		path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
		g_signal_emit (model, signals[LOADED_ROW], 0, path, iter);
		gtk_tree_path_free (path);
	}
}


static void
folder_subscribed (CamelStore *store, CamelFolderInfo *fi, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeIter parent, iter;
	GtkTreePath *path;
	gboolean load;
	char *dirname, *p;

	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		goto done;

	/* make sure we don't already know about it? */
	if (g_hash_table_lookup (si->full_hash, fi->full_name))
		goto done;

	/* get our parent folder's path */
	dirname = alloca(strlen(fi->full_name)+1);
	strcpy(dirname, fi->full_name);
	p = strrchr(dirname, '/');
	if (p == NULL) {
		/* user subscribed to a toplevel folder */
		row = si->row;
	} else {
		*p = 0;
		row = g_hash_table_lookup (si->full_hash, dirname);

		/* if row is NULL, don't bother adding to the tree,
		 * when the user expands enough nodes - it will be
		 * added auto-magically */
		if (row == NULL)
			goto done;
	}

	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &parent, path))) {
		gtk_tree_path_free (path);
		goto done;
	}

	gtk_tree_path_free (path);

	/* make sure parent's subfolders have already been loaded */
	gtk_tree_model_get ((GtkTreeModel *) model, &parent, COL_BOOL_LOAD_SUBDIRS, &load, -1);
	if (load)
		goto done;

	/* append a new node */
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &parent);

	em_folder_tree_model_set_folder_info (model, &iter, si, fi, TRUE);

	g_signal_emit (model, signals[FOLDER_ADDED], 0, fi->full_name, fi->uri);

 done:

	camel_object_unref (store);
	camel_folder_info_free (fi);
}

static void
folder_subscribed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;

	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_subscribed, store, fi, model);
}

static void
folder_unsubscribed (CamelStore *store, CamelFolderInfo *fi, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreePath *path;
	GtkTreeIter iter;

	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		goto done;

	if (!(row = g_hash_table_lookup (si->full_hash, fi->full_name)))
		goto done;

	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path))) {
		gtk_tree_path_free (path);
		goto done;
	}

	em_folder_tree_model_remove_folders (model, si, &iter);

 done:

	camel_object_unref (store);
	camel_folder_info_free (fi);
}

static void
folder_unsubscribed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;

	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_unsubscribed, store, fi, model);
}

static void
folder_created_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;

	/* we only want created events to do more work if we don't support subscriptions */
	if (camel_store_supports_subscriptions (store))
		return;

	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_subscribed, store, fi, model);
}

static void
folder_deleted_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelFolderInfo *fi;

	/* we only want deleted events to do more work if we don't support subscriptions */
	if (camel_store_supports_subscriptions (store))
		return;

	camel_object_ref (store);
	fi = camel_folder_info_clone (event_data);
	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_unsubscribed_cb, store, fi, model);
}

static void
folder_renamed (CamelStore *store, CamelRenameInfo *info, EMFolderTreeModel *model)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	char *parent, *p;

	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		goto done;

	if (!(row = g_hash_table_lookup (si->full_hash, info->old_base)))
		goto done;

	path = gtk_tree_row_reference_get_path (row);
	if (!(gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path))) {
		gtk_tree_path_free (path);
		goto done;
	}

	em_folder_tree_model_remove_folders (model, si, &iter);

	parent = g_strdup(info->new->full_name);
	p = strrchr(parent, '/');
	if (p)
		*p = 0;
	if (p == NULL || parent == p) {
		/* renamed to a toplevel folder on the store */
		path = gtk_tree_row_reference_get_path (si->row);
	} else {
		if (!(row = g_hash_table_lookup (si->full_hash, parent))) {
			/* NOTE: this should never happen, but I
			 * suppose if it does in reality, we can add
			 * code here to add the missing nodes to the
			 * tree */
			g_warning ("This shouldn't be reached\n");
			g_free (parent);
			goto done;
		}

		path = gtk_tree_row_reference_get_path (row);
	}

	g_free (parent);

	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &root, path)) {
		gtk_tree_path_free (path);
		g_warning ("This shouldn't be reached\n");
		goto done;
	}

	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &root);
	em_folder_tree_model_set_folder_info (model, &iter, si, info->new, TRUE);

 done:

	camel_object_unref (store);

	g_free (info->old_base);
	camel_folder_info_free (info->new);
	g_free (info);
}

static void
folder_renamed_cb (CamelStore *store, void *event_data, EMFolderTreeModel *model)
{
	CamelRenameInfo *rinfo, *info = event_data;

	camel_object_ref (store);

	rinfo = g_new0 (CamelRenameInfo, 1);
	rinfo->old_base = g_strdup (info->old_base);
	rinfo->new = camel_folder_info_clone (info->new);

	mail_async_event_emit (mail_async_event, MAIL_ASYNC_GUI, (MailAsyncFunc) folder_renamed, store, rinfo, model);
}

void
em_folder_tree_model_add_store (EMFolderTreeModel *model, CamelStore *store, const char *display_name)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	EAccount *account;
	char *uri;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (display_name != NULL);

	if ((si = g_hash_table_lookup (model->store_hash, store)))
		em_folder_tree_model_remove_store (model, store);

	uri = camel_url_to_string (((CamelService *) store)->url, CAMEL_URL_HIDE_ALL);

	account = mail_config_get_account_by_source_url (uri);

	/* add the store to the tree */
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, NULL);
	gtk_tree_store_set ((GtkTreeStore *) model, &iter,
			    COL_STRING_DISPLAY_NAME, display_name,
			    COL_POINTER_CAMEL_STORE, store,
			    COL_STRING_FULL_NAME, NULL,
			    COL_BOOL_LOAD_SUBDIRS, TRUE,
			    COL_BOOL_IS_STORE, TRUE,
			    COL_STRING_URI, uri, -1);

	path = gtk_tree_model_get_path ((GtkTreeModel *) model, &iter);
	row = gtk_tree_row_reference_new ((GtkTreeModel *) model, path);

	si = g_new (struct _EMFolderTreeModelStoreInfo, 1);
	si->display_name = g_strdup (display_name);
	camel_object_ref (store);
	si->store = store;
	si->account = account;
	si->row = row;
	si->full_hash = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) gtk_tree_row_reference_free);
	g_hash_table_insert (model->store_hash, store, si);
	g_hash_table_insert (model->account_hash, account, si);

	/* each store has folders... but we don't load them until the user demands them */
	root = iter;
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &root);
	gtk_tree_store_set ((GtkTreeStore *) model, &iter,
			    COL_STRING_DISPLAY_NAME, _("Loading..."),
			    COL_POINTER_CAMEL_STORE, NULL,
			    COL_STRING_FULL_NAME, NULL,
			    COL_BOOL_LOAD_SUBDIRS, FALSE,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_STRING_URI, NULL,
			    COL_UINT_UNREAD, 0,
			    -1);

	g_free (uri);

	/* listen to store events */
#define CAMEL_CALLBACK(func) ((CamelObjectEventHookFunc) func)
	si->created_id = camel_object_hook_event (store, "folder_created", CAMEL_CALLBACK (folder_created_cb), model);
	si->deleted_id = camel_object_hook_event (store, "folder_deleted", CAMEL_CALLBACK (folder_deleted_cb), model);
	si->renamed_id = camel_object_hook_event (store, "folder_renamed", CAMEL_CALLBACK (folder_renamed_cb), model);
	si->subscribed_id = camel_object_hook_event (store, "folder_subscribed", CAMEL_CALLBACK (folder_subscribed_cb), model);
	si->unsubscribed_id = camel_object_hook_event (store, "folder_unsubscribed", CAMEL_CALLBACK (folder_unsubscribed_cb), model);

	g_signal_emit (model, signals[LOADED_ROW], 0, path, &root);
	gtk_tree_path_free (path);
}

void
em_folder_tree_model_signal_block (EMFolderTreeModel *model, CamelStore *store, gboolean block)
{
	struct _EMFolderTreeModelStoreInfo *si;

	si = g_hash_table_lookup (model->store_hash, store);
	if (!si)
		return;

	if (block) {
		if (si->created_id) 
			camel_object_unhook_event (store, "folder_created", CAMEL_CALLBACK (folder_created_cb), model);
		si->created_id = 0;
	} else {
		si->created_id = camel_object_hook_event (store, "folder_created", CAMEL_CALLBACK (folder_created_cb), model);
	}
}

static void
em_folder_tree_model_remove_uri (EMFolderTreeModel *model, const char *uri)
{
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (uri != NULL);

	g_hash_table_remove (model->uri_hash, uri);
}


static void
em_folder_tree_model_remove_store_info (EMFolderTreeModel *model, CamelStore *store)
{
	struct _EMFolderTreeModelStoreInfo *si;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));

	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		return;

	g_hash_table_remove (model->store_hash, si->store);
	g_hash_table_remove (model->account_hash, si->account);
}


void
em_folder_tree_model_remove_folders (EMFolderTreeModel *model, struct _EMFolderTreeModelStoreInfo *si, GtkTreeIter *toplevel)
{
	char *uri, *full_name;
	gboolean is_store, go;
	GtkTreeIter iter;

	if (gtk_tree_model_iter_children ((GtkTreeModel *) model, &iter, toplevel)) {
		do {
			GtkTreeIter next = iter;

			go = gtk_tree_model_iter_next ((GtkTreeModel *) model, &next);
			em_folder_tree_model_remove_folders (model, si, &iter);
			iter = next;
		} while (go);
	}

	gtk_tree_model_get ((GtkTreeModel *) model, toplevel, COL_STRING_URI, &uri,
			    COL_STRING_FULL_NAME, &full_name,
			    COL_BOOL_IS_STORE, &is_store, -1);

	if (full_name)
		g_hash_table_remove (si->full_hash, full_name);

	if (uri)
		em_folder_tree_model_remove_uri (model, uri);

	gtk_tree_store_remove ((GtkTreeStore *) model, toplevel);

	if (is_store)
		em_folder_tree_model_remove_store_info (model, si->store);

	g_free (full_name);
	g_free (uri);
}


void
em_folder_tree_model_remove_store (EMFolderTreeModel *model, CamelStore *store)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));

	if (!(si = g_hash_table_lookup (model->store_hash, store)))
		return;

	path = gtk_tree_row_reference_get_path (si->row);
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path);
	gtk_tree_path_free (path);

	/* recursively remove subfolders and finally the toplevel store */
	em_folder_tree_model_remove_folders (model, si, &iter);
}


static xmlNodePtr
find_xml_node (xmlNodePtr root, const char *name)
{
	xmlNodePtr node;
	char *nname;

	node = root->children;
	while (node != NULL) {
		if (!strcmp ((char *)node->name, "node")) {
			nname = (char *)xmlGetProp (node, (const unsigned char *)"name");
			if (nname && !strcmp (nname, name)) {
				xmlFree (nname);
				return node;
			}

			xmlFree (nname);
		}

		node = node->next;
	}

	return node;
}

gboolean
em_folder_tree_model_get_expanded (EMFolderTreeModel *model, const char *key)
{
	xmlNodePtr node;
	const char *name;
	char *buf, *p;

	/* This code needs to be rewritten.
	   First it doesn't belong on the model
	   Second, it shouldn't use an xml tree to store a bit table in memory! */

	node = model->state ? model->state->children : NULL;
	if (!node || strcmp ((char *)node->name, "tree-state") != 0)
		return FALSE;

	name = buf = g_alloca (strlen (key) + 1);
	p = g_stpcpy (buf, key);
	if (p[-1] == '/')
		p[-1] = '\0';
	p = NULL;

	do {
		if ((p = strchr (name, '/')))
			*p = '\0';

		if ((node = find_xml_node (node, name))) {
			gboolean expanded;

			buf = (char *)xmlGetProp (node, (const unsigned char *)"expand");
			expanded = buf && !strcmp ((char *)buf, "true");
			xmlFree (buf);

			if (!expanded || p == NULL)
				return expanded;
		}

		name = p ? p + 1 : NULL;
	} while (name && node);

	return FALSE;
}


void
em_folder_tree_model_set_expanded (EMFolderTreeModel *model, const char *key, gboolean expanded)
{
	xmlNodePtr node, parent;
	const char *name;
	char *buf, *p;

	if (model->state == NULL)
		model->state = xmlNewDoc ((const unsigned char *)"1.0");

	if (!model->state->children) {
		node = xmlNewDocNode (model->state, NULL, (const unsigned char *)"tree-state", NULL);
		xmlDocSetRootElement (model->state, node);
	} else {
		node = model->state->children;
	}

	name = buf = g_alloca (strlen (key) + 1);
	p = g_stpcpy (buf, key);
	if (p[-1] == '/')
		p[-1] = '\0';
	p = NULL;

	do {
		parent = node;
		if ((p = strchr (name, '/')))
			*p = '\0';

		if (!(node = find_xml_node (node, name))) {
			if (!expanded) {
				/* node doesn't exist, so we don't need to set expanded to FALSE */
				return;
			}

			/* node (or parent node) doesn't exist, need to add it */
			node = xmlNewChild (parent, NULL, (const unsigned char *)"node", NULL);
			xmlSetProp (node, (const unsigned char *)"name", (unsigned char *)name);
		}

		xmlSetProp (node, (const unsigned char *)"expand", (const unsigned char *)(expanded || p ? "true" : "false"));

		name = p ? p + 1 : NULL;
	} while (name);
}

/**
 * emftm_uri_to_key
 * Converts uri to key used in functions like em_folder_tree_model_[s/g]et_expanded.
 * @param uri Uri to be converted.
 * @return Key of the uri or NULL, if failed. Returned value should be clear by g_free.
 **/
static gchar *
emftm_uri_to_key (const char *uri)
{
	CamelException ex = { 0 };
	CamelStore *store;
	CamelURL *url;
	gchar *key;

	if (!uri)
		return NULL;

	store = (CamelStore *)camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex);
	camel_exception_clear(&ex);

	url = camel_url_new (uri, NULL);

	if (store == NULL || url == NULL) {
		key = NULL;
	} else {
		const char *path;
		EAccount *account;

		if (((CamelService *)store)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
			path = url->fragment;
		else
			path = url->path && url->path[0]=='/' ? url->path+1:url->path;

		if (path == NULL)
			path = "";

		if ( (account = mail_config_get_account_by_source_url (uri)) )
			key = g_strdup_printf ("%s/%s", account->uid, path);
		else if (CAMEL_IS_VEE_STORE (store))
			key = g_strdup_printf ("vfolder/%s", path);
		else
			key = g_strdup_printf ("local/%s", path);
	}

	if (url)
		camel_url_free (url);

	if (store)
		camel_object_unref (store);

	return key;
}

/**
 * em_folder_tree_model_get_expanded_uri
 * Same as @ref em_folder_tree_model_get_expanded, but here we use uri, not key for node.
 **/
gboolean
em_folder_tree_model_get_expanded_uri (EMFolderTreeModel *model, const char *uri)
{
	gchar *key;
	gboolean expanded;

	g_return_val_if_fail (model != NULL, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	key = emftm_uri_to_key (uri);
	expanded = key && em_folder_tree_model_get_expanded (model, key);

	g_free (key);

	return expanded;
}

/**
 * em_folder_tree_model_set_expanded_uri
 * Same as @ref em_folder_tree_model_set_expanded, but here we use uri, not key for node.
 **/
void
em_folder_tree_model_set_expanded_uri (EMFolderTreeModel *model, const char *uri, gboolean expanded)
{
	gchar *key;

	g_return_if_fail (model != NULL);
	g_return_if_fail (uri != NULL);

	key = emftm_uri_to_key (uri);
	if (key)
		em_folder_tree_model_set_expanded (model, key, expanded);

	g_free (key);
}

void
em_folder_tree_model_save_state (EMFolderTreeModel *model)
{
	char *dirname;

	if (model->state == NULL)
		return;

	dirname = g_path_get_dirname (model->filename);
	if (g_mkdir_with_parents (dirname, 0777) == -1 && errno != EEXIST) {
		g_free (dirname);
		return;
	}

	g_free (dirname);

	e_xml_save_file (model->filename, model->state);
}


static void
expand_foreach_r (EMFolderTreeModel *model, xmlNodePtr parent, const char *dirname, EMFTModelExpandFunc func, void *user_data)
{
	xmlNodePtr node = parent->children;
	char *path, *name, *expand;

	while (node != NULL) {
		if (!strcmp ((char *)node->name, "node")) {
			name = (char *)xmlGetProp (node, (const unsigned char *)"name");
			expand = (char *)xmlGetProp (node, (const unsigned char *)"expand");

			if (expand && name && !strcmp ((char *)expand, "true")) {
				if (dirname)
					path = g_strdup_printf ("%s/%s", dirname, name);
				else
					path = g_strdup (name);

				func (model, path, user_data);
				if (node->children)
					expand_foreach_r (model, node, path, func, user_data);
				g_free (path);
			}

			xmlFree (expand);
			xmlFree (name);
		}

		node = node->next;
	}
}

void
em_folder_tree_model_expand_foreach (EMFolderTreeModel *model, EMFTModelExpandFunc func, void *user_data)
{
	xmlNodePtr root;

	root = model->state ? model->state->children : NULL;
	if (!root || !root->children || strcmp ((char *)root->name, "tree-state") != 0)
		return;

	expand_foreach_r (model, root, NULL, func, user_data);
}

gboolean
em_folder_tree_model_is_type_inbox (EMFolderTreeModel *model, CamelStore *store, const char *full)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreePath *tree_path;
	GtkTreeIter iter;
	guint32 flags;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (full != NULL, FALSE);

	u(printf("Checking if the folder is an INBOX type %p '%s' %d\n", store, full, unread));

	if (!(si = g_hash_table_lookup (model->store_hash, store))) {
		u(printf("  can't find store\n"));
		return FALSE;
	}

	if (!(row = g_hash_table_lookup (si->full_hash, full))) {
		u(printf("  can't find row\n"));
		return FALSE;
	}

	tree_path = gtk_tree_row_reference_get_path (row);
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, tree_path)) {
		gtk_tree_path_free (tree_path);
		return FALSE;
	}

	gtk_tree_path_free (tree_path);

	gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, COL_UINT_FLAGS, &flags, -1);

	if ((flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
		return TRUE;

	return FALSE;
}

char *
em_folder_tree_model_get_folder_name (EMFolderTreeModel *model, CamelStore *store, const char *full)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreePath *tree_path;
	GtkTreeIter iter;
	char *name = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (full != NULL, FALSE);

	if (!(si = g_hash_table_lookup (model->store_hash, store))) {
		u(printf("  can't find store\n"));
		return NULL;
	}

	if (!(row = g_hash_table_lookup (si->full_hash, full))) {
		u(printf("  can't find row\n"));
		return NULL;
	}

	tree_path = gtk_tree_row_reference_get_path (row);
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, tree_path)) {
		gtk_tree_path_free (tree_path);
		return NULL;
	}

	gtk_tree_path_free (tree_path);

	gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, COL_STRING_DISPLAY_NAME, &name, -1);

	return name;
}

void
em_folder_tree_model_set_unread_count (EMFolderTreeModel *model, CamelStore *store, const char *full, int unread)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreePath *tree_path;
	GtkTreeIter iter;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (full != NULL);

	u(printf("set unread count %p '%s' %d\n", store, full, unread));

	if (unread < 0)
		return;

	if (!(si = g_hash_table_lookup (model->store_hash, store))) {
		u(printf("  can't find store\n"));
		return;
	}

	if (!(row = g_hash_table_lookup (si->full_hash, full))) {
		u(printf("  can't find row\n"));
		return;
	}

	tree_path = gtk_tree_row_reference_get_path (row);
	if (!gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, tree_path)) {
		gtk_tree_path_free (tree_path);
		return;
	}

	gtk_tree_path_free (tree_path);

	gtk_tree_store_set ((GtkTreeStore *) model, &iter, COL_UINT_UNREAD, unread, -1);

	/* May be this is from where we should propagate unread count to parents etc. */
	emft_model_unread_count_changed (GTK_TREE_MODEL (model), &iter);
}


char *
em_folder_tree_model_get_selected (EMFolderTreeModel *model)
{
	xmlNodePtr node;
	char *buf, *uri;

	node = model->state ? model->state->children : NULL;
	if (!node || strcmp ((char *)node->name, "tree-state") != 0)
		return NULL;

	node = node->children;
	while (node != NULL) {
		if (!strcmp ((char *)node->name, "selected"))
			break;
		node = node->next;
	}

	if (node == NULL)
		return NULL;

	buf = (char *)xmlGetProp (node, (unsigned char *)"uri");
	uri = g_strdup (buf);
	xmlFree (buf);

	if (uri && !*uri) {
		g_free (uri);
		return NULL;
	}
	return uri;
}


void
em_folder_tree_model_set_selected (EMFolderTreeModel *model, const char *uri)
{
	xmlNodePtr root, node;

	if (model->state == NULL)
		model->state = xmlNewDoc ((unsigned char *)"1.0");

	if (!model->state->children) {
		root = xmlNewDocNode (model->state, NULL, (const unsigned char *)"tree-state", NULL);
		xmlDocSetRootElement (model->state, root);
	} else {
		root = model->state->children;
	}

	node = root->children;
	while (node != NULL) {
		if (!strcmp ((char *)node->name, "selected"))
			break;
		node = node->next;
	}

	if (node == NULL)
		node = xmlNewChild (root, NULL, (const unsigned char *)"selected", NULL);

	xmlSetProp (node, (const unsigned char *)"uri", (unsigned char *)uri);
}

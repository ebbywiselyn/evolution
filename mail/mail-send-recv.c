/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Michael Zucchi <NotZed@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>

/* for the dialog stuff */
#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkimage.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>

#include "libedataserver/e-account-list.h"

#include "em-filter-rule.h"
#include "camel/camel-filter-driver.h"
#include "camel/camel-folder.h"
#include "camel/camel-operation.h"
#include "camel/camel-store.h"

#include "mail-mt.h"
#include "mail-component.h"
#include "mail-config.h"
#include "mail-session.h"
#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-send-recv.h"
#include "mail-folder-cache.h"
#include "em-event.h"
#include <e-util/e-icon-factory.h>
#include <e-util/gconf-bridge.h>

#define d(x)

/* ms between status updates to the gui */
#define STATUS_TIMEOUT (250)

/* pseudo-uri to key the send task on */
#define SEND_URI_KEY "send-task:"

/* Prefix for window size GConf keys */
#define GCONF_KEY_PREFIX "/apps/evolution/mail/send_recv"

/* send/receive email */

/* ********************************************************************** */
/*  This stuff below is independent of the stuff above */

/* this stuff is used to keep track of which folders filters have accessed, and
   what not. the thaw/refreeze thing doesn't really seem to work though */
struct _folder_info {
	char *uri;
	CamelFolder *folder;
	time_t update;
	int count;		/* how many times updated, to slow it down as we go, if we have lots */
};

struct _send_data {
	GList *infos;

	GtkDialog *gd;
	int cancelled;

	CamelFolder *inbox;	/* since we're never asked to update this one, do it ourselves */
	time_t inbox_update;

	GMutex *lock;
	GHashTable *folders;

	GHashTable *active;	/* send_info's by uri */
};

typedef enum {
	SEND_RECEIVE,		/* receiver */
	SEND_SEND,		/* sender */
	SEND_UPDATE,		/* imap-like 'just update folder info' */
	SEND_INVALID
} send_info_t ;

typedef enum {
	SEND_ACTIVE,
	SEND_CANCELLED,
	SEND_COMPLETE
} send_state_t;

struct _send_info {
	send_info_t type;		/* 0 = fetch, 1 = send */
	CamelOperation *cancel;
	char *uri;
	int keep;
	send_state_t state;
	GtkWidget *progress_bar;
	GtkWidget *cancel_button;
	GtkWidget *status_label;

	int again;		/* need to run send again */

	int timeout_id;
	char *what;
	int pc;

	/*time_t update;*/
	struct _send_data *data;
};

static CamelFolder *receive_get_folder(CamelFilterDriver *d, const char *uri, void *data, CamelException *ex);

static struct _send_data *send_data = NULL;
static GtkWidget *send_recv_dialog = NULL;

static void
free_folder_info(struct _folder_info *info)
{
	/*camel_folder_thaw (info->folder);	*/
	mail_sync_folder(info->folder, NULL, NULL);
	camel_object_unref(info->folder);
	g_free(info->uri);
	g_free(info);
}

static void
free_send_info(struct _send_info *info)
{
	g_free(info->uri);
	if (info->cancel)
		camel_operation_unref(info->cancel);
	if (info->timeout_id != 0)
		g_source_remove(info->timeout_id);
	g_free(info->what);
	g_free(info);
}

static struct _send_data *
setup_send_data(void)
{
	struct _send_data *data;

	if (send_data == NULL) {
		send_data = data = g_malloc0(sizeof(*data));
		data->lock = g_mutex_new();
		data->folders = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) NULL,
			(GDestroyNotify) free_folder_info);
		data->inbox = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_LOCAL_INBOX);
		camel_object_ref(data->inbox);
		data->active = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) NULL,
			(GDestroyNotify) free_send_info);
	}
	return send_data;
}

static void
receive_cancel(GtkButton *button, struct _send_info *info)
{
	if (info->state == SEND_ACTIVE) {
		camel_operation_cancel(info->cancel);
		if (info->status_label)
			gtk_label_set_text (
				GTK_LABEL (info->status_label),
				_("Canceling..."));
		info->state = SEND_CANCELLED;
	}
	if (info->cancel_button)
		gtk_widget_set_sensitive(info->cancel_button, FALSE);
}

static void
free_send_data(void)
{
	struct _send_data *data = send_data;

	g_return_if_fail (g_hash_table_size(data->active) == 0);

	if (data->inbox) {
		mail_sync_folder(data->inbox, NULL, NULL);
		/*camel_folder_thaw (data->inbox);		*/
		camel_object_unref(data->inbox);
	}

	g_list_free(data->infos);
	g_hash_table_destroy(data->active);
	g_hash_table_destroy(data->folders);
	g_mutex_free(data->lock);
	g_free(data);
	send_data = NULL;
}

static void
cancel_send_info(void *key, struct _send_info *info, void *data)
{
	receive_cancel (GTK_BUTTON (info->cancel_button), info);
}

static void
hide_send_info(void *key, struct _send_info *info, void *data)
{
	info->cancel_button = NULL;
	info->progress_bar = NULL;
	info->status_label = NULL;

	if (info->timeout_id != 0) {
		g_source_remove (info->timeout_id);
		info->timeout_id = 0;
	}
}

static void
dialog_destroy_cb (struct _send_data *data, GObject *deadbeef)
{
	g_hash_table_foreach (data->active, (GHFunc) hide_send_info, NULL);
	data->gd = NULL;
	send_recv_dialog = NULL;
}

static void
dialog_response(GtkDialog *gd, int button, struct _send_data *data)
{
	switch(button) {
	case GTK_RESPONSE_CANCEL:
		d(printf("cancelled whole thing\n"));
		if (!data->cancelled) {
			data->cancelled = TRUE;
			g_hash_table_foreach(data->active, (GHFunc)cancel_send_info, NULL);
		}
		gtk_dialog_set_response_sensitive(gd, GTK_RESPONSE_CANCEL, FALSE);
		break;
	default:
		d(printf("hiding dialog\n"));
		g_hash_table_foreach(data->active, (GHFunc)hide_send_info, NULL);
		data->gd = NULL;
		/*gtk_widget_destroy((GtkWidget *)gd);*/
		break;
	}
}

static int
operation_status_timeout(void *data)
{
	struct _send_info *info = data;

	if (info->progress_bar) {
		gtk_progress_bar_set_fraction (
			GTK_PROGRESS_BAR (info->progress_bar),
			info->pc / 100.0);
		if (info->what)
			gtk_label_set_text (
				GTK_LABEL (info->status_label),
				info->what);
		return TRUE;
	}

	return FALSE;
}

static void
set_send_status(struct _send_info *info, const char *desc, int pc)
{
	/* FIXME: LOCK */
	g_free(info->what);
	info->what = g_strdup(desc);
	info->pc = pc;
}

/* for camel operation status */
static void
operation_status(CamelOperation *op, const char *what, int pc, void *data)
{
	struct _send_info *info = data;

	/*printf("Operation '%s', percent %d\n");*/
	switch (pc) {
	case CAMEL_OPERATION_START:
		pc = 0;
		break;
	case CAMEL_OPERATION_END:
		pc = 100;
		break;
	}

	set_send_status(info, what, pc);
}

static char *
format_url(const char *internal_url, const char *account_name)
{
	CamelURL *url;
       	char *pretty_url;

	url = camel_url_new(internal_url, NULL);
	if (account_name) {
		if (url->host && *url->host)
			pretty_url = g_strdup_printf("<b>%s (%s)</b>: %s", account_name, url->protocol, url->host);
		else if (url->path)
			pretty_url = g_strdup_printf("<b>%s (%s)</b>: %s", account_name, url->protocol, url->path);
		else
			pretty_url = g_strdup_printf("<b>%s (%s)</b>", account_name, url->protocol);
	}
	else {
		if (url->host && *url->host)
			pretty_url = g_strdup_printf("<b>%s</b>: %s", url->protocol, url->host);
		else if (url->path)
			pretty_url = g_strdup_printf("<b>%s</b>: %s", url->protocol, url->path);
		else
			pretty_url = g_strdup_printf("<b>%s</b>", url->protocol);
	}
	camel_url_free(url);

        return pretty_url;
}

static send_info_t
get_receive_type(const char *url)
{
	CamelProvider *provider;
	CamelException ex;

	/* HACK: since mbox is ALSO used for native evolution trees now, we need to
	   fudge this to treat it as a special 'movemail' source */
	if (!strncmp(url, "mbox:", 5))
		return SEND_RECEIVE;

	camel_exception_init (&ex);
	provider = camel_provider_get(url, &ex);
	camel_exception_clear (&ex);

	if (!provider)
		return SEND_INVALID;

	if (provider->object_types[CAMEL_PROVIDER_STORE]) {
		if (provider->flags & CAMEL_PROVIDER_IS_STORAGE)
			return SEND_UPDATE;
		else
			return SEND_RECEIVE;
	} else if (provider->object_types[CAMEL_PROVIDER_TRANSPORT]) {
		return SEND_SEND;
	}

	return SEND_INVALID;
}

static struct _send_data *
build_dialog (EAccountList *accounts, CamelFolder *outbox, const char *destination)
{
	GtkDialog *gd;
	GtkWidget *table;
	int row, num_sources;
	GList *list = NULL;
	struct _send_data *data;
        GtkWidget *send_icon;
	GtkWidget *recv_icon;
	GtkWidget *scrolled_window;
	GtkWidget *label;
	GtkWidget *status_label;
	GtkWidget *progress_bar;
	GtkWidget *cancel_button;
	struct _send_info *info;
	char *pretty_url;
	EAccount *account;
	EIterator *iter;
	GList *icon_list;
	EMEventTargetSendReceive *target;

	gd = (GtkDialog *)(send_recv_dialog = gtk_dialog_new_with_buttons(_("Send & Receive Mail"), NULL, GTK_DIALOG_NO_SEPARATOR, NULL));
	gtk_window_set_modal ((GtkWindow *) gd, FALSE);

	gconf_bridge_bind_window_size (
		gconf_bridge_get (), GCONF_KEY_PREFIX,
		GTK_WINDOW (send_recv_dialog));

	gtk_widget_ensure_style ((GtkWidget *)gd);
	gtk_container_set_border_width ((GtkContainer *)gd->vbox, 0);
	gtk_container_set_border_width ((GtkContainer *)gd->action_area, 6);

	cancel_button = gtk_button_new_with_mnemonic (_("Cancel _All"));
	gtk_button_set_image (
		GTK_BUTTON (cancel_button),
		gtk_image_new_from_stock (
			GTK_STOCK_CANCEL, GTK_ICON_SIZE_BUTTON));
	gtk_widget_show (cancel_button);
	gtk_dialog_add_action_widget (gd, cancel_button, GTK_RESPONSE_CANCEL);

	icon_list = e_icon_factory_get_icon_list ("mail-send-receive");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (gd), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}

	num_sources = 0;

	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		account = (EAccount *) e_iterator_get (iter);

		if (account->source->url)
			num_sources++;

		e_iterator_next (iter);
	}

	g_object_unref (iter);

	table = gtk_table_new (num_sources, 4, FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table), 6);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_container_set_border_width (
		GTK_CONTAINER (scrolled_window), 6);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrolled_window),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	gtk_scrolled_window_add_with_viewport (
		GTK_SCROLLED_WINDOW (scrolled_window), table);
	gtk_box_pack_start (
		GTK_BOX (gd->vbox), scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (scrolled_window);

	/* must bet setup after send_recv_dialog as it may re-trigger send-recv button */
	data = setup_send_data ();

	row = 0;
	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		EAccountService *source;

		account = (EAccount *) e_iterator_get (iter);

		source = account->source;
		if (!account->enabled || !source->url) {
			e_iterator_next (iter);
			continue;
		}

		/* see if we have an outstanding download active */
		info = g_hash_table_lookup (data->active, source->url);
		if (info == NULL) {
			send_info_t type;

			type = get_receive_type (source->url);
			if (type == SEND_INVALID || type == SEND_SEND) {
				e_iterator_next (iter);
				continue;
			}

			info = g_malloc0 (sizeof (*info));
			info->type = type;

			d(printf("adding source %s\n", source->url));

			info->uri = g_strdup (source->url);
			info->keep = source->keep_on_server;
			info->cancel = camel_operation_new (operation_status, info);
			info->state = SEND_ACTIVE;
			info->timeout_id = g_timeout_add (STATUS_TIMEOUT, operation_status_timeout, info);

			g_hash_table_insert (data->active, info->uri, info);
			list = g_list_prepend (list, info);
		} else if (info->progress_bar != NULL) {
			/* incase we get the same source pop up again */
			e_iterator_next (iter);
			continue;
		} else if (info->timeout_id == 0)
			info->timeout_id = g_timeout_add (STATUS_TIMEOUT, operation_status_timeout, info);

		recv_icon = e_icon_factory_get_image (
			"mail-inbox", E_ICON_SIZE_LARGE_TOOLBAR);
	       	pretty_url = format_url (source->url, account->name);
		label = gtk_label_new (NULL);
		gtk_label_set_ellipsize (
			GTK_LABEL (label), PANGO_ELLIPSIZE_END);
		gtk_label_set_markup (GTK_LABEL (label), pretty_url);
		g_free (pretty_url);

		progress_bar = gtk_progress_bar_new ();

		cancel_button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);

		status_label = gtk_label_new (
			(info->type == SEND_UPDATE) ?
			_("Updating...") : _("Waiting..."));
		gtk_label_set_ellipsize (
			GTK_LABEL (status_label), PANGO_ELLIPSIZE_END);

		/* g_object_set(data->label, "bold", TRUE, NULL); */
		gtk_misc_set_alignment (GTK_MISC (label), 0, .5);
		gtk_misc_set_alignment (GTK_MISC (status_label), 0, .5);

	        gtk_table_attach (
			GTK_TABLE (table), recv_icon,
			0, 1, row, row+2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), label,
			1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), progress_bar,
			2, 3, row, row+2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), cancel_button,
			3, 4, row, row+2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), status_label,
			1, 2, row+1, row+2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

		info->progress_bar = progress_bar;
		info->status_label = status_label;
		info->cancel_button = cancel_button;
		info->data = data;

		g_signal_connect (
			cancel_button, "clicked",
			G_CALLBACK (receive_cancel), info);
		e_iterator_next (iter);
		row = row + 2;
	}

	g_object_unref (iter);

	/* Hook: If some one wants to hook on to the sendreceive dialog, this is the way to go. */
	target = em_event_target_new_send_receive (em_event_peek(), table, data, row, EM_EVENT_SEND_RECEIVE);
	e_event_emit ((EEvent *)em_event_peek (), "mail.sendreceive", (EEventTarget *) target);

	if (outbox && destination) {
		info = g_hash_table_lookup (data->active, SEND_URI_KEY);
		if (info == NULL) {
			info = g_malloc0 (sizeof (*info));
			info->type = SEND_SEND;
			d(printf("adding dest %s\n", destination));

			info->uri = g_strdup (destination);
			info->keep = FALSE;
			info->cancel = camel_operation_new (operation_status, info);
			info->state = SEND_ACTIVE;
			info->timeout_id = g_timeout_add (STATUS_TIMEOUT, operation_status_timeout, info);

			g_hash_table_insert (data->active, SEND_URI_KEY, info);
			list = g_list_prepend (list, info);
		} else if (info->timeout_id == 0)
			info->timeout_id = g_timeout_add (STATUS_TIMEOUT, operation_status_timeout, info);

		send_icon = e_icon_factory_get_image (
			"mail-outbox", E_ICON_SIZE_LARGE_TOOLBAR);
		pretty_url = format_url (destination, NULL);
		label = gtk_label_new (NULL);
		gtk_label_set_ellipsize (
			GTK_LABEL (label), PANGO_ELLIPSIZE_END);
		gtk_label_set_markup (GTK_LABEL (label), pretty_url);

		g_free (pretty_url);

		progress_bar = gtk_progress_bar_new ();
		cancel_button = gtk_button_new_from_stock (GTK_STOCK_CANCEL);

		status_label = gtk_label_new (_("Waiting..."));
		gtk_label_set_ellipsize (
			GTK_LABEL (status_label), PANGO_ELLIPSIZE_END);

		gtk_misc_set_alignment (GTK_MISC (label), 0, .5);
		gtk_misc_set_alignment (GTK_MISC (status_label), 0, .5);

		gtk_table_attach (
			GTK_TABLE (table), send_icon,
			0, 1, row, row+2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), label,
			1, 2, row, row+1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), progress_bar,
			2, 3, row, row+2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), cancel_button,
			3, 4, row, row+2, 0, 0, 0, 0);
		gtk_table_attach (
			GTK_TABLE (table), status_label,
			1, 2, row+1, row+2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

		info->progress_bar = progress_bar;
		info->cancel_button = cancel_button;
		info->data = data;
		info->status_label = status_label;

		g_signal_connect (
			cancel_button, "clicked",
			G_CALLBACK (receive_cancel), info);
		gtk_widget_show_all (table);
	}

	gtk_widget_show (GTK_WIDGET (gd));

	g_signal_connect (gd, "response", G_CALLBACK (dialog_response), data);

	g_object_weak_ref ((GObject *) gd, (GWeakNotify) dialog_destroy_cb, data);

	data->infos = list;
	data->gd = gd;

	return data;
}

static void
update_folders(char *uri, struct _folder_info *info, void *data)
{
	time_t now = *((time_t *)data);

	d(printf("checking update for folder: %s\n", info->uri));

	/* let it flow through to the folders every 10 seconds */
	/* we back off slowly as we progress */
	if (now > info->update+10+info->count*5) {
		d(printf("upating a folder: %s\n", info->uri));
		/*camel_folder_thaw(info->folder);
		  camel_folder_freeze(info->folder);*/
		info->update = now;
		info->count++;
	}
}

static void
receive_status (CamelFilterDriver *driver, enum camel_filter_status_t status, int pc, const char *desc, void *data)
{
	struct _send_info *info = data;
	time_t now = time(NULL);

	/* let it flow through to the folder, every now and then too? */
	g_hash_table_foreach(info->data->folders, (GHFunc)update_folders, &now);

	if (info->data->inbox && now > info->data->inbox_update+20) {
		d(printf("updating inbox too\n"));
		/* this doesn't seem to work right :( */
		/*camel_folder_thaw(info->data->inbox);
		  camel_folder_freeze(info->data->inbox);*/
		info->data->inbox_update = now;
	}

	/* we just pile them onto the port, assuming it can handle it.
	   We could also have a receiver port and see if they've been processed
	   yet, so if this is necessary its not too hard to add */
	/* the mail_gui_port receiver will free everything for us */
	switch (status) {
	case CAMEL_FILTER_STATUS_START:
	case CAMEL_FILTER_STATUS_END:
		set_send_status(info, desc, pc);
		break;
	default:
		break;
	}
}

/* when receive/send is complete */
static void
receive_done (char *uri, void *data)
{
	struct _send_info *info = data;

	/* if we've been called to run again - run again */
	if (info->type == SEND_SEND && info->state == SEND_ACTIVE && info->again) {
		info->again = 0;
		mail_send_queue (mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX),
				 info->uri,
				 FILTER_SOURCE_OUTGOING,
				 info->cancel,
				 receive_get_folder, info,
				 receive_status, info,
				 receive_done, info);
		return;
	}

	if (info->progress_bar) {
		const gchar *text;

		gtk_progress_bar_set_fraction(
			GTK_PROGRESS_BAR (info->progress_bar), 1.0);

		if (info->state == SEND_CANCELLED)
			text = _("Canceled.");
		else {
			text = _("Complete.");
			info->state = SEND_COMPLETE;
		}

		gtk_label_set_text (GTK_LABEL (info->status_label), text);
	}

	if (info->cancel_button)
		gtk_widget_set_sensitive (info->cancel_button, FALSE);

	/* remove/free this active download */
	d(printf("%s: freeing info %p\n", G_STRFUNC, info));
	if (info->type == SEND_SEND)
		g_hash_table_steal(info->data->active, SEND_URI_KEY);
	else
		g_hash_table_steal(info->data->active, info->uri);
	info->data->infos = g_list_remove(info->data->infos, info);

	if (g_hash_table_size(info->data->active) == 0) {
		if (info->data->gd)
			gtk_widget_destroy((GtkWidget *)info->data->gd);
		free_send_data();
	}

	free_send_info(info);
}

/* although we dont do anythign smart here yet, there is no need for this interface to
   be available to anyone else.
   This can also be used to hook into which folders are being updated, and occasionally
   let them refresh */
static CamelFolder *
receive_get_folder(CamelFilterDriver *d, const char *uri, void *data, CamelException *ex)
{
	struct _send_info *info = data;
	CamelFolder *folder;
	struct _folder_info *oldinfo;
	gpointer oldkey, oldinfoptr;

	g_mutex_lock(info->data->lock);
	oldinfo = g_hash_table_lookup(info->data->folders, uri);
	g_mutex_unlock(info->data->lock);
	if (oldinfo) {
		camel_object_ref(oldinfo->folder);
		return oldinfo->folder;
	}
	folder = mail_tool_uri_to_folder (uri, 0, ex);
	if (!folder)
		return NULL;

	/* we recheck that the folder hasn't snuck in while we were loading it... */
	/* and we assume the newer one is the same, but unref the old one anyway */
	g_mutex_lock(info->data->lock);

	if (g_hash_table_lookup_extended (info->data->folders, uri, &oldkey, &oldinfoptr)) {
		oldinfo = (struct _folder_info *) oldinfoptr;
		camel_object_unref(oldinfo->folder);
		oldinfo->folder = folder;
	} else {
		/*camel_folder_freeze (folder);		*/
		oldinfo = g_malloc0(sizeof(*oldinfo));
		oldinfo->folder = folder;
		oldinfo->uri = g_strdup(uri);
		g_hash_table_insert(info->data->folders, oldinfo->uri, oldinfo);
	}

	camel_object_ref (folder);

	g_mutex_unlock(info->data->lock);

	return folder;
}

/* ********************************************************************** */

static void
get_folders (CamelStore *store, GPtrArray *folders, CamelFolderInfo *info)
{
	CamelException ex;

	camel_exception_init (&ex);

	while (info) {
		if (camel_store_can_refresh_folder (store, info, &ex))
			g_ptr_array_add (folders, g_strdup (info->uri));
		camel_exception_clear (&ex);

		get_folders (store, folders, info->child);
		info = info->next;
	}
}

struct _refresh_folders_msg {
	MailMsg base;

	struct _send_info *info;
	GPtrArray *folders;
	CamelStore *store;
	CamelFolderInfo *finfo;
};

static gchar *
refresh_folders_desc (struct _refresh_folders_msg *m)
{
	return g_strdup_printf(_("Checking for new mail"));
}

static void
refresh_folders_exec (struct _refresh_folders_msg *m)
{
	int i;
	CamelFolder *folder;
	CamelException ex = CAMEL_EXCEPTION_INITIALISER;

	get_folders (m->store, m->folders, m->finfo);

	for (i=0;i<m->folders->len;i++) {
		folder = mail_tool_uri_to_folder(m->folders->pdata[i], 0, &ex);
		if (folder) {
			camel_folder_refresh_info(folder, &ex);
			camel_exception_clear(&ex);
			camel_object_unref(folder);
		} else if (camel_exception_is_set(&ex)) {
			g_warning ("Failed to refresh folders: %s", camel_exception_get_description (&ex));
			camel_exception_clear (&ex);
		}

		if (camel_operation_cancel_check(m->info->cancel))
			break;
	}
}

static void
refresh_folders_done (struct _refresh_folders_msg *m)
{
	receive_done("", m->info);
}

static void
refresh_folders_free (struct _refresh_folders_msg *m)
{
	int i;

	for (i=0;i<m->folders->len;i++)
		g_free(m->folders->pdata[i]);
	g_ptr_array_free(m->folders, TRUE);

	camel_store_free_folder_info (m->store, m->finfo);
	camel_object_unref(m->store);
}

static MailMsgInfo refresh_folders_info = {
	sizeof (struct _refresh_folders_msg),
	(MailMsgDescFunc) refresh_folders_desc,
	(MailMsgExecFunc) refresh_folders_exec,
	(MailMsgDoneFunc) refresh_folders_done,
	(MailMsgFreeFunc) refresh_folders_free
};

static gboolean
receive_update_got_folderinfo(CamelStore *store, CamelFolderInfo *info, void *data)
{
	if (info) {
		GPtrArray *folders = g_ptr_array_new();
		struct _refresh_folders_msg *m;
		struct _send_info *sinfo = data;

		m = mail_msg_new(&refresh_folders_info);
		m->store = store;
		camel_object_ref(store);
		m->folders = folders;
		m->info = sinfo;
		m->finfo = info;

		mail_msg_unordered_push (m);

		/* do not free folder info, we will free it later */
		return FALSE;
	} else {
		receive_done ("", data);
	}

	return TRUE;
}

static void
receive_update_got_store (char *uri, CamelStore *store, void *data)
{
	struct _send_info *info = data;

	if (store) {
		mail_note_store(store, info->cancel, receive_update_got_folderinfo, info);
	} else {
		receive_done("", info);
	}
}

GtkWidget *
mail_send_receive (void)
{
	CamelFolder *outbox_folder;
	struct _send_data *data;
	EAccountList *accounts;
	EAccount *account;
	GList *scan;

	if (send_recv_dialog != NULL) {
		if (GTK_WIDGET_REALIZED(send_recv_dialog)) {
			gdk_window_show(send_recv_dialog->window);
			gdk_window_raise(send_recv_dialog->window);
		}
		return send_recv_dialog;
	}

	if (!camel_session_is_online (session))
		return send_recv_dialog;

	account = mail_config_get_default_account ();
	if (!account || !account->transport->url)
		return send_recv_dialog;

	accounts = mail_config_get_accounts ();

	outbox_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX);
	data = build_dialog (accounts, outbox_folder, account->transport->url);
	scan = data->infos;
	while (scan) {
		struct _send_info *info = scan->data;

		switch(info->type) {
		case SEND_RECEIVE:
			mail_fetch_mail(info->uri, info->keep,
					FILTER_SOURCE_INCOMING,
					info->cancel,
					receive_get_folder, info,
					receive_status, info,
					receive_done, info);
			break;
		case SEND_SEND:
			/* todo, store the folder in info? */
			mail_send_queue(outbox_folder, info->uri,
					FILTER_SOURCE_OUTGOING,
					info->cancel,
					receive_get_folder, info,
					receive_status, info,
					receive_done, info);
			break;
		case SEND_UPDATE:
			mail_get_store(info->uri, info->cancel, receive_update_got_store, info);
			break;
		default:
			break;
		}
		scan = scan->next;
	}

	return send_recv_dialog;
}

struct _auto_data {
	EAccount *account;
	int period;		/* in seconds */
	int timeout_id;
};

static GHashTable *auto_active;

static gboolean
auto_timeout(void *data)
{
	struct _auto_data *info = data;

	if (camel_session_is_online(session)) {
		const char *uri = e_account_get_string(info->account, E_ACCOUNT_SOURCE_URL);
		int keep = e_account_get_bool(info->account, E_ACCOUNT_SOURCE_KEEP_ON_SERVER);

		mail_receive_uri(uri, keep);
	}

	return TRUE;
}

static void
auto_account_removed(EAccountList *eal, EAccount *ea, void *dummy)
{
	struct _auto_data *info = g_object_get_data((GObject *)ea, "mail-autoreceive");

	g_return_if_fail(info != NULL);

	if (info->timeout_id) {
		g_source_remove(info->timeout_id);
		info->timeout_id = 0;
	}
}

static void
auto_account_finalised(struct _auto_data *info)
{
	if (info->timeout_id)
		g_source_remove(info->timeout_id);
	g_free(info);
}

static void
auto_account_commit(struct _auto_data *info)
{
	int period, check;

	check = info->account->enabled
		&& e_account_get_bool(info->account, E_ACCOUNT_SOURCE_AUTO_CHECK)
		&& e_account_get_string(info->account, E_ACCOUNT_SOURCE_URL);
	period = e_account_get_int(info->account, E_ACCOUNT_SOURCE_AUTO_CHECK_TIME)*60;
	period = MAX(60, period);

	if (info->timeout_id
	    && (!check
		|| period != info->period)) {
		g_source_remove(info->timeout_id);
		info->timeout_id = 0;
	}
	info->period = period;
	if (check && info->timeout_id == 0)
		info->timeout_id = g_timeout_add(info->period*1000, auto_timeout, info);
}

static void
auto_account_added(EAccountList *eal, EAccount *ea, void *dummy)
{
	struct _auto_data *info;

	info = g_malloc0(sizeof(*info));
	info->account = ea;
	g_object_set_data_full((GObject *)ea, "mail-autoreceive", info, (GDestroyNotify)auto_account_finalised);
	auto_account_commit(info);
}

static void
auto_account_changed(EAccountList *eal, EAccount *ea, void *dummy)
{
	struct _auto_data *info = g_object_get_data((GObject *)ea, "mail-autoreceive");

	g_return_if_fail(info != NULL);

	auto_account_commit(info);
}

static void
auto_online(CamelObject *o, void *ed, void *d)
{
	EIterator *iter;
	EAccountList *accounts;
	struct _auto_data *info;

	if (!GPOINTER_TO_INT(ed))
		return;

	accounts = mail_config_get_accounts ();
	for (iter = e_list_get_iterator((EList *)accounts);e_iterator_is_valid(iter);e_iterator_next(iter)) {
		info  = g_object_get_data((GObject *)e_iterator_get(iter), "mail-autoreceive");
		if (info && info->timeout_id)
			auto_timeout(info);
	}
}

/* call to setup initial, and after changes are made to the config */
/* FIXME: Need a cleanup funciton for when object is deactivated */
void
mail_autoreceive_init(void)
{
	EAccountList *accounts;
	EIterator *iter;

	if (auto_active)
		return;

	accounts = mail_config_get_accounts ();
	auto_active = g_hash_table_new(g_str_hash, g_str_equal);

	g_signal_connect(accounts, "account-added", G_CALLBACK(auto_account_added), NULL);
	g_signal_connect(accounts, "account-removed", G_CALLBACK(auto_account_removed), NULL);
	g_signal_connect(accounts, "account-changed", G_CALLBACK(auto_account_changed), NULL);

	for (iter = e_list_get_iterator((EList *)accounts);e_iterator_is_valid(iter);e_iterator_next(iter))
		auto_account_added(accounts, (EAccount *)e_iterator_get(iter), NULL);

	camel_object_hook_event(mail_component_peek_session(NULL), "online", auto_online, NULL);
}

/* we setup the download info's in a hashtable, if we later need to build the gui, we insert
   them in to add them. */
void
mail_receive_uri (const char *uri, int keep)
{
	struct _send_info *info;
	struct _send_data *data;
	CamelFolder *outbox_folder;
	send_info_t type;

	data = setup_send_data();
	info = g_hash_table_lookup(data->active, uri);
	if (info != NULL) {
		d(printf("download of %s still in progress\n", uri));
		return;
	}

	d(printf("starting non-interactive download of '%s'\n", uri));

	type = get_receive_type (uri);
	if (type == SEND_INVALID || type == SEND_SEND) {
		d(printf ("unsupported provider: '%s'\n", uri));
		return;
	}

	info = g_malloc0 (sizeof (*info));
	info->type = type;
	info->progress_bar = NULL;
	info->status_label = NULL;
	info->uri = g_strdup (uri);
	info->keep = keep;
	info->cancel = camel_operation_new (operation_status, info);
	info->cancel_button = NULL;
	info->data = data;
	info->state = SEND_ACTIVE;
	info->timeout_id = 0;

	d(printf("Adding new info %p\n", info));

	g_hash_table_insert (data->active, info->uri, info);

	switch (info->type) {
	case SEND_RECEIVE:
		mail_fetch_mail (info->uri, info->keep,
				 FILTER_SOURCE_INCOMING,
				 info->cancel,
				 receive_get_folder, info,
				 receive_status, info,
				 receive_done, info);
		break;
	case SEND_SEND:
		/* todo, store the folder in info? */
		outbox_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX);
		mail_send_queue (outbox_folder, info->uri,
				 FILTER_SOURCE_OUTGOING,
				 info->cancel,
				 receive_get_folder, info,
				 receive_status, info,
				 receive_done, info);
		break;
	case SEND_UPDATE:
		mail_get_store (info->uri, info->cancel, receive_update_got_store, info);
		break;
	default:
		g_return_if_reached();
	}
}

void
mail_send (void)
{
	CamelFolder *outbox_folder;
	EAccountService *transport;
	struct _send_info *info;
	struct _send_data *data;
	send_info_t type;

	transport = mail_config_get_default_transport ();
	if (!transport || !transport->url)
		return;

	data = setup_send_data ();
	info = g_hash_table_lookup (data->active, SEND_URI_KEY);
	if (info != NULL) {
		info->again++;
		d(printf("send of %s still in progress\n", transport->url));
		return;
	}

	d(printf("starting non-interactive send of '%s'\n", transport->url));

	type = get_receive_type (transport->url);
	if (type == SEND_INVALID) {
		d(printf ("unsupported provider: '%s'\n", transport->url));
		return;
	}

	info = g_malloc0 (sizeof (*info));
	info->type = SEND_SEND;
	info->progress_bar = NULL;
	info->status_label = NULL;
	info->uri = g_strdup (transport->url);
	info->keep = FALSE;
	info->cancel = NULL; 
	info->cancel_button = NULL;
	info->data = data;
	info->state = SEND_ACTIVE;
	info->timeout_id = 0;

	d(printf("Adding new info %p\n", info));

	g_hash_table_insert (data->active, SEND_URI_KEY, info);

	/* todo, store the folder in info? */
	outbox_folder = mail_component_get_folder(NULL, MAIL_COMPONENT_FOLDER_OUTBOX);
	mail_send_queue (outbox_folder, info->uri,
			 FILTER_SOURCE_OUTGOING,
			 info->cancel,
			 receive_get_folder, info,
			 receive_status, info,
			 receive_done, info);
}

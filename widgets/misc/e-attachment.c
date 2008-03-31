/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 *  Authors: Ettore Perazzoli <ettore@ximian.com>
 *           Jeffrey Stedfast <fejj@ximian.com>
 *	     Srinivasa Ragavan <sragavan@novell.com>
 *
 *  Copyright 1999-2005 Novell, Inc. (www.novell.com)
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

#include <glib.h>

#ifdef G_OS_WIN32
/* Include <windows.h> early (as the gnome-vfs stuff below will
 * include it anyway, sigh) to workaround the DATADIR problem.
 * <windows.h> (and the headers it includes) stomps all over the
 * namespace like a baboon on crack, and especially the DATADIR enum
 * in objidl.h causes problems.
 */
#undef DATADIR
#define DATADIR crap_DATADIR
#include <windows.h>
#undef DATADIR
#endif

#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include <camel/camel.h>
#include <gtk/gtk.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkdialog.h>
#include <libgnomevfs/gnome-vfs-mime.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libebook/e-vcard.h>

#include "e-util/e-error.h"
#include "e-util/e-mktemp.h"
#include "e-util/e-util-private.h"

#include "e-attachment.h"

enum {
	CHANGED,
	UPDATE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

static void
changed (EAttachment *attachment)
{
	g_signal_emit (attachment, signals[CHANGED], 0);
}


/* GtkObject methods.  */

static void
finalise (GObject *object)
{
	EAttachment *attachment = (EAttachment *) object;
	GtkWidget *dialog;

	if (attachment->editor_gui != NULL) {
		dialog = glade_xml_get_widget (attachment->editor_gui, "dialog");
		g_signal_emit_by_name (dialog, "response", GTK_RESPONSE_CLOSE);
	}

	if (attachment->is_available_local) {
		camel_object_unref (attachment->body);
		if (attachment->pixbuf_cache != NULL)
			g_object_unref (attachment->pixbuf_cache);
	} else {
		if (attachment->handle)
			gnome_vfs_async_cancel(attachment->handle);
		g_free (attachment->description);
	}

	g_free (attachment->file_name);
	g_free (attachment->store_uri);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}


/* Signals.  */

static void
real_changed (EAttachment *attachment)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));
}

static void
real_update_attachment (EAttachment *attachment, char *msg)
{
	g_return_if_fail (E_IS_ATTACHMENT (attachment));
}


static void
class_init (EAttachmentClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass*) klass;
	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	object_class->finalize = finalise;
	klass->changed = real_changed;
	klass->update = real_update_attachment;

	signals[CHANGED] = g_signal_new ("changed",
					 E_TYPE_ATTACHMENT,
					 G_SIGNAL_RUN_FIRST,
					 G_STRUCT_OFFSET (EAttachmentClass, changed),
					 NULL,
					 NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);
	signals[UPDATE] = g_signal_new ("update",
					 E_TYPE_ATTACHMENT,
					 G_SIGNAL_RUN_FIRST,
					 G_STRUCT_OFFSET (EAttachmentClass, update),
					 NULL,
					 NULL,
					 g_cclosure_marshal_VOID__VOID,
					 G_TYPE_NONE, 0);

}

static void
init (EAttachment *attachment)
{
	attachment->editor_gui = NULL;
	attachment->body = NULL;
	attachment->size = 0;
	attachment->pixbuf_cache = NULL;
	attachment->index = -1;
	attachment->file_name = NULL;
	attachment->percentage = -1;
	attachment->description = NULL;
	attachment->disposition = FALSE;
	attachment->sign = CAMEL_CIPHER_VALIDITY_SIGN_NONE;
	attachment->encrypt = CAMEL_CIPHER_VALIDITY_ENCRYPT_NONE;
	attachment->store_uri = NULL;
}

GType
e_attachment_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (EAttachmentClass),
			NULL,
			NULL,
			(GClassInitFunc) class_init,
			NULL,
			NULL,
			sizeof (EAttachment),
			0,
			(GInstanceInitFunc) init,
		};

		type = g_type_register_static (G_TYPE_OBJECT, "EAttachment", &info, 0);
	}

	return type;
}

/**
 * file_ext_is:
 * @param file_name: path for file
 * @param ext: desired extension, with a dot
 * @return if file_name has extension ext or not
 **/

static gboolean
file_ext_is (const char *file_name, const char *ext)
{
	int i, dot = -1;

	if (!file_name || !ext)
		return FALSE;

	for (i = 0; file_name[i]; i++) {
		if (file_name [i] == '.')
			dot = i;
	}

	if (dot > 0) {
		return 0 == g_ascii_strcasecmp (file_name + dot, ext);
	}

	return FALSE;
}

static char *
attachment_guess_mime_type (const char *file_name)
{
	GnomeVFSFileInfo *info;
	GnomeVFSResult result;
	char *type = NULL;

	info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (file_name, info,
					  GNOME_VFS_FILE_INFO_GET_MIME_TYPE |
					  GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE |
					  GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	if (result != GNOME_VFS_OK) {
		CamelURL *url;
		char *uri;

		url = camel_url_new ("file://", NULL);
		camel_url_set_path (url, file_name);
		uri = camel_url_to_string (url, 0);
		camel_url_free (url);

		result = gnome_vfs_get_file_info (uri, info, GNOME_VFS_FILE_INFO_GET_MIME_TYPE | GNOME_VFS_FILE_INFO_FORCE_SLOW_MIME_TYPE | GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

		g_free (uri);
	}

	if (result == GNOME_VFS_OK) {
		gchar *content = NULL;

		type = g_strdup (gnome_vfs_file_info_get_mime_type (info));

		if (type && strcmp (type, "text/directory") == 0 &&
		    file_ext_is (file_name, ".vcf") &&
		    g_file_get_contents (file_name, &content, NULL, NULL) &&
		    content) {
			EVCard *vc = e_vcard_new_from_string (content);

			if (vc) {
				g_free (type);
				g_object_unref (G_OBJECT (vc));

				type = g_strdup ("text/x-vcard");
			}

		}

		g_free (content);
	}

	gnome_vfs_file_info_unref (info);

	if (type) {
		/* gnome_vfs can sometimes return invalid type, so check for it */
		CamelContentType *ctype = camel_content_type_decode (type);

		if (!ctype) {
			g_free (type);
			type = NULL;
		} else
			camel_content_type_unref (ctype);
	}

	return type;
}


/**
 * e_attachment_new:
 * @file_name: filename to attach
 * @disposition: Content-Disposition of the attachment
 * @ex: exception
 *
 * Return value: the new attachment, or %NULL on error
 **/
EAttachment *
e_attachment_new (const char *file_name, const char *disposition, CamelException *ex)
{
	EAttachment *new;
	CamelMimePart *part;
	CamelDataWrapper *wrapper;
	CamelStream *stream;
	struct stat statbuf;
	char *mime_type;
	char *filename;
	CamelURL *url;

	g_return_val_if_fail (file_name != NULL, NULL);

	if (g_stat (file_name, &statbuf) < 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: %s"),
				      file_name, g_strerror (errno));
		return NULL;
	}

	/* return if it's not a regular file */
	if (!S_ISREG (statbuf.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: not a regular file"),
				      file_name);
		return NULL;
	}

	if (!(stream = camel_stream_fs_new_with_name (file_name, O_RDONLY, 0))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: %s"),
				      file_name, g_strerror (errno));
		return NULL;
	}

	if ((mime_type = attachment_guess_mime_type (file_name))) {
		if (!g_ascii_strcasecmp (mime_type, "message/rfc822")) {
			wrapper = (CamelDataWrapper *) camel_mime_message_new ();
		} else {
			wrapper = camel_data_wrapper_new ();
		}

		camel_data_wrapper_construct_from_stream (wrapper, stream);
		camel_data_wrapper_set_mime_type (wrapper, mime_type);
		g_free (mime_type);
	} else {
		wrapper = camel_data_wrapper_new ();
		camel_data_wrapper_construct_from_stream (wrapper, stream);
		camel_data_wrapper_set_mime_type (wrapper, "application/octet-stream");
	}

	camel_object_unref (stream);

	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (wrapper);

	camel_mime_part_set_disposition (part, disposition);
	filename = g_path_get_basename (file_name);
	camel_mime_part_set_filename (part, filename);

#if 0
	/* Note: Outlook 2002 is broken with respect to Content-Ids on
           non-multipart/related parts, so as an interoperability
           workaround, don't set a Content-Id on these parts. Fixes
           bug #10032 */
	/* set the Content-Id */
	content_id = camel_header_msgid_generate ();
	camel_mime_part_set_content_id (part, content_id);
	g_free (content_id);
#endif

	new = g_object_new (E_TYPE_ATTACHMENT, NULL);
	new->editor_gui = NULL;
	new->body = part;
	new->size = statbuf.st_size;
	new->guessed_type = TRUE;
	new->handle = NULL;
	new->is_available_local = TRUE;
	new->file_name = filename;

	url = camel_url_new ("file://", NULL);
	camel_url_set_path (url, file_name);
	new->store_uri = camel_url_to_string (url, 0);
	camel_url_free (url);

	return new;
}


typedef struct DownloadInfo {
	EAttachment *attachment;
	char *file_name;
	char *uri;
	GtkWindow *parent; /* for error dialog */
} DownloadInfo;

static int
async_progress_update_cb (GnomeVFSAsyncHandle      *handle,
			  GnomeVFSXferProgressInfo *info,
			  DownloadInfo *download_info)
{
	switch (info->status) {
	case GNOME_VFS_XFER_PROGRESS_STATUS_OK:
		if (info->file_size) {
			download_info->attachment->percentage = info->bytes_copied*100/info->file_size;
			g_signal_emit (download_info->attachment, signals[UPDATE], 0);
		} else {
			download_info->attachment->percentage = 0;
			g_signal_emit (download_info->attachment, signals[UPDATE], 0);
		}

		if (info->phase == GNOME_VFS_XFER_PHASE_COMPLETED) {
			CamelException ex;

			if (!info->file_size) {
				if (info->vfs_status == GNOME_VFS_OK)
					info->vfs_status = GNOME_VFS_ERROR_EOF;
				goto error_msg;
			}

			download_info->attachment->handle = NULL;
			camel_exception_init (&ex);
			e_attachment_build_remote_file (download_info->file_name, download_info->attachment, "attachment", &ex);
			if (camel_exception_is_set (&ex)) {
				e_error_run (download_info->parent, "mail-composer:no-attach",
						download_info->uri, camel_exception_get_description (&ex), NULL);
				camel_exception_clear (&ex);
				goto error;
			}
			download_info->attachment->percentage = -1;
			download_info->attachment->is_available_local = TRUE;
			g_signal_emit (download_info->attachment, signals[UPDATE], 0);
			g_free (download_info->file_name);
			g_free (download_info->uri);
			g_free (download_info);
		}
		return TRUE;
	case GNOME_VFS_XFER_PROGRESS_STATUS_VFSERROR:
	error_msg:
		e_error_run (download_info->parent, "mail-composer:no-attach",
				download_info->uri, gnome_vfs_result_to_string (info->vfs_status), NULL);
	error:
		g_object_unref (download_info->attachment);
		g_free (download_info->file_name);
		g_free (download_info->uri);
		g_free (download_info);
		return FALSE;
	default:
		break;
	}

	return TRUE;
}

static void
download_to_local_path (GnomeVFSURI *source_uri, GnomeVFSURI *target_uri, DownloadInfo *download_info)

{
	GList *source_uri_list;
	GList *target_uri_list;

	source_uri_list = g_list_append (NULL, source_uri);
	target_uri_list = g_list_append (NULL, target_uri);

	/* Callback info */
	gnome_vfs_async_xfer (&download_info->attachment->handle,    /* handle_return   */
			      source_uri_list,                       /* source_uri_list */
			      target_uri_list,                       /* target_uri_list */
			      GNOME_VFS_XFER_DEFAULT,                /* xfer_options    */
			      GNOME_VFS_XFER_ERROR_MODE_ABORT,       /* error_mode      */
			      GNOME_VFS_XFER_OVERWRITE_MODE_REPLACE, /* overwrite_mode  */
			      GNOME_VFS_PRIORITY_DEFAULT,            /* priority        */
			      (GnomeVFSAsyncXferProgressCallback) async_progress_update_cb,
			      download_info,                         /* update_callback_data   */
			      NULL,                                  /* progress_sync_callback */
			      NULL);                                 /* sync_callback_data     */
}

EAttachment *
e_attachment_new_remote_file (GtkWindow *error_dlg_parent, const char *uri, const char *disposition, const char *path, CamelException *ex)
{
	EAttachment *new;
	DownloadInfo *download_info;
	CamelURL *url;
	char *base;

	g_return_val_if_fail (uri != NULL, NULL);

	url = camel_url_new (uri, NULL);
	base = g_path_get_basename (url->path);
	camel_url_free (url);

	new = g_object_new (E_TYPE_ATTACHMENT, NULL);
	new->editor_gui = NULL;
	new->body = NULL;
	new->size = 0;
	new->guessed_type = FALSE;
	new->handle = NULL;
	new->is_available_local = FALSE;
	new->percentage = 0;
	new->file_name = g_build_filename (path, base, NULL);

	g_free (base);

	download_info = g_new (DownloadInfo, 1);
	download_info->attachment = new;
	download_info->file_name = g_strdup (new->file_name);
	download_info->uri = g_strdup (uri);
	download_info->parent = error_dlg_parent;
	download_to_local_path (gnome_vfs_uri_new (uri), gnome_vfs_uri_new (new->file_name), download_info);

	return new;
}


void
e_attachment_build_remote_file (const char *file_name, EAttachment *attachment, const char *disposition, CamelException *ex)
{
	CamelMimePart *part;
	CamelDataWrapper *wrapper;
	CamelStream *stream;
	struct stat statbuf;
	char *mime_type;
	char *filename;
	CamelURL *url;

	g_return_if_fail (file_name != NULL);

	if (g_stat (file_name, &statbuf) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: %s"),
				      file_name, g_strerror (errno));
		g_message ("Cannot attach file %s: %s\n", file_name, g_strerror (errno));
		return;
	}

	/* return if it's not a regular file */
	if (!S_ISREG (statbuf.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: not a regular file"),
				      file_name);
		g_message ("Cannot attach file %s: not a regular file", file_name);
		return;
	}

	if (!(stream = camel_stream_fs_new_with_name (file_name, O_RDONLY, 0))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot attach file %s: %s"),
				      file_name, g_strerror (errno));
		return;
	}

	if ((mime_type = attachment_guess_mime_type (file_name))) {
		if (!g_ascii_strcasecmp (mime_type, "message/rfc822")) {
			wrapper = (CamelDataWrapper *) camel_mime_message_new ();
		} else {
			wrapper = camel_data_wrapper_new ();
		}

		camel_data_wrapper_construct_from_stream (wrapper, stream);
		camel_data_wrapper_set_mime_type (wrapper, mime_type);
		g_free (mime_type);
	} else {
		wrapper = camel_data_wrapper_new ();
		camel_data_wrapper_construct_from_stream (wrapper, stream);
		camel_data_wrapper_set_mime_type (wrapper, "application/octet-stream");
	}

	camel_object_unref (stream);

	part = camel_mime_part_new ();
	camel_medium_set_content_object (CAMEL_MEDIUM (part), wrapper);
	camel_object_unref (wrapper);

	if (attachment->disposition)
		camel_mime_part_set_disposition (part, "inline");
	else
		camel_mime_part_set_disposition (part, "attachment");

	if (!attachment->file_name)
		filename = g_path_get_basename (file_name);
	else
		filename = g_path_get_basename (attachment->file_name);

	camel_mime_part_set_filename (part, filename);

	if (attachment->description) {
		camel_mime_part_set_description (part, attachment->description);
		g_free (attachment->description);
		attachment->description = NULL;
	}

	attachment->editor_gui = NULL;
	attachment->body = part;
	attachment->size = statbuf.st_size;
	attachment->guessed_type = TRUE;
	g_free (attachment->file_name);
	attachment->file_name = filename;

	url = camel_url_new ("file://", NULL);
	camel_url_set_path (url, file_name);
	attachment->store_uri = camel_url_to_string (url, 0);
	camel_url_free (url);

}


/**
 * e_attachment_new_from_mime_part:
 * @part: a CamelMimePart
 *
 * Return value: a new EAttachment based on the mime part
 **/
EAttachment *
e_attachment_new_from_mime_part (CamelMimePart *part)
{
	EAttachment *new;

	g_return_val_if_fail (CAMEL_IS_MIME_PART (part), NULL);

	new = g_object_new (E_TYPE_ATTACHMENT, NULL);
	new->editor_gui = NULL;
	camel_object_ref (part);
	new->body = part;
	new->guessed_type = FALSE;
	new->is_available_local = TRUE;
	new->size = camel_mime_part_get_content_size (part);
	new->file_name = g_strdup (camel_mime_part_get_filename(part));

	return new;
}


/* The attachment property dialog.  */

typedef struct {
	GtkWidget *dialog;
	GtkEntry *file_name_entry;
	GtkEntry *description_entry;
	GtkEntry *mime_type_entry;
	GtkToggleButton *disposition_checkbox;
	EAttachment *attachment;
} DialogData;

static void
destroy_dialog_data (DialogData *data)
{
	g_free (data);
}

/*
 * fixme: I am converting EVERYTHING to/from UTF-8, although mime types
 * are in ASCII. This is not strictly necessary, but we want to be
 * consistent and possibly check for errors somewhere.
 */

static void
set_entry (GladeXML *xml, const char *widget_name, const char *value)
{
	GtkEntry *entry;

	entry = GTK_ENTRY (glade_xml_get_widget (xml, widget_name));
	if (entry == NULL)
		g_warning ("Entry for `%s' not found.", widget_name);
	else
		gtk_entry_set_text (entry, value ? value : "");
}

static void
connect_widget (GladeXML *gui, const char *name, const char *signal_name,
		GCallback func, gpointer data)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (gui, name);
	g_signal_connect (widget, signal_name, func, data);
}

static void
close_cb (GtkWidget *widget, gpointer data)
{
	EAttachment *attachment;
	DialogData *dialog_data;

	dialog_data = (DialogData *) data;
	attachment = dialog_data->attachment;

	gtk_widget_destroy (dialog_data->dialog);
	g_object_unref (attachment->editor_gui);
	attachment->editor_gui = NULL;

	destroy_dialog_data (dialog_data);
}

static void
ok_cb (GtkWidget *widget, gpointer data)
{
	DialogData *dialog_data;
	EAttachment *attachment;
	const char *str;

	dialog_data = (DialogData *) data;
	attachment = dialog_data->attachment;

	str = gtk_entry_get_text (dialog_data->file_name_entry);
	if (attachment->is_available_local)
		camel_mime_part_set_filename (attachment->body, str);
	g_free (attachment->file_name);
	attachment->file_name = g_strdup (str);

	str = gtk_entry_get_text (dialog_data->description_entry);
	if (attachment->is_available_local) {
		camel_mime_part_set_description (attachment->body, str);
	} else {
		g_free (attachment->description);
		attachment->description = g_strdup (str);
	}

	str = gtk_entry_get_text (dialog_data->mime_type_entry);
	if (attachment->is_available_local) {
		camel_mime_part_set_content_type (attachment->body, str);
		camel_data_wrapper_set_mime_type(camel_medium_get_content_object(CAMEL_MEDIUM (attachment->body)), str);
	}

	if (attachment->is_available_local) {
		switch (gtk_toggle_button_get_active (dialog_data->disposition_checkbox)) {
		case 0:
			camel_mime_part_set_disposition (attachment->body, "attachment");
			break;
		case 1:
			camel_mime_part_set_disposition (attachment->body, "inline");
			break;
		default:
			/* Hmmmm? */
			break;
		}
	} else {
		attachment->disposition = gtk_toggle_button_get_active (dialog_data->disposition_checkbox);
	}

	changed (attachment);
	close_cb (widget, data);
}

static void
response_cb (GtkWidget *widget, gint response, gpointer data)
{
	if (response == GTK_RESPONSE_OK)
		ok_cb (widget, data);
	else
		close_cb (widget, data);
}

void
e_attachment_edit (EAttachment *attachment, GtkWidget *parent)
{
	CamelContentType *content_type;
	const char *disposition;
	DialogData *dialog_data;
	GladeXML *editor_gui;
	GtkWidget *window;
	char *type;
	char *filename;

	g_return_if_fail (E_IS_ATTACHMENT (attachment));

	if (attachment->editor_gui != NULL) {
		window = glade_xml_get_widget (attachment->editor_gui, "dialog");
		gdk_window_show (window->window);
		return;
	}

	filename = g_build_filename (EVOLUTION_GLADEDIR, "e-attachment.glade", NULL);
	editor_gui = glade_xml_new (filename, NULL, NULL);
	g_free (filename);

	if (editor_gui == NULL) {
		g_warning ("Cannot load `e-attachment.glade'");
		return;
	}

	attachment->editor_gui = editor_gui;

	gtk_window_set_transient_for (GTK_WINDOW (glade_xml_get_widget (editor_gui, "dialog")),
				      GTK_WINDOW (gtk_widget_get_toplevel (parent)));

	dialog_data = g_new (DialogData, 1);
	dialog_data->attachment = attachment;
	dialog_data->dialog = glade_xml_get_widget (editor_gui, "dialog");
	dialog_data->file_name_entry = GTK_ENTRY (glade_xml_get_widget (editor_gui, "file_name_entry"));
	dialog_data->description_entry = GTK_ENTRY (glade_xml_get_widget (editor_gui, "description_entry"));
	dialog_data->mime_type_entry = GTK_ENTRY (glade_xml_get_widget (editor_gui, "mime_type_entry"));
	dialog_data->disposition_checkbox = GTK_TOGGLE_BUTTON (glade_xml_get_widget (editor_gui, "disposition_checkbox"));

	if (attachment->is_available_local && attachment->body) {
		set_entry (editor_gui, "file_name_entry", camel_mime_part_get_filename (attachment->body));
		set_entry (editor_gui, "description_entry", camel_mime_part_get_description (attachment->body));
		content_type = camel_mime_part_get_content_type (attachment->body);
		type = camel_content_type_simple (content_type);
		set_entry (editor_gui, "mime_type_entry", type);
		g_free (type);

		disposition = camel_mime_part_get_disposition (attachment->body);
		gtk_toggle_button_set_active (dialog_data->disposition_checkbox,
					      disposition && !g_ascii_strcasecmp (disposition, "inline"));
	} else {
		set_entry (editor_gui, "file_name_entry", attachment->file_name);
		set_entry (editor_gui, "description_entry", attachment->description);
		if ((type = attachment_guess_mime_type (attachment->file_name))) {
			set_entry (editor_gui, "mime_type_entry", type);
			g_free (type);
		} else {
			set_entry (editor_gui, "mime_type_entry", "");
		}

		gtk_toggle_button_set_active (dialog_data->disposition_checkbox, attachment->disposition);
	}

	connect_widget (editor_gui, "dialog", "response", (GCallback)response_cb, dialog_data);

	/* make sure that when the parent gets hidden/closed that our windows also close */
	parent = gtk_widget_get_toplevel (parent);
	gtk_signal_connect_while_alive (GTK_OBJECT (parent), "destroy", (GCallback) close_cb, dialog_data,
					GTK_OBJECT (dialog_data->dialog));
	gtk_signal_connect_while_alive (GTK_OBJECT (parent), "hide", (GCallback) close_cb, dialog_data,
					GTK_OBJECT (dialog_data->dialog));
}

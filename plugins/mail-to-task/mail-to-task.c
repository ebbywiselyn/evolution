
/* Copyright (C) 2004 Novell, Inc */
/* Authors: Michael Zucchi
            Rodrigo Moya */

/* This file is licensed under the GNU GPL v2 or later */

/* Convert a mail message into a task */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>

#include <gconf/gconf-client.h>
#include <libecal/e-cal.h>
#include <libedataserverui/e-source-selector-dialog.h>
#include <camel/camel-folder.h>
#include <camel/camel-medium.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-multipart.h>
#include <camel/camel-stream.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-utf8.h>
#include "mail/em-menu.h"
#include "mail/em-popup.h"
#include "mail/em-utils.h"
#include "mail/em-folder-view.h"
#include "mail/em-format-html.h"
#include "e-util/e-dialog-utils.h"
#include <gtkhtml/gtkhtml.h>
#include <calendar/common/authentication.h>

typedef struct {
	ECal *client;
	struct _CamelFolder *folder;
	GPtrArray *uids;
	char *selected_text;
}AsyncData;

static char *
clean_name(const unsigned char *s)
{
	GString *out = g_string_new("");
	guint32 c;
	char *r;

	while ((c = camel_utf8_getc ((const unsigned char **)&s)))
	{
		if (!g_unichar_isprint (c) || ( c < 0x7f && strchr (" /'\"`&();|<>$%{}!", c )))
			c = '_';
		g_string_append_u (out, c);
	}

	r = g_strdup (out->str);
	g_string_free (out, TRUE);

	return r;
}

static void
set_attendees (ECalComponent *comp, CamelMimeMessage *message)
{
	GSList *attendees = NULL, *l, *to_free = NULL;
	ECalComponentAttendee *ca;
	const CamelInternetAddress *to, *cc, *bcc, *arr[3];
	int len, i, j;

	to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);
	bcc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_BCC);

	arr[0] = to, arr[1] = cc, arr[2] = bcc;

	for(j = 0; j < 3; j++)
	{
		len = CAMEL_ADDRESS (arr[j])->addresses->len;
		for (i = 0; i < len; i++) {
			const char *name, *addr;

			if (camel_internet_address_get (arr[j], i, &name, &addr)) {
				char *temp;

				ca = g_new0 (ECalComponentAttendee, 1);

				temp = g_strconcat ("mailto:", addr, NULL);
				ca->value = temp;
				to_free = g_slist_prepend (to_free, temp);

				ca->cn = name;
				/* FIXME: missing many fields */

				attendees = g_slist_append (attendees, ca);
			}
		}
	}

	e_cal_component_set_attendee_list (comp, attendees);

	for (l = attendees; l != NULL; l = l->next)
		g_free (l->data);

	g_slist_foreach (to_free, (GFunc) g_free, NULL);

	g_slist_free (to_free);
	g_slist_free (attendees);
}

static void
set_description (ECalComponent *comp, CamelMimeMessage *message)
{
	CamelDataWrapper *content;
	CamelStream *mem;
	CamelContentType *type;
	CamelMimePart *mime_part = CAMEL_MIME_PART (message);
	ECalComponentText text;
	GSList sl;
	char *str, *convert_str = NULL;
	gsize bytes_read, bytes_written;
	gint count = 2;

	content = camel_medium_get_content_object ((CamelMedium *) message);
	if (!content)
		return;

	/*
	 * Get non-multipart content from multipart message.
	 */
	while (CAMEL_IS_MULTIPART (content) && count > 0)
	{
		mime_part = camel_multipart_get_part (CAMEL_MULTIPART (content), 0);
		content = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
		count--;
	}

	if (!mime_part)
		return;

	type = camel_mime_part_get_content_type (mime_part);
	if (!camel_content_type_is (type, "text", "plain"))
		return;

	mem = camel_stream_mem_new ();
	camel_data_wrapper_decode_to_stream (content, mem);

	str = g_strndup ((const gchar*)((CamelStreamMem *) mem)->buffer->data, ((CamelStreamMem *) mem)->buffer->len);
	camel_object_unref (mem);

	/* convert to UTF-8 string */
	if (str && content->mime_type->params && content->mime_type->params->value)
	{
		convert_str = g_convert (str, strlen (str),
					 "UTF-8", content->mime_type->params->value,
					 &bytes_read, &bytes_written, NULL);
	}

	if (convert_str)
		text.value = convert_str;
	else
		text.value = str;
	text.altrep = NULL;
	sl.next = NULL;
	sl.data = &text;

	e_cal_component_set_description_list (comp, &sl);

	g_free (str);
	if (convert_str)
		g_free (convert_str);
}

static void
set_organizer (ECalComponent *comp, CamelMimeMessage *message)
{
	const CamelInternetAddress *address;
	const char *str, *name;
	ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};
	char *temp;

	if (message->reply_to)
		address = message->reply_to;
	else if (message->from)
		address = message->from;
	else
		return;

	if (!camel_internet_address_get (address, 0, &name, &str))
		return;

	temp = g_strconcat ("mailto:", str, NULL);
	organizer.value = temp;
	organizer.cn = name;
	e_cal_component_set_organizer (comp, &organizer);

	g_free (temp);
}

static void
set_attachments (ECal *client, ECalComponent *comp, CamelMimeMessage *message)
{
	int parts, i;
	GSList *list = NULL;
	const char *uid;
	const char *store_uri;
	char *store_dir;
	CamelDataWrapper *content;

	content = camel_medium_get_content_object ((CamelMedium *) message);
	if (!content || !CAMEL_IS_MULTIPART (content))
		return;

	parts = camel_multipart_get_number (CAMEL_MULTIPART (content));
	if (parts < 1)
		return;

	e_cal_component_get_uid (comp, &uid);
	store_uri = e_cal_get_local_attachment_store (client);
	if (!store_uri)
		return;
	store_dir = g_filename_from_uri (store_uri, NULL, NULL);

	for (i = 1; i < parts; i++)
	{
		char *filename, *path, *tmp;
		const char *orig_filename;
		CamelMimePart *mime_part;

		mime_part = camel_multipart_get_part (CAMEL_MULTIPART (content), i);

		orig_filename = camel_mime_part_get_filename (mime_part);
		if (!orig_filename)
			continue;

		tmp = clean_name ((const unsigned char *)orig_filename);
		filename = g_strdup_printf ("%s-%s", uid, tmp);
		path = g_build_filename (store_dir, filename, NULL);

		if (em_utils_save_part_to_file (NULL, path, mime_part))
		{
			char *uri;
			uri = g_filename_to_uri (path, NULL, NULL);
			list = g_slist_append (list, g_strdup (uri));
			g_free (uri);
		}

		g_free (tmp);
		g_free (filename);
		g_free (path);
	}

	g_free (store_dir);

	e_cal_component_set_attachment_list (comp, list);
}

static gboolean
do_mail_to_task (AsyncData *data)
{
	ECal *client = data->client;
	struct _CamelFolder *folder = data->folder;
	GPtrArray *uids = data->uids;
	GError *err = NULL;
	gboolean readonly = FALSE;

	/* open the task client */
	if (!e_cal_open (client, FALSE, &err)) {
		e_notice (NULL, GTK_MESSAGE_ERROR, _("Cannot open calendar. %s"), err ? err->message : "");
	} else if (!e_cal_is_read_only (client, &readonly, &err) || readonly) {
		if (err)
			e_notice (NULL, GTK_MESSAGE_ERROR, err->message);
		else
			e_notice (NULL, GTK_MESSAGE_ERROR, _("Selected source is read only, thus cannot create task there. Select other source, please."));
	} else {
		int i;

		for (i = 0; i < (uids ? uids->len : 0); i++) {
			CamelMimeMessage *message;
			ECalComponent *comp;
			ECalComponentText text;
			icalproperty *icalprop;
			icalcomponent *icalcomp;

			/* retrieve the message from the CamelFolder */
			message = camel_folder_get_message (folder, g_ptr_array_index (uids, i), NULL);
			if (!message) {
				continue;
			}

			comp = e_cal_component_new ();
			e_cal_component_set_new_vtype (comp, E_CAL_COMPONENT_TODO);
			e_cal_component_set_uid (comp, camel_mime_message_get_message_id (message));

			/* set the task's summary */
			text.value = camel_mime_message_get_subject (message);
			text.altrep = NULL;
			e_cal_component_set_summary (comp, &text);

			/* set all fields */
			if (data->selected_text) {
				GSList sl;

				text.value = data->selected_text;
				text.altrep = NULL;
				sl.next = NULL;
				sl.data = &text;

				e_cal_component_set_description_list (comp, &sl);
			} else
				set_description (comp, message);
			set_organizer (comp, message);
			set_attendees (comp, message);

			/* set attachment files */
			set_attachments (client, comp, message);

			icalcomp = e_cal_component_get_icalcomponent (comp);

			icalprop = icalproperty_new_x ("1");
			icalproperty_set_x_name (icalprop, "X-EVOLUTION-MOVE-CALENDAR");
			icalcomponent_add_property (icalcomp, icalprop);

			/* save the task to the selected source */
			if (!e_cal_create_object (client, icalcomp, NULL, &err)) {
				g_warning ("Could not create object: %s", err ? err->message : "Unknown error");

				if (err)
					g_error_free (err);
				err = NULL;
			}

			g_object_unref (comp);
		}
	}

	/* free memory */
	g_object_unref (data->client);
	g_ptr_array_free (data->uids, TRUE);
	g_free (data->selected_text);
	g_free (data);
	data = NULL;

	if (err)
		g_error_free (err);

	return TRUE;
}

void org_gnome_mail_to_task (void *ep, EMPopupTargetSelect *t);
void org_gnome_mail_to_task_menu (EPlugin *ep, EMMenuTargetSelect *target);

static void
copy_uids (char *uid, GPtrArray *uid_array)
{
	g_ptr_array_add (uid_array, g_strdup (uid));
}

static gboolean
text_contains_nonwhitespace (const char *text, gint len)
{
	const char *p;
	gunichar c = 0;

	if (!text || len<=0)
		return FALSE;

	p = text;

	while (p && p - text < len) {
		c = g_utf8_get_char (p);
		if (!c)
			break;

		if (!g_unichar_isspace (c))
			break;

		p = g_utf8_next_char (p);
	}

	return p - text < len - 1 && c != 0;
}

/* should be freed with g_free after done with it */
static char *
get_selected_text (EMFolderView *emfv)
{
	char *text = NULL;
	gint len;

	if (!emfv || !emfv->preview || !gtk_html_command (((EMFormatHTML *)emfv->preview)->html, "is-selection-active"))
		return NULL;

	if (gtk_html_command (((EMFormatHTML *)emfv->preview)->html, "is-selection-active")
	    && (text = gtk_html_get_selection_plain_text (((EMFormatHTML *)emfv->preview)->html, &len))
	    && len && text && text[0] && text_contains_nonwhitespace (text, len)) {
		/* selection is ok, so use it as returned from gtkhtml widget */
	} else {
		g_free (text);
		text = NULL;
	}

	return text;
}

static void
convert_to_task (GPtrArray *uid_array, struct _CamelFolder *folder, EMFolderView *emfv)
{
	GtkWidget *dialog;
	GConfClient *conf_client;
	ESourceList *source_list;

	/* ask the user which tasks list to save to */
	conf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (conf_client, "/apps/evolution/tasks/sources");

	dialog = e_source_selector_dialog_new (NULL, source_list);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		ESource *source;

		/* if a source has been selected, perform the mail2task operation */
		source = e_source_selector_dialog_peek_primary_selection (E_SOURCE_SELECTOR_DIALOG (dialog));
		if (source) {
			ECal *client = NULL;
			AsyncData *data = NULL;
			GThread *thread = NULL;
			GError *error = NULL;

			client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_TODO);
			if (!client) {
				char *uri = e_source_get_uri (source);

				g_warning ("Could not create the client: %s \n", uri);

				g_free (uri);
				g_object_unref (conf_client);
				g_object_unref (source_list);
				gtk_widget_destroy (dialog);
				return;
			}

			/* Fill the elements in AsynData */
			data = g_new0 (AsyncData, 1);
			data->client = client;
			data->folder = folder;
			data->uids = uid_array;

			if (uid_array->len == 1)
				data->selected_text = get_selected_text (emfv);
			else
				data->selected_text = NULL;

			thread = g_thread_create ((GThreadFunc) do_mail_to_task, data, FALSE, &error);
			if (!thread) {
				g_warning (G_STRLOC ": %s", error->message);
				g_error_free (error);
			}

		}
	}

	g_object_unref (conf_client);
	g_object_unref (source_list);
	gtk_widget_destroy (dialog);

}

void
org_gnome_mail_to_task (void *ep, EMPopupTargetSelect *t)
{
	GPtrArray *uid_array = NULL;

	if (t->uids->len > 0) {
		/* FIXME Some how in the thread function the values inside t->uids gets freed
		   and are corrupted which needs to be fixed, this is sought of work around fix for
		   the gui inresponsiveness */
		uid_array = g_ptr_array_new ();
		g_ptr_array_foreach (t->uids, (GFunc)copy_uids, (gpointer) uid_array);
	} else {
		return;
	}

	convert_to_task (uid_array, t->folder, (EMFolderView *) t->target.widget);
}

void org_gnome_mail_to_task_menu (EPlugin *ep, EMMenuTargetSelect *t)
{
	GPtrArray *uid_array = NULL;

	if (t->uids->len > 0) {
		/* FIXME Some how in the thread function the values inside t->uids gets freed
		   and are corrupted which needs to be fixed, this is sought of work around fix for
		   the gui inresponsiveness */
		uid_array = g_ptr_array_new ();
		g_ptr_array_foreach (t->uids, (GFunc)copy_uids, (gpointer) uid_array);
	} else {
		return;
	}

	convert_to_task (uid_array, t->folder, (EMFolderView *) t->target.widget);
}

int e_plugin_lib_enable(EPluginLib *ep, int enable);

int
e_plugin_lib_enable(EPluginLib *ep, int enable)
{
	return 0;
}

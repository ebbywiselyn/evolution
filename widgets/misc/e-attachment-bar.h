/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-attachment-bar.h
 *
 * Copyright (C) 2005  Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * published by the Free Software Foundation; either version 2 of the
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
 * Author: Ettore Perazzoli
 * 	   Srinivasa Ragavan
 */

#ifndef __E_ATTACHMENT_BAR_H__
#define __E_ATTACHMENT_BAR_H__

#include <libgnomeui/gnome-icon-list.h>

#include <bonobo/bonobo-ui-node.h>
#include <bonobo/bonobo-ui-util.h>

#include <camel/camel-multipart.h>
#include "e-attachment.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_ATTACHMENT_BAR \
	(e_attachment_bar_get_type ())
#define E_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_ATTACHMENT_BAR, EAttachmentBar))
#define E_ATTACHMENT_BAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_ATTACHMENT_BAR, EAttachmentBarClass))
#define E_IS_ATTACHMENT_BAR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_ATTACHMENT_BAR))
#define E_IS_ATTACHMENT_BAR_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE ((obj), E_TYPE_ATTACHMENT_BAR))

typedef struct _EAttachmentBar EAttachmentBar;
typedef struct _EAttachmentBarClass EAttachmentBarClass;

struct _EAttachmentBar {
	GnomeIconList parent;
	gboolean expand;

	struct _EAttachmentBarPrivate *priv;
};

struct _EAttachmentBarClass {
	GnomeIconListClass parent_class;

	void (* changed) (EAttachmentBar *bar);
};


GtkType e_attachment_bar_get_type (void);

GtkWidget *e_attachment_bar_new (GtkAdjustment *adj);
void e_attachment_bar_to_multipart (EAttachmentBar *bar, CamelMultipart *multipart,
				    const char *default_charset);
guint e_attachment_bar_get_num_attachments (EAttachmentBar *bar);
void e_attachment_bar_attach (EAttachmentBar *bar, const char *file_name, const char *disposition);
void e_attachment_bar_attach_mime_part (EAttachmentBar *bar, CamelMimePart *part);
int e_attachment_bar_get_download_count (EAttachmentBar *bar);
void e_attachment_bar_attach_remote_file (EAttachmentBar *bar, const char *url, const char *disposition);
GSList *e_attachment_bar_get_attachment (EAttachmentBar *bar, int id);
void e_attachment_bar_add_attachment (EAttachmentBar *bar, EAttachment *attachment);
void e_attachment_bar_edit_selected (EAttachmentBar *bar);
void e_attachment_bar_remove_selected (EAttachmentBar *bar);
GtkWidget ** e_attachment_bar_get_selector(EAttachmentBar *bar);
GSList *e_attachment_bar_get_parts (EAttachmentBar *bar);
GSList *e_attachment_bar_get_selected (EAttachmentBar *bar);
void e_attachment_bar_set_width(EAttachmentBar *bar, int bar_width);
GSList * e_attachment_bar_get_all_attachments (EAttachmentBar *bar);
void e_attachment_bar_create_attachment_cache (EAttachment *attachment);
void 
e_attachment_bar_bonobo_ui_populate_with_recent (BonoboUIComponent *uic, const char *path,
						 EAttachmentBar *bar, 
						 BonoboUIVerbFn verb_cb, gpointer user_data);
GtkAction *
e_attachment_bar_recent_action_new (EAttachmentBar *bar, 
				const gchar *action_name,
				const gchar *action_label);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_ATTACHMENT_BAR_H__ */

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* msg-composer-hdrs.h
 *
 * Copyright (C) 1999 Ximian, Inc.
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
 */

#ifndef ___E_MSG_COMPOSER_HDRS_H__
#define ___E_MSG_COMPOSER_HDRS_H__

#include <gtk/gtk.h>

#include <bonobo/bonobo-ui-component.h>

#include <e-util/e-signature.h>
#include <libedataserver/e-account.h>
#include <camel/camel-mime-message.h>
#include <libebook/e-destination.h>

#define E_TYPE_MSG_COMPOSER_HDRS \
	(e_msg_composer_hdrs_get_type ())
#define E_MSG_COMPOSER_HDRS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MSG_COMPOSER_HDRS, EMsgComposerHdrs))
#define E_MSG_COMPOSER_HDRS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MSG_COMPOSER_HDRS, EMsgComposerHdrsClass))
#define E_IS_MSG_COMPOSER_HDRS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MSG_COMPOSER_HDRS))
#define E_IS_MSG_COMPOSER_HDRS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MSG_COMPOSER_HDRS))
#define E_MSG_COMPOSER_HDRS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MSG_COMPOSER_HDRS, EMsgComposerHdrsClass))

G_BEGIN_DECLS

typedef struct _EMsgComposerHdrs EMsgComposerHdrs;
typedef struct _EMsgComposerHdrsClass EMsgComposerHdrsClass;
typedef struct _EMsgComposerHdrsPrivate EMsgComposerHdrsPrivate;

struct _EMsgComposerHdrs {
	GtkTable parent;

	EMsgComposerHdrsPrivate *priv;

	guint32 visible_mask;
};

struct _EMsgComposerHdrsClass {
	GtkTableClass parent_class;

	void (* subject_changed) (EMsgComposerHdrs *hdrs, gchar *subject);

	void (* hdrs_changed) (EMsgComposerHdrs *hdrs);

	void (* from_changed) (EMsgComposerHdrs *hdrs);
};

typedef enum {
	E_MSG_COMPOSER_VISIBLE_FROM       = (1 << 0),
	E_MSG_COMPOSER_VISIBLE_REPLYTO    = (1 << 1),
	E_MSG_COMPOSER_VISIBLE_TO         = (1 << 2),
	E_MSG_COMPOSER_VISIBLE_CC         = (1 << 3),
	E_MSG_COMPOSER_VISIBLE_BCC        = (1 << 4),
	E_MSG_COMPOSER_VISIBLE_POSTTO     = (1 << 5),
	E_MSG_COMPOSER_VISIBLE_SUBJECT    = (1 << 7)
} EMsgComposerHeaderVisibleFlags;

#define E_MSG_COMPOSER_VISIBLE_MASK_SENDER \
	(E_MSG_COMPOSER_VISIBLE_FROM | \
	 E_MSG_COMPOSER_VISIBLE_REPLYTO)

#define E_MSG_COMPOSER_VISIBLE_MASK_BASIC \
	(E_MSG_COMPOSER_VISIBLE_MASK_SENDER | \
	 E_MSG_COMPOSER_VISIBLE_SUBJECT)

#define E_MSG_COMPOSER_VISIBLE_MASK_RECIPIENTS \
	(E_MSG_COMPOSER_VISIBLE_TO | \
	 E_MSG_COMPOSER_VISIBLE_CC | \
	 E_MSG_COMPOSER_VISIBLE_BCC)

#define E_MSG_COMPOSER_VISIBLE_MASK_MAIL \
	(E_MSG_COMPOSER_VISIBLE_MASK_BASIC | \
	 E_MSG_COMPOSER_VISIBLE_MASK_RECIPIENTS)

#define E_MSG_COMPOSER_VISIBLE_MASK_POST \
	(E_MSG_COMPOSER_VISIBLE_MASK_BASIC | \
	 E_MSG_COMPOSER_VISIBLE_POSTTO)

GType		e_msg_composer_hdrs_get_type	(void);
GtkWidget  *e_msg_composer_hdrs_new                (BonoboUIComponent *uic, int visible_mask, int visible_flags);

void        e_msg_composer_hdrs_to_message         (EMsgComposerHdrs *hdrs,
						    CamelMimeMessage *msg);

void        e_msg_composer_hdrs_to_redirect        (EMsgComposerHdrs *hdrs,
						    CamelMimeMessage *msg);


EAccount *  e_msg_composer_hdrs_get_from_account   (EMsgComposerHdrs *hdrs);
gboolean    e_msg_composer_hdrs_set_from_account   (EMsgComposerHdrs *hdrs,
						    const char *account_name);
ESignature *e_msg_composer_hdrs_get_signature      (EMsgComposerHdrs *hdrs);
gboolean    e_msg_composer_hdrs_set_signature      (EMsgComposerHdrs *hdrs,
						    ESignature *signature);
void        e_msg_composer_hdrs_set_reply_to       (EMsgComposerHdrs *hdrs,
						    const char *reply_to);
void        e_msg_composer_hdrs_set_to             (EMsgComposerHdrs *hdrs,
						    EDestination    **to_destv);
void        e_msg_composer_hdrs_set_cc             (EMsgComposerHdrs *hdrs,
						    EDestination    **cc_destv);
void        e_msg_composer_hdrs_set_bcc            (EMsgComposerHdrs *hdrs,
						    EDestination    **bcc_destv);
void        e_msg_composer_hdrs_set_post_to        (EMsgComposerHdrs *hdrs,
						    const char       *post_to);
void        e_msg_composer_hdrs_set_post_to_list   (EMsgComposerHdrs *hdrs,
						    GList *urls);
void        e_msg_composer_hdrs_set_post_to_base   (EMsgComposerHdrs *hdrs,
					            const gchar      *base,
						    const gchar      *post_to);
void        e_msg_composer_hdrs_set_subject        (EMsgComposerHdrs *hdrs,
						    const char       *subject);

CamelInternetAddress *e_msg_composer_hdrs_get_from (EMsgComposerHdrs *hdrs);
CamelInternetAddress *e_msg_composer_hdrs_get_reply_to (EMsgComposerHdrs *hdrs);

EDestination **e_msg_composer_hdrs_get_to          (EMsgComposerHdrs *hdrs);
EDestination **e_msg_composer_hdrs_get_cc          (EMsgComposerHdrs *hdrs);
EDestination **e_msg_composer_hdrs_get_bcc         (EMsgComposerHdrs *hdrs);
EDestination **e_msg_composer_hdrs_get_recipients  (EMsgComposerHdrs *hdrs);
const char    *e_msg_composer_hdrs_get_subject     (EMsgComposerHdrs *hdrs);

/* list of gchar* uris; this data is to be freed by the caller */
GList         *e_msg_composer_hdrs_get_post_to     (EMsgComposerHdrs *hdrs);

GtkWidget  *e_msg_composer_hdrs_get_from_hbox      (EMsgComposerHdrs *hdrs);
GtkWidget  *e_msg_composer_hdrs_get_reply_to_entry (EMsgComposerHdrs *hdrs);
GtkWidget  *e_msg_composer_hdrs_get_to_entry       (EMsgComposerHdrs *hdrs);
GtkWidget  *e_msg_composer_hdrs_get_cc_entry       (EMsgComposerHdrs *hdrs);
GtkWidget  *e_msg_composer_hdrs_get_bcc_entry      (EMsgComposerHdrs *hdrs);
GtkWidget  *e_msg_composer_hdrs_get_post_to_label  (EMsgComposerHdrs *hdrs);
GtkWidget  *e_msg_composer_hdrs_get_subject_entry  (EMsgComposerHdrs *hdrs);

void        e_msg_composer_hdrs_set_visible_mask   (EMsgComposerHdrs *hdrs,
						    int visible_mask);
void        e_msg_composer_hdrs_set_visible        (EMsgComposerHdrs *hdrs,
						    int visible_flags);

G_END_DECLS

#endif /* __E_MSG_COMPOSER_HDRS_H__ */

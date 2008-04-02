/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008 Novell, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU Lesser General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_COMPOSER_HEADER_TABLE_H
#define E_COMPOSER_HEADER_TABLE_H

#include "e-composer-common.h"

#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>
#include <libebook/e-destination.h>
#include <e-util/e-signature.h>
#include <e-util/e-signature-list.h>

#include "e-composer-header.h"

/* Standard GObject macros */
#define E_TYPE_COMPOSER_HEADER_TABLE \
	(e_composer_header_table_get_type ())
#define E_COMPOSER_HEADER_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_HEADER_TABLE, EComposerHeaderTable))
#define E_COMPOSER_HEADER_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_HEADER_TABLE, EComposerHeaderTableClass))
#define E_IS_COMPOSER_HEADER_TABLE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_HEADER_TABLE))
#define E_IS_COMPOSER_HEADER_TABLE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_HEADER_TABLE))
#define E_COMPOSER_HEADER_TABLE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_HEADER_TABLE, EComposerHeaderTableClass))

G_BEGIN_DECLS

typedef struct _EComposerHeaderTable EComposerHeaderTable;
typedef struct _EComposerHeaderTableClass EComposerHeaderTableClass;
typedef struct _EComposerHeaderTablePrivate EComposerHeaderTablePrivate;

/* Headers, listed in the order they should appear in the table. */
typedef enum {
	E_COMPOSER_HEADER_FROM,
	E_COMPOSER_HEADER_REPLY_TO,
	E_COMPOSER_HEADER_TO,
	E_COMPOSER_HEADER_CC,
	E_COMPOSER_HEADER_BCC,
	E_COMPOSER_HEADER_POST_TO,
	E_COMPOSER_HEADER_SUBJECT,
	E_COMPOSER_NUM_HEADERS
} EComposerHeaderType;

struct _EComposerHeaderTable {
	GtkTable parent;
	EComposerHeaderTablePrivate *priv;
};

struct _EComposerHeaderTableClass {
	GtkTableClass parent_class;
};

GType		e_composer_header_table_get_type (void);
GtkWidget *	e_composer_header_table_new	(void);
EComposerHeader * e_composer_header_table_get_header
						(EComposerHeaderTable *table,
						 EComposerHeaderType type);
gboolean	e_composer_header_table_get_header_sensitive
						(EComposerHeaderTable *table,
						 EComposerHeaderType type);
void		e_composer_header_table_set_header_sensitive
						(EComposerHeaderTable *table,
						 EComposerHeaderType type,
						 gboolean sensitive);
gboolean	e_composer_header_table_get_header_visible
						(EComposerHeaderTable *table,
						 EComposerHeaderType type);
void		e_composer_header_table_set_header_visible
						(EComposerHeaderTable *table,
						 EComposerHeaderType type,
						 gboolean visible);
EAccount *	e_composer_header_table_get_account
						(EComposerHeaderTable *table);
gboolean	e_composer_header_table_set_account
						(EComposerHeaderTable *table,
						 EAccount *account);
EAccountList *	e_composer_header_table_get_account_list
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_account_list
						(EComposerHeaderTable *table,
						 EAccountList *account_list);
const gchar *	e_composer_header_table_get_account_name
						(EComposerHeaderTable *table);
gboolean	e_composer_header_table_set_account_name
						(EComposerHeaderTable *table,
						 const gchar *account_name);
EDestination ** e_composer_header_table_get_destinations
						(EComposerHeaderTable *table);
EDestination **	e_composer_header_table_get_destinations_bcc
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_destinations_bcc
						(EComposerHeaderTable *table,
						 EDestination **destinations);
EDestination **	e_composer_header_table_get_destinations_cc
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_destinations_cc
						(EComposerHeaderTable *table,
						 EDestination **destinations);
EDestination **	e_composer_header_table_get_destinations_to
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_destinations_to
						(EComposerHeaderTable *table,
						 EDestination **destinations);
GList *		e_composer_header_table_get_post_to
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_post_to_base
						(EComposerHeaderTable *table,
						 const gchar *base_url,
						 const gchar *post_to);
void		e_composer_header_table_set_post_to_list
						(EComposerHeaderTable *table,
						 GList *folder_list);
const gchar *	e_composer_header_table_get_reply_to
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_reply_to
						(EComposerHeaderTable *table,
						 const gchar *reply_to);
ESignature *	e_composer_header_table_get_signature
						(EComposerHeaderTable *table);
gboolean	e_composer_header_table_set_signature
						(EComposerHeaderTable *table,
						 ESignature *signature);
ESignatureList *e_composer_header_table_get_signature_list
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_signature_list
						(EComposerHeaderTable *table,
						 ESignatureList *signature_list);
const gchar *	e_composer_header_table_get_subject
						(EComposerHeaderTable *table);
void		e_composer_header_table_set_subject
						(EComposerHeaderTable *table,
						 const gchar *subject);

G_END_DECLS

#endif /* E_COMPOSER_HEADER_TABLE_H */

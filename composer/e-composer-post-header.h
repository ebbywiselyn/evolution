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

#ifndef E_COMPOSER_POST_HEADER_H
#define E_COMPOSER_POST_HEADER_H

#include <libedataserver/e-account.h>

#include "e-composer-text-header.h"

/* Standard GObject macros */
#define E_TYPE_COMPOSER_POST_HEADER \
	(e_composer_post_header_get_type ())
#define E_COMPOSER_POST_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_POST_HEADER, EComposerPostHeader))
#define E_COMPOSER_POST_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_COMPOSER_POST_HEADER, EComposerPostHeaderClass))
#define E_IS_COMPOSER_POST_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_POST_HEADER))
#define E_IS_COMPOSER_POST_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_COMPOSER_POST_HEADER))
#define E_COMPOSER_POST_HEADER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_POST_HEADER, EComposerPostHeaderClass))

G_BEGIN_DECLS

typedef struct _EComposerPostHeader EComposerPostHeader;
typedef struct _EComposerPostHeaderClass EComposerPostHeaderClass;
typedef struct _EComposerPostHeaderPrivate EComposerPostHeaderPrivate;

struct _EComposerPostHeader {
	EComposerTextHeader parent;
	EComposerPostHeaderPrivate *priv;
};

struct _EComposerPostHeaderClass {
	EComposerTextHeaderClass parent_class;
};

GType		e_composer_post_header_get_type	(void);
EComposerHeader * e_composer_post_header_new	(const gchar *label);
EAccount *	e_composer_post_header_get_account
						(EComposerPostHeader *header);
void		e_composer_post_header_set_account
						(EComposerPostHeader *header,
						 EAccount *account);
GList *		e_composer_post_header_get_folders
						(EComposerPostHeader *header);
void		e_composer_post_header_set_folders
						(EComposerPostHeader *header,
						 GList *folders);
void		e_composer_post_header_set_folders_base
						(EComposerPostHeader *header,
						 const gchar *base_url,
						 const gchar *folders);

G_END_DECLS

#endif /* E_COMPOSER_POST_HEADER_H */

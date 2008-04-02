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

#ifndef E_COMPOSER_FROM_HEADER_H
#define E_COMPOSER_FROM_HEADER_H

#include "e-composer-common.h"

#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

#include "e-account-combo-box.h"
#include "e-composer-header.h"

/* Standard GObject macros */
#define E_TYPE_COMPOSER_FROM_HEADER \
	(e_composer_from_header_get_type ())
#define E_COMPOSER_FROM_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_COMPOSER_FROM_HEADER, EComposerFromHeader))
#define E_COMPOSER_FROM_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((obj), E_TYPE_COMPOSER_FROM_HEADER, EComposerFromHeaderClass))
#define E_IS_COMPOSER_FROM_HEADER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_COMPOSER_FROM_HEADER))
#define E_IS_COMPOSER_FROM_HEADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_COMPOSER_FROM_HEADER))
#define E_COMPOSER_FROM_HEADER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_COMPOSER_FROM_HEADER, EComposerFromHeaderClass))

G_BEGIN_DECLS

typedef struct _EComposerFromHeader EComposerFromHeader;
typedef struct _EComposerFromHeaderClass EComposerFromHeaderClass;

struct _EComposerFromHeader {
	EComposerHeader parent;
};

struct _EComposerFromHeaderClass {
	EComposerHeaderClass parent_class;
};

GType		e_composer_from_header_get_type	(void);
EComposerHeader * e_composer_from_header_new	(const gchar *label);
EAccountList *	e_composer_from_header_get_account_list
						(EComposerFromHeader *header);
void		e_composer_from_header_set_account_list
						(EComposerFromHeader *header,
						 EAccountList *account_list);
EAccount *	e_composer_from_header_get_active
						(EComposerFromHeader *header);
gboolean	e_composer_from_header_set_active
						(EComposerFromHeader *header,
						 EAccount *account);
const gchar *	e_composer_from_header_get_active_name
						(EComposerFromHeader *header);
gboolean	e_composer_from_header_set_active_name
						(EComposerFromHeader *header,
						 const gchar *account_name);

G_END_DECLS

#endif /* E_COMPOSER_FROM_HEADER_H */

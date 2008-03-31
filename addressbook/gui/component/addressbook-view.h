/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* addressbook-view.h
 *
 * Copyright (C) 2004  Novell, Inc.
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
 * Author: Chris Toshok <toshok@ximian.com>
 */

#ifndef _ADDRESSBOOK_VIEW_H_
#define _ADDRESSBOOK_VIEW_H_

#include <bonobo/bonobo-control.h>

#define ADDRESSBOOK_TYPE_VIEW			(addressbook_view_get_type ())
#define ADDRESSBOOK_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), ADDRESSBOOK_TYPE_VIEW, AddressbookView))
#define ADDRESSBOOK_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), ADDRESSBOOK_TYPE_VIEW, AddressbookViewClass))
#define ADDRESSBOOK_IS_VIEW(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ADDRESSBOOK_TYPE_VIEW))
#define ADDRESSBOOK_IS_VIEW_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), ADDRESSBOOK_TYPE_VIEW))


typedef struct _AddressbookView        AddressbookView;
typedef struct _AddressbookViewPrivate AddressbookViewPrivate;
typedef struct _AddressbookViewClass   AddressbookViewClass;

struct _AddressbookView {
	GObject parent;

	AddressbookViewPrivate *priv;
};

struct _AddressbookViewClass {
	GObjectClass parent_class;
};


GType addressbook_view_get_type (void);

AddressbookView   *addressbook_view_new                   (void);

EActivityHandler  *addressbook_view_peek_activity_handler (AddressbookView *view);
GtkWidget         *addressbook_view_peek_info_label       (AddressbookView *view);
GtkWidget         *addressbook_view_peek_sidebar          (AddressbookView *view);
GtkWidget         *addressbook_view_peek_statusbar        (AddressbookView *view);
BonoboControl     *addressbook_view_peek_folder_view      (AddressbookView *view);

void              addressbook_view_edit_contact           (AddressbookView* view,
							   const char* source_id,
							   const char* contact_id);

#endif /* _ADDRESSBOOK_VIEW_H_ */

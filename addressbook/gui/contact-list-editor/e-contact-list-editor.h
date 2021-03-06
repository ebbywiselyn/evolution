/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-contact-list-editor.h
 * Copyright (C) 2001  Ximian, Inc.
 * Author: Chris Toshok <toshok@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __E_CONTACT_LIST_EDITOR_H__
#define __E_CONTACT_LIST_EDITOR_H__

#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-app-helper.h>
#include <glade/glade.h>
#include <libedataserverui/e-name-selector.h>

#include "addressbook/gui/contact-editor/eab-editor.h"

#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include <libebook/e-destination.h>

#define E_TYPE_CONTACT_LIST_EDITOR \
	(e_contact_list_editor_get_type ())
#define E_CONTACT_LIST_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_LIST_EDITOR, EContactListEditor))
#define E_CONTACT_LIST_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACT_LIST_EDITOR, EContactListEditorClass))
#define E_IS_CONTACT_LIST_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_LIST_EDITOR))
#define E_IS_CONTACT_LIST_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((obj), E_TYPE_CONTACT_LIST_EDITOR))
#define E_CONTACT_LIST_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACT_LIST_EDITOR, EContactListEditorClass))

G_BEGIN_DECLS

typedef struct _EContactListEditor EContactListEditor;
typedef struct _EContactListEditorClass EContactListEditorClass;
typedef struct _EContactListEditorPrivate EContactListEditorPrivate;

struct _EContactListEditor
{
	EABEditor parent;
	EContactListEditorPrivate *priv;
};

struct _EContactListEditorClass
{
	EABEditorClass parent_class;
};

GType		e_contact_list_editor_get_type	(void);
EContactListEditor * e_contact_list_editor_new	(EBook *book,
						 EContact *list_contact,
						 gboolean is_new_list,
						 gboolean editable);
EBook *		e_contact_list_editor_get_book	(EContactListEditor *editor);
void		e_contact_list_editor_set_book	(EContactListEditor *editor,
						 EBook *book);
EContact *	e_contact_list_editor_get_contact
						(EContactListEditor *editor);
void		e_contact_list_editor_set_contact
						(EContactListEditor *editor,
						 EContact *contact);
gboolean	e_contact_list_editor_get_is_new_list
						(EContactListEditor *editor);
void		e_contact_list_editor_set_is_new_list
						(EContactListEditor *editor,
						 gboolean is_new_list);
gboolean	e_contact_list_editor_get_editable
						(EContactListEditor *editor);
void		e_contact_list_editor_set_editable
						(EContactListEditor *editor,
						 gboolean editable);
gboolean	e_contact_list_editor_request_close_all (void);

G_END_DECLS

#endif /* __E_CONTACT_LIST_EDITOR_H__ */

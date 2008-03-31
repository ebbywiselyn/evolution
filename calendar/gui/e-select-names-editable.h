/*
 * e-select-names-editable.h
 *
 * Author: Mike Kestner  <mkestner@ximian.com>
 *
 * Copyright (C) 2003 Ximian Inc.
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
 */

#ifndef __E_SELECT_NAMES_EDITABLE_H__
#define __E_SELECT_NAMES_EDITABLE_H__

#include <libedataserverui/e-name-selector-entry.h>

G_BEGIN_DECLS

#define E_TYPE_SELECT_NAMES_EDITABLE	     (e_select_names_editable_get_type ())
#define E_SELECT_NAMES_EDITABLE(o)	     (G_TYPE_CHECK_INSTANCE_CAST ((o), E_TYPE_SELECT_NAMES_EDITABLE, ESelectNamesEditable))
#define E_SELECT_NAMES_EDITABLE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), E_TYPE_SELECT_NAMES_EDITABLE, ESelectNamesEditableClass))
#define E_IS_SELECT_NAMES_EDITABLE(o)	     (G_TYPE_CHECK_INSTANCE_TYPE ((o), E_TYPE_SELECT_NAMES_EDITABLE))
#define E_IS_SELECT_NAMES_EDITABLE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((o), E_TYPE_SELECT_NAMES_EDITABLE))
#define E_SELECT_NAMES_EDITABLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), E_TYPE_SELECT_NAMES_EDITABLE, ESelectNamesEditableClass))

typedef struct _ESelectNamesEditable      ESelectNamesEditable;
typedef struct _ESelectNamesEditableClass ESelectNamesEditableClass;
typedef struct _ESelectNamesEditablePriv  ESelectNamesEditablePriv;

struct _ESelectNamesEditable
{
	ENameSelectorEntry parent;

	ESelectNamesEditablePriv *priv;
};

struct _ESelectNamesEditableClass
{
	ENameSelectorEntryClass parent_class;
};

GType      e_select_names_editable_get_type (void);

ESelectNamesEditable *e_select_names_editable_new (void);

gchar *e_select_names_editable_get_email (ESelectNamesEditable *esne);
GList *e_select_names_editable_get_emails (ESelectNamesEditable *esne);

gchar *e_select_names_editable_get_name (ESelectNamesEditable *esne);
GList *e_select_names_editable_get_names (ESelectNamesEditable *esne);

void   e_select_names_editable_set_address (ESelectNamesEditable *esne, const gchar *name, const gchar *email);

G_END_DECLS

#endif /* __E_SELECT_NAMES_EDITABLE_H__ */

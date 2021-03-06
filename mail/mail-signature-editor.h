/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors:
 *    Radek Doulik <rodo@ximian.com>
 *
 *  Copyright 2001, 2002 Ximian, Inc. (www.ximian.com)
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
 */

#ifndef MAIL_SIGNATURE_EDITOR_H
#define MAIL_SIGNATURE_EDITOR_H

#include <gtkhtml-editor.h>
#include <e-util/e-signature.h>

/* Standard GObject macros */
#define E_TYPE_SIGNATURE_EDITOR \
	(e_signature_editor_get_type ())
#define E_SIGNATURE_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SIGNATURE_EDITOR, ESignatureEditor))
#define E_SIGNATURE_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SIGNATURE_EDITOR, ESignatureEditorClass))
#define E_IS_SIGNATURE_EDITOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SIGNATURE_EDITOR))
#define E_IS_SIGNATURE_EDITOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SIGNATURE_EDITOR))
#define E_SIGNATURE_EDITOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SIGNATURE_EDITOR, ESignatureEditorClass))

G_BEGIN_DECLS

typedef struct _ESignatureEditor ESignatureEditor;
typedef struct _ESignatureEditorClass ESignatureEditorClass;
typedef struct _ESignatureEditorPrivate ESignatureEditorPrivate;

struct _ESignatureEditor {
	GtkhtmlEditor parent;
	ESignatureEditorPrivate *priv;
};

struct _ESignatureEditorClass {
	GtkhtmlEditorClass parent_class;
};

GType		e_signature_editor_get_type	 (void);
GtkWidget *	e_signature_editor_new		 (void);
ESignature *	e_signature_editor_get_signature (ESignatureEditor *editor);
void		e_signature_editor_set_signature (ESignatureEditor *editor,
						  ESignature *signature);

G_END_DECLS

#endif /* MAIL_SIGNATURE_EDITOR_H */

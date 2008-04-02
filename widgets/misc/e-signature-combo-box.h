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

#ifndef E_SIGNATURE_COMBO_BOX_H
#define E_SIGNATURE_COMBO_BOX_H

#include <gtk/gtk.h>
#include <e-util/e-signature.h>
#include <e-util/e-signature-list.h>

/* Standard GObject macros */
#define E_TYPE_SIGNATURE_COMBO_BOX \
	(e_signature_combo_box_get_type ())
#define E_SIGNATURE_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SIGNATURE_COMBO_BOX, ESignatureComboBox))
#define E_SIGNATURE_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SIGNATURE_COMBO_BOX, ESignatureComboBoxClass))
#define E_IS_SIGNATURE_COMBO_BOX(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SIGNATURE_COMBO_BOX))
#define E_IS_SIGNATURE_COMBO_BOX_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SIGNATURE_COMBO_BOX))
#define E_SIGNATURE_COMBO_BOX_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SIGNATURE_COMBO_BOX, ESignatureComboBoxClass))

G_BEGIN_DECLS

typedef struct _ESignatureComboBox ESignatureComboBox;
typedef struct _ESignatureComboBoxClass ESignatureComboBoxClass;
typedef struct _ESignatureComboBoxPrivate ESignatureComboBoxPrivate;

struct _ESignatureComboBox {
	GtkComboBox parent;
	ESignatureComboBoxPrivate *priv;
};

struct _ESignatureComboBoxClass {
	GtkComboBoxClass parent_class;
};

GType		e_signature_combo_box_get_type	(void);
GtkWidget *	e_signature_combo_box_new	(void);
ESignatureList *e_signature_combo_box_get_signature_list
					(ESignatureComboBox *combo_box);
void		e_signature_combo_box_set_signature_list
					(ESignatureComboBox *combo_box,
					 ESignatureList *signature_list);
ESignature *	e_signature_combo_box_get_active
					(ESignatureComboBox *combo_box);
gboolean	e_signature_combo_box_set_active
					(ESignatureComboBox *combo_box,
					 ESignature *signature);

G_END_DECLS

#endif /* E_SIGNATURE_COMBO_BOX_H */

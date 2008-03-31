/*
 * e-cell-number.c - Number item for e-table.
 * Copyright 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <config.h>

#include <sys/time.h>
#include <unistd.h>

#include <glib/gi18n.h>
#include "e-util/e-util.h"

#include "e-cell-number.h"

G_DEFINE_TYPE (ECellNumber, e_cell_number, E_CELL_TEXT_TYPE)

static char *
ecn_get_text(ECellText *cell, ETableModel *model, int col, int row)
{
	return e_format_number(GPOINTER_TO_INT (e_table_model_value_at(model, col, row)));
}

static void
ecn_free_text(ECellText *cell, char *text)
{
	g_free(text);
}

static void
e_cell_number_class_init (ECellNumberClass *klass)
{
	ECellTextClass *ectc = E_CELL_TEXT_CLASS (klass);

	ectc->get_text  = ecn_get_text;
	ectc->free_text = ecn_free_text;
}

static void
e_cell_number_init (ECellNumber *cell_number)
{
}

/**
 * e_cell_number_new:
 * @fontname: font to be used to render on the screen
 * @justify: Justification of the string in the cell.
 *
 * Creates a new ECell renderer that can be used to render numbers that
 * that come from the model.  The value returned from the model is
 * interpreted as being an int.
 *
 * See ECellText for other features.
 *
 * Returns: an ECell object that can be used to render numbers.
 */
ECell *
e_cell_number_new (const char *fontname, GtkJustification justify)
{
	ECellNumber *ecn = g_object_new (E_CELL_NUMBER_TYPE, NULL);

	e_cell_text_construct(E_CELL_TEXT(ecn), fontname, justify);

	return (ECell *) ecn;
}


/*
 *
 *
 * Copyright (C) 2004 Sivaiah Nallagatla
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <e-util/e-config.h>
#include <addressbook/gui/widgets/eab-config.h>
#include <libedataserver/e-source.h>
#include <string.h>

struct _GtkWidget *e_book_file_dummy (EPlugin *epl, EConfigHookItemFactoryData *data);

struct _GtkWidget *
e_book_file_dummy (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EABConfigTargetSource *t = (EABConfigTargetSource *) data->target;
	ESource *source = t->source;
	char *uri_text;
	const char *relative_uri;

        uri_text = e_source_get_uri (source);
	if (strncmp (uri_text, "file", 4)) {
		g_free (uri_text);

		return NULL;
	}

	relative_uri = e_source_peek_relative_uri (source);
	g_free (uri_text);


	if (relative_uri && *relative_uri) {
		return NULL;
	}

	e_source_set_relative_uri (source, e_source_peek_uid (source));

	return NULL;
}

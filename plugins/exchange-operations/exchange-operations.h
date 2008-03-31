/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Praveen Kumar <kpraveen@novell.com>
 * Copyright (C) 2005 Novell, Inc.
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

#ifndef __EXCHANGE_OPERATIONS_H__
#define __EXCHANGE_OPERATIONS_H__

#include <gtk/gtk.h>

#include "e-util/e-plugin.h"
#include "exchange-config-listener.h"
#include <exchange-account.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define ERROR_DOMAIN "org-gnome-exchange-operations"

extern ExchangeConfigListener *exchange_global_config_listener;

int e_plugin_lib_enable (EPluginLib *eplib, int enable);

ExchangeAccount *exchange_operations_get_exchange_account (void);
ExchangeConfigListenerStatus exchange_is_offline (gint *mode);

gboolean exchange_operations_tokenize_string (char **string, char *token, char delimit, unsigned int maxsize);

gboolean exchange_operations_cta_add_node_to_tree (GtkTreeStore *store, GtkTreeIter *parent, const char *nuri);
void exchange_operations_cta_select_node_from_tree (GtkTreeStore *store, GtkTreeIter *parent, const char *nuri, const char *ruri, GtkTreeSelection *selection) ;

void exchange_operations_report_error (ExchangeAccount *account, ExchangeAccountResult result);

void exchange_operations_update_child_esources (ESource *source, const gchar *old_path, const gchar *new_path);

gboolean is_exchange_personal_folder (ExchangeAccount *account, char *uri);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif

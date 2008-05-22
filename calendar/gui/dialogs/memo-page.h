/* Evolution calendar - Main page of the memo editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef MEMO_PAGE_H
#define MEMO_PAGE_H

#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-widget.h>
#include "comp-editor-page.h"

G_BEGIN_DECLS

#define TYPE_MEMO_PAGE            (memo_page_get_type ())
#define MEMO_PAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_MEMO_PAGE, MemoPage))
#define MEMO_PAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_MEMO_PAGE, MemoPageClass))
#define IS_MEMO_PAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_MEMO_PAGE))
#define IS_MEMO_PAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), TYPE_MEMO_PAGE))

typedef struct _MemoPagePrivate MemoPagePrivate;

typedef struct {
	CompEditorPage page;

	/* Private data */
	MemoPagePrivate *priv;
} MemoPage;

typedef struct {
	CompEditorPageClass parent_class;
} MemoPageClass;

GType     memo_page_get_type  (void);
MemoPage *memo_page_construct (MemoPage *epage);
MemoPage *memo_page_new       (BonoboUIComponent *uic, CompEditorPageFlags flags);
void      memo_page_set_classification (MemoPage *page, ECalComponentClassification class);
void      memo_page_set_show_categories (MemoPage *page, gboolean state);
void 	  memo_page_set_info_string (MemoPage *mpage, const gchar *icon, const gchar *msg);
G_END_DECLS

#endif

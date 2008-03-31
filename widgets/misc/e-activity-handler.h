/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-activity-handler.h
 *
 * Copyright (C) 2001, 2002, 2003 Novell, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _E_ACTIVITY_HANDLER_H_
#define _E_ACTIVITY_HANDLER_H_

#include "e-task-bar.h"
#include "e-util/e-logger.h"
#include <glib-object.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_ACTIVITY_HANDLER			(e_activity_handler_get_type ())
#define E_ACTIVITY_HANDLER(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_ACTIVITY_HANDLER, EActivityHandler))
#define E_ACTIVITY_HANDLER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_ACTIVITY_HANDLER, EActivityHandlerClass))
#define E_IS_ACTIVITY_HANDLER(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_ACTIVITY_HANDLER))
#define E_IS_ACTIVITY_HANDLER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_ACTIVITY_HANDLER))


typedef struct _EActivityHandler        EActivityHandler;
typedef struct _EActivityHandlerPrivate EActivityHandlerPrivate;
typedef struct _EActivityHandlerClass   EActivityHandlerClass;

#define EAH_ICON_INFO  "stock_dialog-info"
#define EAH_ICON_ERROR "stock_dialog-warning"

struct _EActivityHandler {
	GObject parent;

	EActivityHandlerPrivate *priv;
};

struct _EActivityHandlerClass {
	GObjectClass parent_class;
};


GtkType  e_activity_handler_get_type  (void);

EActivityHandler *e_activity_handler_new  (void);

void  e_activity_handler_attach_task_bar  (EActivityHandler *activity_hanlder,
					   ETaskBar         *task_bar);

void  e_activity_handler_set_message   (EActivityHandler *activity_handler,
				        const char       *message);

void  e_activity_handler_unset_message (EActivityHandler *activity_handler);

guint  e_activity_handler_operation_started  (EActivityHandler *activity_handler,
					      const char       *component_id,
					      GdkPixbuf        *icon_pixbuf,
					      const char       *information,
					      gboolean          cancellable);
guint  e_activity_handler_cancelable_operation_started  (EActivityHandler *activity_handler,
						      const char       *component_id,
						      GdkPixbuf        *icon_pixbuf,
					      	      const char       *information,
					      	      gboolean          cancellable,
						      void (*cancel_func)(gpointer),
						      gpointer user_data);

void  e_activity_handler_operation_progressing  (EActivityHandler *activity_handler,
						 guint             activity_id,
						 const char       *information,
						 double            progress);

void  e_activity_handler_operation_finished  (EActivityHandler *activity_handler,
					      guint             activity_id);

void e_activity_handler_set_logger (EActivityHandler *handler, ELogger *logger);
guint e_activity_handler_make_error (EActivityHandler *activity_handler,
				      const char *component_id,
				      int error_type,
				      GtkWidget  *error);
void
e_activity_handler_operation_set_error (EActivityHandler *activity_handler,
                                          guint activity_id,
                                          GtkWidget *error);

void
e_activity_handler_set_error_flush_time (EActivityHandler *handler, int time);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_ACTIVITY_HANDLER_H_ */

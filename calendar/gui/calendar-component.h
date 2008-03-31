/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* calendar-component.h
 *
 * Copyright (C) 2003  Novell, Inc.
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

#ifndef _CALENDAR_COMPONENT_H_
#define _CALENDAR_COMPONENT_H_


#include <bonobo/bonobo-object.h>
#include <libedataserver/e-source-list.h>
#include "Evolution.h"


#define CALENDAR_TYPE_COMPONENT			(calendar_component_get_type ())
#define CALENDAR_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), CALENDAR_TYPE_COMPONENT, CalendarComponent))
#define CALENDAR_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), CALENDAR_TYPE_COMPONENT, CalendarComponentClass))
#define CALENDAR_IS_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), CALENDAR_TYPE_COMPONENT))
#define CALENDAR_IS_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), CALENDAR_TYPE_COMPONENT))


typedef struct _CalendarComponent        CalendarComponent;
typedef struct _CalendarComponentPrivate CalendarComponentPrivate;
typedef struct _CalendarComponentClass   CalendarComponentClass;

struct _CalendarComponent {
	BonoboObject parent;

	CalendarComponentPrivate *priv;
};

struct _CalendarComponentClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Component__epv epv;
};


GType calendar_component_get_type  (void);

CalendarComponent *calendar_component_peek  (void);

const char       *calendar_component_peek_base_directory    (CalendarComponent *component);
const char       *calendar_component_peek_config_directory  (CalendarComponent *component);
ESourceList      *calendar_component_peek_source_list       (CalendarComponent *component);


#endif /* _CALENDAR_COMPONENT_H_ */

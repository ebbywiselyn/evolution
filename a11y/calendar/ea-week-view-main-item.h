/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-week-view-main-item.h
 *
 * Copyright (C) 2003 Ximian, Inc.
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
 * Author: Bolian Yin <bolian.yin@sun.com> Sun Microsystem Inc., 2003
 * Author: Yang Wu <Yang.Wu@sun.com> Sun Microsystem Inc., 2003
 *
 */

#ifndef __EA_WEEK_VIEW_MAIN_ITEM_H__
#define __EA_WEEK_VIEW_MAIN_ITEM_H__

#include <atk/atkgobjectaccessible.h>
#include "e-week-view-main-item.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define EA_TYPE_WEEK_VIEW_MAIN_ITEM                     (ea_week_view_main_item_get_type ())
#define EA_WEEK_VIEW_MAIN_ITEM(obj)                     (G_TYPE_CHECK_INSTANCE_CAST ((obj), EA_TYPE_WEEK_VIEW_MAIN_ITEM, EaWeekViewMainItem))
#define EA_WEEK_VIEW_MAIN_ITEM_CLASS(klass)             (G_TYPE_CHECK_CLASS_CAST ((klass), EA_TYPE_WEEK_VIEW_MAIN_ITEM, EaWeekViewMainItemClass))
#define EA_IS_WEEK_VIEW_MAIN_ITEM(obj)                  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EA_TYPE_WEEK_VIEW_MAIN_ITEM))
#define EA_IS_WEEK_VIEW_MAIN_ITEM_CLASS(klass)          (G_TYPE_CHECK_CLASS_TYPE ((klass), EA_TYPE_WEEK_VIEW_MAIN_ITEM))
#define EA_WEEK_VIEW_MAIN_ITEM_GET_CLASS(obj)           (G_TYPE_INSTANCE_GET_CLASS ((obj), EA_TYPE_WEEK_VIEW_MAIN_ITEM, EaWeekViewMainItemClass))

typedef struct _EaWeekViewMainItem                   EaWeekViewMainItem;
typedef struct _EaWeekViewMainItemClass              EaWeekViewMainItemClass;

struct _EaWeekViewMainItem
{
	AtkGObjectAccessible parent;
};

GType ea_week_view_main_item_get_type (void);

struct _EaWeekViewMainItemClass
{
	AtkGObjectAccessibleClass parent_class;
};

AtkObject*     ea_week_view_main_item_new         (GObject *obj);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EA_WEEK_VIEW_MAIN_ITEM_H__ */

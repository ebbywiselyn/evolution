/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* vim:expandtab:shiftwidth=8:tabstop=8:
 */
/* Evolution Accessibility: ea-widgets.c
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
 *
 */

#include "ea-factory.h"
#include "widgets/ea-calendar-item.h"
#include "widgets/ea-combo-button.h"
#include "widgets/ea-expander.h"
#include "ea-widgets.h"

EA_FACTORY_GOBJECT (EA_TYPE_CALENDAR_ITEM, ea_calendar_item, ea_calendar_item_new)
EA_FACTORY (EA_TYPE_COMBO_BUTTON, ea_combo_button, ea_combo_button_new)
EA_FACTORY (EA_TYPE_EXPANDER, ea_expander, ea_expander_new)

void e_calendar_item_a11y_init (void)
{
    EA_SET_FACTORY (e_calendar_item_get_type (), ea_calendar_item);
}

void e_combo_button_a11y_init (void)
{
    EA_SET_FACTORY (e_combo_button_get_type (), ea_combo_button);
}

void e_expander_a11y_init (void)
{
     EA_SET_FACTORY (e_expander_get_type (), ea_expander);
}

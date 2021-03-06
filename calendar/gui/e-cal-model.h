/* Evolution calendar - Data model for ETable
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
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

#ifndef E_CAL_MODEL_H
#define E_CAL_MODEL_H

#include <table/e-table-model.h>
#include <libecal/e-cal.h>
#include "e-cell-date-edit-text.h"

G_BEGIN_DECLS

#define E_TYPE_CAL_MODEL            (e_cal_model_get_type ())
#define E_CAL_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_CAL_MODEL, ECalModel))
#define E_CAL_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_CAL_MODEL, ECalModelClass))
#define E_IS_CAL_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_CAL_MODEL))
#define E_IS_CAL_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_CAL_MODEL))

typedef struct _ECalModelPrivate ECalModelPrivate;

typedef enum {
	/* If you add new items here or reorder them, you have to update the
	   .etspec files for the tables using this model */
	E_CAL_MODEL_FIELD_CATEGORIES,
	E_CAL_MODEL_FIELD_CLASSIFICATION,
	E_CAL_MODEL_FIELD_COLOR,            /* not a real field */
	E_CAL_MODEL_FIELD_COMPONENT,        /* not a real field */
	E_CAL_MODEL_FIELD_DESCRIPTION,
	E_CAL_MODEL_FIELD_DTSTART,
	E_CAL_MODEL_FIELD_HAS_ALARMS,       /* not a real field */
	E_CAL_MODEL_FIELD_ICON,             /* not a real field */
	E_CAL_MODEL_FIELD_SUMMARY,
	E_CAL_MODEL_FIELD_UID,
	E_CAL_MODEL_FIELD_LAST
} ECalModelField;

typedef enum {
	E_CAL_MODEL_FLAGS_INVALID            = -1,
	E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES = 0x01
} ECalModelFlags;

typedef struct {
	ECal *client;
	icalcomponent *icalcomp;
	time_t instance_start;
	time_t instance_end;

	/* private data */
	ECellDateEditValue *dtstart;
	ECellDateEditValue *dtend;
	ECellDateEditValue *due;
	ECellDateEditValue *completed;
	gchar *color;
} ECalModelComponent;

typedef struct {
	ECalModelComponent *comp_data;
	gpointer cb_data;
} ECalModelGenerateInstancesData;

typedef struct _ECalModel {
	ETableModel model;
	ECalModelPrivate *priv;
} ECalModel;

typedef struct {
	ETableModelClass parent_class;

	/* virtual methods */
	const gchar * (* get_color_for_component) (ECalModel *model, ECalModelComponent *comp_data);
	void          (* fill_component_from_model) (ECalModel *model, ECalModelComponent *comp_data,
						     ETableModel *source_model, gint row);

	/* Signals */
	void (* time_range_changed) (ECalModel *model, time_t start, time_t end);
	void (* row_appended) (ECalModel *model);
	void (* cal_view_progress) (ECalModel *model, const char *message, int progress, ECalSourceType type);
	void (* cal_view_done) (ECalModel *model, ECalendarStatus status, ECalSourceType type);
} ECalModelClass;

GType               e_cal_model_get_type                       (void);
icalcomponent_kind  e_cal_model_get_component_kind             (ECalModel           *model);
void                e_cal_model_set_component_kind             (ECalModel           *model,
								icalcomponent_kind   kind);
ECalModelFlags      e_cal_model_get_flags                      (ECalModel           *model);
void                e_cal_model_set_flags                      (ECalModel           *model,
								ECalModelFlags       flags);
icaltimezone       *e_cal_model_get_timezone                   (ECalModel           *model);
void                e_cal_model_set_timezone                   (ECalModel           *model,
								icaltimezone        *zone);
void                e_cal_model_set_default_category           (ECalModel           *model,
								const gchar         *default_cat);
gboolean            e_cal_model_get_use_24_hour_format         (ECalModel           *model);
void                e_cal_model_set_use_24_hour_format         (ECalModel           *model,
								gboolean             use24);
ECal               *e_cal_model_get_default_client             (ECalModel           *model);
void                e_cal_model_set_default_client             (ECalModel           *model,
								ECal                *client);
GList              *e_cal_model_get_client_list                (ECalModel           *model);
ECal               *e_cal_model_get_client_for_uri             (ECalModel           *model,
								const char          *uri);
void                e_cal_model_add_client                     (ECalModel           *model,
								ECal                *client);
void                e_cal_model_remove_client                  (ECalModel           *model,
								ECal                *client);
void                e_cal_model_remove_all_clients             (ECalModel           *model);
void                e_cal_model_get_time_range                 (ECalModel           *model,
								time_t              *start,
								time_t              *end);
void                e_cal_model_set_time_range                 (ECalModel           *model,
								time_t               start,
								time_t               end);
const char         *e_cal_model_get_search_query               (ECalModel           *model);
void                e_cal_model_set_search_query               (ECalModel           *model,
								const gchar         *sexp);
icalcomponent      *e_cal_model_create_component_with_defaults (ECalModel           *model);
const gchar        *e_cal_model_get_color_for_component        (ECalModel           *model,
								ECalModelComponent  *comp_data);
gboolean            e_cal_model_get_rgb_color_for_component    (ECalModel           *model,
								ECalModelComponent  *comp_data,
								double              *red,
								double              *green,
								double              *blue);
ECalModelComponent *e_cal_model_get_component_at               (ECalModel           *model,
								gint                 row);
ECalModelComponent *e_cal_model_get_component_for_uid          (ECalModel           *model,
								const ECalComponentId *id);
gchar              *e_cal_model_date_value_to_string           (ECalModel           *model,
								const void          *value);
ECalModelComponent *e_cal_model_copy_component_data            (ECalModelComponent  *comp_data);
void                e_cal_model_free_component_data            (ECalModelComponent  *comp_data);
void                e_cal_model_generate_instances             (ECalModel           *model,
								time_t               start,
								time_t               end,
								ECalRecurInstanceFn  cb,
								gpointer             cb_data);
GPtrArray * e_cal_model_get_object_array (ECalModel *model);
void e_cal_model_set_instance_times (ECalModelComponent *comp_data, const icaltimezone *zone);
void e_cal_model_set_search_query_with_time_range (ECalModel *model, const char *sexp, time_t start, time_t end);

gboolean e_cal_model_test_row_editable (ECalModel *model, int row);

G_END_DECLS

#endif

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2003 Ximian, Inc. (www.ximian.com)
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
 */

#ifndef EM_ICON_STREAM_H
#define EM_ICON_STREAM_H

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EM_ICON_STREAM_TYPE     (em_icon_stream_get_type ())
#define EM_ICON_STREAM(obj)     (CAMEL_CHECK_CAST((obj), EM_ICON_STREAM_TYPE, EMIconStream))
#define EM_ICON_STREAM_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), EM_ICON_STREAM_TYPE, EMIconStreamClass))
#define EM_IS_ICON_STREAM(o)    (CAMEL_CHECK_TYPE((o), EM_ICON_STREAM_TYPE))

struct _GtkHTML;
struct _GtkIconStream;

#include "mail/em-sync-stream.h"

typedef struct _EMIconStream {
	EMSyncStream sync;

	unsigned int width, height;
	guint destroy_id;
	struct _GdkPixbufLoader *loader;
	struct _GtkImage *image;
	char *key;

	guint keep:1;
} EMIconStream;

typedef struct {
	EMSyncStreamClass parent_class;
} EMIconStreamClass;

CamelType    em_icon_stream_get_type (void);
CamelStream *em_icon_stream_new(GtkImage *image, const char *key, unsigned int maxwidth, unsigned int maxheight, int keep);

struct _GdkPixbuf *em_icon_stream_get_image(const char *key, unsigned int maxwidth, unsigned int maxheight);
int em_icon_stream_is_resized(const char *key, unsigned int maxwidth, unsigned int maxheight);

void em_icon_stream_clear_cache(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EM_ICON_STREAM_H */

/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2004 Ximian, Inc. (www.ximian.com)
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
 *
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* This isn't as portable as, say, the stuff in GNU coreutils.  But I care not for OSF1. */
#ifdef HAVE_STATVFS
# ifdef HAVE_SYS_STATVFS_H
#  include <sys/statvfs.h>
# endif
#else
#ifdef HAVE_STATFS
# ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>	/* bsd interface */
# endif
# ifdef HAVE_SYS_MOUNT_H
#  include <sys/mount.h>
# endif
#endif
#endif

#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>

#include "e-fsutils.h"

/**
 * e_fsutils_usage:
 * @path:
 *
 * Calculate the amount of disk space used by a given path.
 *
 * Return value: The number of 1024 byte blocks used by the
 * filesystem.
 **/
long e_fsutils_usage(const char *inpath)
{
	GDir *dir;
	const char *d;
	long size = 0;
	GSList *paths;

	/* iterative, depth-first scan, because i can ... */
	paths = g_slist_prepend(NULL, g_strdup(inpath));

	while (paths) {
		char *path = paths->data;

		paths = g_slist_remove_link(paths, paths);

		dir = g_dir_open(path, 0, NULL);
		if (dir == NULL) {
			g_free(path);
			goto fail;
		}

		while ((d = g_dir_read_name(dir))) {
			char *full_path;
			struct stat st;

			full_path = g_build_filename(path, d, NULL);
			if (g_stat(full_path, &st) == -1) {
				g_free(full_path);
				g_dir_close(dir);
				g_free(path);
				goto fail;
			} else if (S_ISDIR(st.st_mode)) {
				paths = g_slist_prepend(paths, full_path);
				full_path = NULL;
			} else if (S_ISREG(st.st_mode)) {
				/* This is in 512 byte blocks.  st_blksize is page size on linux,
				   on *BSD it might be significant. */
#ifndef G_OS_WIN32
				size += st.st_blocks/2;
#endif
			}

			g_free(full_path);
		}

		g_dir_close(dir);
		g_free(path);
	}

	return size;

fail:
	g_slist_foreach(paths, (GFunc)g_free, NULL);
	g_slist_free(paths);

	return -1;
}

/**
 * e_fsutils_avail:
 * @path:
 *
 * Find the available disk space at the given path.
 *
 * Return value: -1 if it could not be determined, otherwise the
 * number of disk blocks, expressed as system-independent, 1024 byte
 * blocks.
 **/
long
e_fsutils_avail(const char *path)
{
#if defined(HAVE_STATVFS)
	struct statvfs stfs;

	if (statvfs(path, &stfs) == -1)
		return -1;

	/* Assumes that frsize === power of 2 */
	if (stfs.f_frsize >= 1024)
		return stfs.f_bavail * (stfs.f_frsize / 1024);
	else
		return stfs.f_bavail / (1024 / stfs.f_frsize);
#elif defined(HAVE_STATFS)
	struct statfs stfs;

	if (statfs(path, &stfs) == -1)
		return -1;

	/* For BSD this isn't clear, it may be dependent on f_bsize */
	return stfs.f_bavail / 2;
#else
	errno = ENOSYS;
	return -1;
#endif
}


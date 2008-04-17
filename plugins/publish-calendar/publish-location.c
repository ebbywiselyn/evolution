/*
 * Authors:
 *  David Trowbridge <trowbrds@cs.colorado.edu>
 *  Gary Ekker <gekker@novell.com>
 *
 * Copyright (C) 2005 Novell, Inc (www.novell.com)
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

#include "publish-location.h"
#include <libxml/tree.h>
#include <gconf/gconf-client.h>
#include <libedataserver/e-url.h>
#include <libedataserverui/e-passwords.h>
#include <string.h>

static EPublishUri *
migrateURI (const gchar *xml, xmlDocPtr doc)
{
	GConfClient *client;
	GSList *uris, *l, *events = NULL;
	xmlChar *location, *enabled, *frequency, *username;
	xmlNodePtr root, p;
	EPublishUri *uri;
	gchar *password, *temp;
	EUri *euri;

	client = gconf_client_get_default ();
	uris = gconf_client_get_list (client, "/apps/evolution/calendar/publish/uris", GCONF_VALUE_STRING, NULL);
	l = uris;
	while (l && l->data) {
		gchar *str = l->data;
		if (strcmp (xml, str) == 0) {
			uris = g_slist_remove (uris, str);
			g_free (str);
		}
		l = g_slist_next (l);
	}

	uri = g_new0 (EPublishUri, 1);

	root = doc->children;
	location = xmlGetProp (root, (const unsigned char *)"location");
	enabled = xmlGetProp (root, (const unsigned char *)"enabled");
	frequency = xmlGetProp (root, (const unsigned char *)"frequency");
	username = xmlGetProp (root, (const unsigned char *)"username");

	euri = e_uri_new ((const char *)location);

	if (!euri) {
		g_warning ("Could not form the uri for %s \n", location);
		goto cleanup;
	}

	if (euri->user)
		g_free (euri->user);

	euri->user = g_strdup ((const char *)username);

	temp = e_uri_to_string (euri, FALSE);
	uri->location = g_strdup_printf ("dav://%s", strstr (temp, "//") + 2);
	g_free (temp);
	e_uri_free (euri);

	if (enabled != NULL)
		uri->enabled = atoi ((char *)enabled);
	if (frequency != NULL)
		uri->publish_frequency = atoi ((char *)frequency);
	uri->publish_format = URI_PUBLISH_AS_FB;

	password = e_passwords_get_password ("Calendar", (char *)location);
	if (password) {
		e_passwords_forget_password ("Calendar", (char *)location);
		e_passwords_add_password (uri->location, password);
		e_passwords_remember_password ("Calendar", uri->location);
	}

	for (p = root->children; p != NULL; p = p->next) {
		xmlChar *uid = xmlGetProp (p, (const unsigned char *)"uid");
		if (strcmp ((char *)p->name, "source") == 0) {
			events = g_slist_append (events, uid);
		} else {
			g_free (uid);
		}
	}
	uri->events = events;

	uris = g_slist_prepend (uris, e_publish_uri_to_xml (uri));
	gconf_client_set_list (client, "/apps/evolution/calendar/publish/uris", GCONF_VALUE_STRING, uris, NULL);
	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);
	g_object_unref (client);

cleanup:
	xmlFree (location);
	xmlFree (enabled);
	xmlFree (frequency);
	xmlFree (username);
	xmlFreeDoc (doc);

	return uri;
}

EPublishUri *
e_publish_uri_from_xml (const gchar *xml)
{
	xmlDocPtr doc;
	xmlNodePtr root, p;
	xmlChar *location, *enabled, *frequency;
	xmlChar *publish_time, *format, *username = NULL;
	GSList *events = NULL;
	EPublishUri *uri;

	doc = xmlParseDoc ((const unsigned char *)xml);
	if (doc == NULL)
		return NULL;

	root = doc->children;
	if (strcmp ((char *)root->name, "uri") != 0)
		return NULL;

	if ((username = xmlGetProp (root, (const unsigned char *)"username"))) {
		xmlFree (username);
		return migrateURI (xml, doc);

	}

	uri = g_new0 (EPublishUri, 1);

	location = xmlGetProp (root, (const unsigned char *)"location");
	enabled = xmlGetProp (root, (const unsigned char *)"enabled");
	frequency = xmlGetProp (root, (const unsigned char *)"frequency");
	format = xmlGetProp (root, (const unsigned char *)"format");
	publish_time = xmlGetProp (root, (const unsigned char *)"publish_time");

	if (location != NULL)
		uri->location = (char *)location;
	if (enabled != NULL)
		uri->enabled = atoi ((char *)enabled);
	if (frequency != NULL)
		uri->publish_frequency = atoi ((char *)frequency);
	if (format != NULL)
		uri->publish_format = atoi ((char *)format);
	if (publish_time != NULL)
		uri->last_pub_time = (char *)publish_time;

	uri->password = g_strdup ("");

	for (p = root->children; p != NULL; p = p->next) {
		xmlChar *uid = xmlGetProp (p, (const unsigned char *)"uid");
		if (strcmp ((char *)p->name, "event") == 0) {
			events = g_slist_append (events, uid);
		} else {
			g_free (uid);
		}
	}
	uri->events = events;

	xmlFree (enabled);
	xmlFree (frequency);
	xmlFree (format);
	xmlFreeDoc (doc);

	return uri;
}

gchar *
e_publish_uri_to_xml (EPublishUri *uri)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	gchar *enabled, *frequency, *format;
	GSList *calendars = NULL;
	xmlChar *xml_buffer;
	char *returned_buffer;
	int xml_buffer_size;

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (uri->location != NULL, NULL);

	doc = xmlNewDoc ((const unsigned char *)"1.0");

	root = xmlNewDocNode (doc, NULL, (const unsigned char *)"uri", NULL);
	enabled = g_strdup_printf ("%d", uri->enabled);
	frequency = g_strdup_printf ("%d", uri->publish_frequency);
	format = g_strdup_printf ("%d", uri->publish_format);
	xmlSetProp (root, (const unsigned char *)"location", (unsigned char *)uri->location);
	xmlSetProp (root, (const unsigned char *)"enabled", (unsigned char *)enabled);
	xmlSetProp (root, (const unsigned char *)"frequency", (unsigned char *)frequency);
	xmlSetProp (root, (const unsigned char *)"format", (unsigned char *)format);
	xmlSetProp (root, (const unsigned char *)"publish_time", (unsigned char *)uri->last_pub_time);

	for (calendars = uri->events; calendars != NULL; calendars = g_slist_next (calendars)) {
		xmlNodePtr node;
		node = xmlNewChild (root, NULL, (const unsigned char *)"event", NULL);
		xmlSetProp (node, (const unsigned char *)"uid", calendars->data);
	}
	xmlDocSetRootElement (doc, root);

	xmlDocDumpMemory (doc, &xml_buffer, &xml_buffer_size);
	xmlFreeDoc (doc);

	returned_buffer = g_malloc (xml_buffer_size + 1);
	memcpy (returned_buffer, xml_buffer, xml_buffer_size);
	returned_buffer[xml_buffer_size] = '\0';
	xmlFree (xml_buffer);
	g_free (enabled);
	g_free (frequency);
	g_free (format);

	return returned_buffer;
}

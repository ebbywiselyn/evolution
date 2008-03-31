/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* addressbook-component.c
 *
 * Copyright (C) 2003  Ettore Perazzoli
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

/* EPFIXME: Add autocompletion setting.  */


#include <config.h>

#include "addressbook-component.h"
#include "addressbook-config.h"
#include "addressbook-migrate.h"
#include "addressbook-view.h"
#include "addressbook/gui/contact-editor/eab-editor.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "e-util/e-plugin.h"
#include "e-util/e-import.h"
#include "addressbook/gui/widgets/eab-popup.h"
#include "addressbook/gui/widgets/eab-menu.h"
#include "addressbook/gui/widgets/eab-config.h"
#include "addressbook/importers/evolution-addressbook-importers.h"

#include "misc/e-task-bar.h"
#include "misc/e-info-label.h"

#include "shell/e-component-view.h"

#include <string.h>
#include <bonobo/bonobo-i18n.h>
#include <gtk/gtkimage.h>
#include <gconf/gconf-client.h>
#include <e-util/e-util.h>
#include <libedataserver/e-url.h>

#ifdef ENABLE_SMIME
#include "smime/gui/component.h"
#endif

#define LDAP_BASE_URI "ldap://"
#define PERSONAL_RELATIVE_URI "system"

#define PARENT_TYPE bonobo_object_get_type ()
static BonoboObjectClass *parent_class = NULL;

struct _AddressbookComponentPrivate {
	GConfClient *gconf_client;
	char *base_directory;
	GList *views;
};

static void
ensure_sources (AddressbookComponent *component)
{
	GSList *groups;
	ESourceList *source_list;
	ESourceGroup *group;
	ESourceGroup *on_this_computer;
	ESourceGroup *on_ldap_servers;
	ESource *personal_source;
	char *base_uri, *base_uri_proto;
	const gchar *base_dir;

	on_this_computer = NULL;
	on_ldap_servers = NULL;
	personal_source = NULL;

	if (!e_book_get_addressbooks (&source_list, NULL)) {
		g_warning ("Could not get addressbook source list from GConf!");
		return;
	}

	base_dir = addressbook_component_peek_base_directory (component);
	base_uri = g_build_filename (base_dir, "local", NULL);

	base_uri_proto = g_filename_to_uri (base_uri, NULL, NULL);

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;

		for (g = groups; g; g = g->next) {

			group = E_SOURCE_GROUP (g->data);

			/* compare only file:// part. If user home dir name changes we do not want to create
			   one more group  */

			if (!on_this_computer && !strncmp (base_uri_proto, e_source_group_peek_base_uri (group), 7))
				on_this_computer = group;
			else if (!on_ldap_servers && !strcmp (LDAP_BASE_URI, e_source_group_peek_base_uri (group)))
				on_ldap_servers = group;
		}
	}

	if (on_this_computer) {
		/* make sure "Personal" shows up as a source under
		   this group */
		GSList *sources = e_source_group_peek_sources (on_this_computer);
		GSList *s;
		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;
			if (!strcmp (PERSONAL_RELATIVE_URI, relative_uri)) {
				personal_source = source;
				break;
			}
		}
		/* Make sure we have the correct base uri. This can change when user's
		   homedir name changes */
		if (strcmp (base_uri_proto, e_source_group_peek_base_uri (on_this_computer))) {
		    e_source_group_set_base_uri (on_this_computer, base_uri_proto);

		    /* *sigh* . We shouldn't  need this sync call here as set_base_uri
		       call results in synching to gconf, but that happens in idle loop
		       and too late to prevent user seeing "Can not Open ... because of invalid uri" error.*/
		    e_source_list_sync (source_list,NULL);
		}
	}
	else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), base_uri_proto);
		e_source_list_add_group (source_list, group, -1);

		on_this_computer = group;
	}

	if (!personal_source) {
		/* Create the default Person addressbook */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (on_this_computer, source, -1);

		e_source_set_property (source, "completion", "true");

		personal_source = source;
	}

	if (!on_ldap_servers) {
		/* Create the LDAP source group */
		group = e_source_group_new (_("On LDAP Servers"), LDAP_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		on_ldap_servers = group;
	}
	if (personal_source)
		g_object_unref (personal_source);
	g_free (base_uri_proto);
	g_free (base_uri);
}

static void
view_destroyed_cb (gpointer data, GObject *where_the_object_was)
{
	AddressbookComponent *addressbook_component = data;
	AddressbookComponentPrivate *priv;
	GList *l;

	priv = addressbook_component->priv;

	for (l = priv->views; l; l = l->next) {
		AddressbookView *view = l->data;
		if (G_OBJECT (view) == where_the_object_was) {
			priv->views = g_list_remove (priv->views, view);
			break;
		}
	}
}

/* Evolution::Component CORBA methods.  */

static GNOME_Evolution_ComponentView
impl_createView (PortableServer_Servant servant,
		 GNOME_Evolution_ShellView parent,
		 CORBA_Environment *ev)
{
	AddressbookComponent *addressbook_component = ADDRESSBOOK_COMPONENT (bonobo_object_from_servant (servant));
	AddressbookComponentPrivate *priv = addressbook_component->priv;
	AddressbookView *view = addressbook_view_new ();
	EComponentView *component_view;

	g_object_weak_ref (G_OBJECT (view), view_destroyed_cb, addressbook_component);
	priv->views = g_list_append (priv->views, view);

	component_view = e_component_view_new_controls (parent, "contacts",
							bonobo_control_new (addressbook_view_peek_sidebar (view)),
							addressbook_view_peek_folder_view (view),
							bonobo_control_new (addressbook_view_peek_statusbar (view)));

	return BONOBO_OBJREF(component_view);

}

static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 3;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = "contact";
	list->_buffer[0].description = _("New Contact");
	list->_buffer[0].menuDescription = _("_Contact");
	list->_buffer[0].tooltip = _("Create a new contact");
	list->_buffer[0].menuShortcut = 'c';
	list->_buffer[0].iconName = "contact-new";
	list->_buffer[0].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[1].id = "contact_list";
	list->_buffer[1].description = _("New Contact List");
	list->_buffer[1].menuDescription = _("Contact _List");
	list->_buffer[1].tooltip = _("Create a new contact list");
	list->_buffer[1].menuShortcut = 'l';
	list->_buffer[1].iconName = "stock_contact-list";
	list->_buffer[1].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[2].id = "address_book";
	list->_buffer[2].description = _("New Address Book");
	list->_buffer[2].menuDescription = _("Address _Book");
	list->_buffer[2].tooltip = _("Create a new address book");
	list->_buffer[2].menuShortcut = '\0';
	list->_buffer[2].iconName = "address-book-new";
	list->_buffer[2].type = GNOME_Evolution_CREATABLE_FOLDER;

	return list;
}

static void
book_loaded_cb (EBook *book, EBookStatus status, gpointer data)
{
	EContact *contact;
	char *item_type_name = data;

	if (status != E_BOOK_ERROR_OK) {
		/* XXX we really need a dialog here, but we don't have
		   access to the ESource so we can't use
		   eab_load_error_dialog.  fun! */
		return;
	}

	contact = e_contact_new ();

	if (!strcmp (item_type_name, "contact")) {
		eab_show_contact_editor (book, contact, TRUE, TRUE);
	}
	else if (!strcmp (item_type_name, "contact_list")) {
		eab_show_contact_list_editor (book, contact, TRUE, TRUE);
	}

	g_object_unref (book);
	g_object_unref (contact);

	g_free (item_type_name);
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	EBook *book;
	GConfClient *gconf_client;
	ESourceList *source_list;
	char *uid;

	if (!item_type_name ||
	    (strcmp (item_type_name, "address_book") &&
	     strcmp (item_type_name, "contact") &&
	     strcmp (item_type_name, "contact_list"))) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UnknownType, NULL);
		return;
	}

	if (!strcmp (item_type_name, "address_book")) {
		addressbook_config_create_new_source (NULL);
		return;
	}

	gconf_client = gconf_client_get_default();
	uid = gconf_client_get_string (gconf_client, "/apps/evolution/addressbook/display/primary_addressbook",
			NULL);
	g_object_unref (gconf_client);
	if (!e_book_get_addressbooks (&source_list, NULL)) {
		g_warning ("Could not get addressbook source list from GConf!");
		g_free (uid);
		return;
	}
	if (uid) {
		ESource *source = e_source_list_peek_source_by_uid(source_list, uid);
		if (source) {
			book = e_book_new (source, NULL);
		}
		else {
			book = e_book_new_default_addressbook (NULL);
		}
		g_free (uid);
	}
	else {
		book = e_book_new_default_addressbook (NULL);
	}

	e_book_async_open (book, FALSE, book_loaded_cb, g_strdup (item_type_name));
}

static void
impl_handleURI (PortableServer_Servant servant,
		const char* uri,
		CORBA_Environment *ev)
{
	AddressbookComponent *addressbook_component = ADDRESSBOOK_COMPONENT (bonobo_object_from_servant (servant));
	AddressbookComponentPrivate *priv;
	AddressbookView *view = NULL;

	GList *l;
	char *src_uid = NULL;
	char *contact_uid = NULL;

	priv = addressbook_component->priv;
	l = g_list_last (priv->views);
	if (!l)
		return;

	view = l->data;

	if (!strncmp (uri, "contacts:", 9)) {
		EUri *euri = e_uri_new (uri);
		const char *p;
		char *header, *content;
		size_t len, clen;

		p = euri->query;
		if (p) {
			while (*p) {
				len = strcspn (p, "=&");

				/* If it's malformed, give up. */
				if (p[len] != '=')
					break;

				header = (char *) p;
				header[len] = '\0';
				p += len + 1;

				clen = strcspn (p, "&");

				content = g_strndup (p, clen);

				if (!g_ascii_strcasecmp (header, "source-uid")) {
					src_uid = g_strdup (content);
				} else if (!g_ascii_strcasecmp (header, "contact-uid")) {
					contact_uid = g_strdup (content);
				}

				g_free (content);

				p += clen;
				if (*p == '&') {
					p++;
					if (!strcmp (p, "amp;"))
						p += 4;
				}
			}

			addressbook_view_edit_contact (view, src_uid, contact_uid);

			g_free (src_uid);
			g_free (contact_uid);
		}
		e_uri_free (euri);
	}

}

static void
impl_upgradeFromVersion (PortableServer_Servant servant, short major, short minor, short revision, CORBA_Environment *ev)
{
	GError *err = NULL;

	if (!addressbook_migrate (addressbook_component_peek (), major, minor, revision, &err)) {
		GNOME_Evolution_Component_UpgradeFailed *failedex;

		failedex = GNOME_Evolution_Component_UpgradeFailed__alloc();
		failedex->what = CORBA_string_dup(_("Failed upgrading Address Book settings or folders."));
		failedex->why = CORBA_string_dup(err->message);
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UpgradeFailed, failedex);
	}

	if (err)
		g_error_free(err);
}

static CORBA_boolean
impl_requestQuit (PortableServer_Servant servant, CORBA_Environment *ev)
{
	return eab_editor_request_close_all ();
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	AddressbookComponentPrivate *priv = ADDRESSBOOK_COMPONENT (object)->priv;
	GList *l;

	if (priv->gconf_client != NULL) {
		g_object_unref (priv->gconf_client);
		priv->gconf_client = NULL;
	}

	for (l = priv->views; l; l = l->next) {
		AddressbookView *view = l->data;
		g_object_weak_unref (G_OBJECT (view), view_destroyed_cb, ADDRESSBOOK_COMPONENT (object));
	}
	g_list_free (priv->views);
	priv->views = NULL;
	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	AddressbookComponentPrivate *priv = ADDRESSBOOK_COMPONENT (object)->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
addressbook_component_class_init (AddressbookComponentClass *class)
{
	POA_GNOME_Evolution_Component__epv *epv = &class->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	epv->createView              = impl_createView;
	epv->_get_userCreatableItems = impl__get_userCreatableItems;
	epv->requestCreateItem       = impl_requestCreateItem;
	epv->upgradeFromVersion      = impl_upgradeFromVersion;
	epv->requestQuit             = impl_requestQuit;
	epv->handleURI               = impl_handleURI;

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	parent_class = g_type_class_peek_parent (class);
}

static void
addressbook_component_init (AddressbookComponent *component)
{
	AddressbookComponentPrivate *priv;
	static int first = TRUE;

	priv = g_new0 (AddressbookComponentPrivate, 1);

	/* EPFIXME: Should use a custom one instead? */
	priv->gconf_client = gconf_client_get_default ();

	priv->base_directory = g_build_filename (e_get_user_data_dir (), "addressbook", NULL);

	component->priv = priv;

	ensure_sources (component);

#ifdef ENABLE_SMIME
	smime_component_init ();
#endif

	if (first) {
		EImportClass *klass;

		first = FALSE;
		e_plugin_hook_register_type(eab_popup_hook_get_type());
		e_plugin_hook_register_type(eab_menu_hook_get_type());
		e_plugin_hook_register_type(eab_config_hook_get_type());

		klass = g_type_class_ref(e_import_get_type());
		e_import_class_add_importer(klass, evolution_ldif_importer_peek(), NULL, NULL);
		e_import_class_add_importer(klass, evolution_vcard_importer_peek(), NULL, NULL);
		e_import_class_add_importer(klass, evolution_csv_outlook_importer_peek(), NULL, NULL);
		e_import_class_add_importer(klass, evolution_csv_mozilla_importer_peek(), NULL, NULL);
		e_import_class_add_importer(klass, evolution_csv_evolution_importer_peek(), NULL, NULL);
	}
}


/* Public API.  */

AddressbookComponent *
addressbook_component_peek (void)
{
	static AddressbookComponent *component = NULL;

	if (component == NULL)
		component = g_object_new (addressbook_component_get_type (), NULL);

	return component;
}

GConfClient*
addressbook_component_peek_gconf_client (AddressbookComponent *component)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_COMPONENT (component), NULL);

	return component->priv->gconf_client;
}

const char *
addressbook_component_peek_base_directory (AddressbookComponent *component)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_COMPONENT (component), NULL);

	return component->priv->base_directory;
}

BONOBO_TYPE_FUNC_FULL (AddressbookComponent, GNOME_Evolution_Component, PARENT_TYPE, addressbook_component)

/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-component.c
 *
 * Copyright (C) 2003  Ximian Inc.
 *
 * Authors: Ettore Perazzoli <ettore@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *	    Jeffrey Stedfast <fejj@ximian.com>
 *
 * This  program is free  software; you  can redistribute  it and/or
 * modify it under the terms of version 2  of the GNU General Public
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libedataserver/e-data-server-util.h>
#include "em-utils.h"
#include "em-composer-utils.h"
#include "em-format.h"
#include "em-folder-tree.h"
#include "em-folder-browser.h"
#include "em-message-browser.h"
#include "em-folder-selector.h"
#include "em-folder-selection.h"
#include "em-folder-utils.h"
#include "em-migrate.h"

#include "misc/e-info-label.h"
#include "e-util/e-util.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"
#include "e-util/e-logger.h"
#include "e-util/gconf-bridge.h"

#include "em-search-context.h"
#include "mail-config.h"
#include "mail-component.h"
#include "mail-folder-cache.h"
#include "mail-vfolder.h"
#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-send-recv.h"
#include "mail-session.h"
#include "message-list.h"

#include "e-activity-handler.h"
#include "shell/e-user-creatable-items-handler.h"
#include "shell/e-component-view.h"

#include "composer/e-msg-composer.h"

#include "e-task-bar.h"

#include <gtk/gtklabel.h>

#include <e-util/e-mktemp.h>
#include <Evolution.h>

#include <table/e-tree.h>
#include <table/e-tree-memory.h>
#include <glib/gi18n.h>

#include <camel/camel-file-utils.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-disco-store.h>
#include <camel/camel-offline-store.h>

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>

#define d(x)

static void create_local_item_cb(EUserCreatableItemsHandler *handler, const char *item_type_name, void *data);
static void view_changed_timeout_remove (EComponentView *component_view);

#define MAIL_COMPONENT_DEFAULT(mc) if (mc == NULL) mc = mail_component_peek();

#define PARENT_TYPE evolution_component_get_type ()
static BonoboObjectClass *parent_class = NULL;

#define OFFLINE 0
#define ONLINE 1

struct _store_info {
	CamelStore *store;
	char *name;

	/* we keep a reference to these so they remain around for the session */
	CamelFolder *vtrash;
	CamelFolder *vjunk;

	/* for setup only */
	void (*done)(CamelStore *store, CamelFolderInfo *info, void *data);
	void *done_data;

	int ref_count:31;
	guint removed:1;
};

struct _MailComponentPrivate {
	GMutex *lock;

	/* states/data used during shutdown */
	enum { MC_QUIT_START, MC_QUIT_SYNC, MC_QUIT_THREADS } quit_state;
	int quit_count;
	int quit_expunge;	/* expunge on quit this time around? */

	char *base_directory;

	EMFolderTreeModel *model;

	EActivityHandler *activity_handler;

	MailAsyncEvent *async_event;
	GHashTable *store_hash; /* stores store_info objects by store */

	RuleContext *search_context;

	char *context_path;	/* current path for right-click menu */

	CamelStore *local_store;
	ELogger *logger;

	EComponentView *component_view;
};

/* indexed by _mail_component_folder_t */
static struct {
	char *name;
	char *uri;
	CamelFolder *folder;
} mc_default_folders[] = {
	/* translators: standard local mailbox names */
	{ N_("Inbox"), },
	{ N_("Drafts"), },
	{ N_("Outbox"), },
	{ N_("Sent"), },
	{ "Inbox", },		/* 'always local' inbox */
};

static struct _store_info *
store_info_new(CamelStore *store, const char *name)
{
	struct _store_info *si;

	si = g_malloc0(sizeof(*si));
	si->ref_count = 1;
	if (name == NULL)
		si->name = camel_service_get_name((CamelService *)store, TRUE);
	else
		si->name = g_strdup(name);
	si->store = store;
	camel_object_ref(store);
	/* If these are vfolders then they need to be opened now,
	 * otherwise they wont keep track of all folders */
	if ((store->flags & CAMEL_STORE_VTRASH) != 0)
		si->vtrash = camel_store_get_trash(store, NULL);
	if ((store->flags & CAMEL_STORE_VJUNK) != 0)
		si->vjunk = camel_store_get_junk(store, NULL);

	return si;
}

static void
store_info_ref(struct _store_info *si)
{
	si->ref_count++;
}

static void
store_info_unref(struct _store_info *si)
{
	if (si->ref_count > 1) {
		si->ref_count--;
		return;
	}

	if (si->vtrash)
		camel_object_unref(si->vtrash);
	if (si->vjunk)
		camel_object_unref(si->vjunk);
	camel_object_unref(si->store);
	g_free(si->name);
	g_free(si);
}

static gboolean
mc_add_store_done(CamelStore *store, CamelFolderInfo *info, void *data)
{
	struct _store_info *si = data;

	if (si->done)
		si->done(store, info, si);

	if (!si->removed) {
		/* let the counters know about the already opened junk/trash folders */
		if (si->vtrash)
			mail_note_folder(si->vtrash);
		if (si->vjunk)
			mail_note_folder(si->vjunk);
	}

	store_info_unref(si);

	return TRUE;
}

/* Utility functions.  */
static void
mc_add_store(MailComponent *component, CamelStore *store, const char *name, void (*done)(CamelStore *store, CamelFolderInfo *info, void *data))
{
	struct _store_info *si;

	MAIL_COMPONENT_DEFAULT(component);

	si = store_info_new(store, name);
	si->done = done;
	g_hash_table_insert(component->priv->store_hash, store, si);
	em_folder_tree_model_add_store(component->priv->model, store, si->name);
	store_info_ref(si);
	mail_note_store(store, NULL, mc_add_store_done, si);
}

static void
mc_add_local_store_done(CamelStore *store, CamelFolderInfo *info, void *data)
{
	/*MailComponent *mc = data;*/
	int i;

	for (i=0;i<sizeof(mc_default_folders)/sizeof(mc_default_folders[0]);i++) {
		if (mc_default_folders[i].folder)
			mail_note_folder(mc_default_folders[i].folder);
	}
}

static void
mc_add_local_store(CamelStore *store, const char *name, MailComponent *mc)
{
	mc_add_store(mc, store, name, mc_add_local_store_done);
	camel_object_unref(store);
	g_object_unref(mc);
}

static void
mc_setup_local_store(MailComponent *mc)
{
	MailComponentPrivate *p = mc->priv;
	CamelURL *url;
	char *tmp;
	CamelException ex;
	int i;

	g_mutex_lock(p->lock);
	if (p->local_store != NULL) {
		g_mutex_unlock(p->lock);
		return;
	}

	camel_exception_init(&ex);

	url = camel_url_new("mbox:", NULL);
	tmp = g_build_filename (p->base_directory, "local", NULL);
	camel_url_set_path(url, tmp);
	g_free(tmp);
	tmp = camel_url_to_string(url, 0);
	p->local_store = (CamelStore *)camel_session_get_service(session, tmp, CAMEL_PROVIDER_STORE, &ex);
	g_free(tmp);
	if (p->local_store == NULL)
		goto fail;

	for (i=0;i<sizeof(mc_default_folders)/sizeof(mc_default_folders[0]);i++) {
		/* FIXME: should this uri be account relative? */
		camel_url_set_fragment(url, mc_default_folders[i].name);
		mc_default_folders[i].uri = camel_url_to_string(url, 0);
		mc_default_folders[i].folder = camel_store_get_folder(p->local_store, mc_default_folders[i].name,
								      CAMEL_STORE_FOLDER_CREATE, &ex);
		camel_exception_clear(&ex);
	}

	camel_url_free(url);
	g_mutex_unlock(p->lock);

	g_object_ref(mc);
	camel_object_ref(p->local_store);
	mail_async_event_emit(p->async_event, MAIL_ASYNC_GUI, (MailAsyncFunc)mc_add_local_store, p->local_store, _("On This Computer"), mc);

	return;
fail:
	g_mutex_unlock(p->lock);

	g_warning("Could not setup local store/folder: %s", ex.desc);

	camel_url_free(url);
	camel_exception_clear(&ex);
}

static void
load_accounts (MailComponent *component, EAccountList *accounts)
{
	EIterator *iter;

	/* Load each service (don't connect!). Check its provider and
	 * see if this belongs in the shell's folder list. If so, add
	 * it.
	 */

	iter = e_list_get_iterator ((EList *) accounts);
	while (e_iterator_is_valid (iter)) {
		EAccountService *service;
		EAccount *account;
		const char *name;

		account = (EAccount *) e_iterator_get (iter);
		service = account->source;
		name = account->name;

		/* HACK: mbox url's are handled by the local store setup above,
		   any that come through as account sources are really movemail sources! */
		if (account->enabled
		    && service->url != NULL
		    && service->url[0]
		    && strncmp(service->url, "mbox:", 5) != 0)
			mail_component_load_store_by_uri (component, service->url, name);

		e_iterator_next (iter);
	}

	g_object_unref (iter);
}

static void
setup_search_context (MailComponent *component)
{
	MailComponentPrivate *priv = component->priv;

	if (priv->search_context == NULL) {
		char *user = g_build_filename(component->priv->base_directory, "searches.xml", NULL);
		char *system = g_build_filename (EVOLUTION_PRIVDATADIR, "searchtypes.xml", NULL);

		priv->search_context = (RuleContext *)em_search_context_new ();
		g_object_set_data_full (G_OBJECT (priv->search_context), "user", user, g_free);
		g_object_set_data_full (G_OBJECT (priv->search_context), "system", system, g_free);
		rule_context_load (priv->search_context, system, user);
	}
}

static void
mc_startup(MailComponent *mc)
{
	static int started = 0;
	GConfClient *gconf;

	if (started)
		return;
	started = 1;

	mc_setup_local_store(mc);
	load_accounts(mc, mail_config_get_accounts());

	gconf = mail_config_get_gconf_client();

	if (gconf_client_get_bool (gconf, "/apps/evolution/mail/display/enable_vfolders", NULL))
		vfolder_load_storage();
}

static void
folder_selected_cb (EMFolderTree *emft, const char *path, const char *uri, guint32 flags, EMFolderView *view)
{
	EMFolderTreeModel *model;

	if ((flags & CAMEL_FOLDER_NOSELECT) || !path) {
		em_folder_view_set_folder (view, NULL, NULL);
	} else {
		model = em_folder_tree_get_model (emft);
		em_folder_tree_model_set_selected (model, uri);
		em_folder_tree_model_save_state (model);

		em_folder_view_set_folder_uri (view, uri);
	}
}

static int
check_autosave(void *data)
{
	e_msg_composer_check_autosave(NULL);

	return FALSE;
}

static void
view_control_activate_cb (BonoboControl *control, gboolean activate, EMFolderView *view)
{
	BonoboUIComponent *uic;
	static int recover = 0;

	uic = bonobo_control_get_ui_component (control);
	g_return_if_fail (uic != NULL);

	if (activate) {
		Bonobo_UIContainer container;

		container = bonobo_control_get_remote_ui_container (control, NULL);
		bonobo_ui_component_set_container (uic, container, NULL);
		bonobo_object_release_unref (container, NULL);

		g_return_if_fail (container == bonobo_ui_component_get_container(uic));
		g_return_if_fail (container != CORBA_OBJECT_NIL);

		em_folder_view_activate (view, uic, activate);
		e_user_creatable_items_handler_activate(g_object_get_data((GObject *)view, "e-creatable-items-handler"), uic);
	} else {
		em_folder_view_activate (view, uic, activate);
		bonobo_ui_component_unset_container (uic, NULL);
	}

	/* This is a weird place to put it, but createControls does it too early.
	   I also think we should wait to do it until we actually visit the mailer.
	   The delay is arbitrary - without it it shows up before the main window */
	if (!recover) {
		recover = 1;
		g_timeout_add(1000, check_autosave, NULL);
	}
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	MailComponentPrivate *priv = MAIL_COMPONENT (object)->priv;

	view_changed_timeout_remove ((EComponentView *)object);

	if (priv->activity_handler != NULL) {
		g_object_unref (priv->activity_handler);
		priv->activity_handler = NULL;
	}

	if (priv->search_context != NULL) {
		g_object_unref (priv->search_context);
		priv->search_context = NULL;
	}

	if (priv->local_store != NULL) {
		camel_object_unref (priv->local_store);
		priv->local_store = NULL;
	}

	priv->component_view = NULL;

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	MailComponentPrivate *priv = MAIL_COMPONENT (object)->priv;

	g_free (priv->base_directory);

	mail_async_event_destroy (priv->async_event);

	g_hash_table_destroy (priv->store_hash);

	if (mail_async_event_destroy (priv->async_event) == -1) {
		g_warning("Cannot destroy async event: would deadlock");
		g_warning(" system may be unstable at exit");
	}

	g_free (priv->context_path);
	g_mutex_free(priv->lock);
	g_object_unref (priv->model);
	g_object_unref (priv->logger);
	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
view_on_url (GObject *emitter, const char *url, const char *nice_url, MailComponent *mail_component)
{
	MailComponentPrivate *priv = mail_component->priv;

	e_activity_handler_set_message (priv->activity_handler, nice_url);
}

static void
view_changed(EMFolderView *emfv, EComponentView *component_view)
{
	EInfoLabel *el = g_object_get_data((GObject *)component_view, "info-label");
	CORBA_Environment ev;

	CORBA_exception_init(&ev);

	if (emfv->folder) {
		char *name, *title;
		const char *use_name; /* will contain localized name, if necessary */
		guint32 visible, unread, deleted, junked, junked_not_deleted;
		GPtrArray *selected;
		GString *tmp = g_string_new("");

		camel_object_get(emfv->folder, NULL,
				 CAMEL_FOLDER_NAME, &name,
				 CAMEL_FOLDER_DELETED, &deleted,
				 CAMEL_FOLDER_VISIBLE, &visible,
				 CAMEL_FOLDER_JUNKED, &junked,
				 CAMEL_FOLDER_JUNKED_NOT_DELETED, &junked_not_deleted,
				 CAMEL_FOLDER_UNREAD, &unread, NULL);

		selected = message_list_get_selected(emfv->list);

		/* This is so that if any of these are
		 * shared/reused, we fallback to the standard
		 * display behaviour */
		if (selected->len > 1)
			g_string_append_printf(tmp, ngettext ("%d selected, ", "%d selected, ", selected->len), selected->len);

		if (CAMEL_IS_VTRASH_FOLDER(emfv->folder)) {
			if (((CamelVTrashFolder *)emfv->folder)->type == CAMEL_VTRASH_FOLDER_TRASH) {
				g_string_append_printf(tmp, ngettext ("%d deleted", "%d deleted", deleted), deleted);
			} else {
				guint32 num = junked_not_deleted;

				if (!emfv->hide_deleted)
					num = junked;

				g_string_append_printf (tmp, ngettext ("%d junk", "%d junk", num), num);
			}
		} else if (em_utils_folder_is_drafts(emfv->folder, emfv->folder_uri)) {
			g_string_append_printf(tmp, ngettext ("%d draft", "%d drafts", visible), visible);
		} else if (em_utils_folder_is_sent(emfv->folder, emfv->folder_uri)) {
			g_string_append_printf(tmp, ngettext ("%d sent", "%d sent", visible), visible);
		} else if (em_utils_folder_is_outbox(emfv->folder, emfv->folder_uri)) {
			g_string_append_printf(tmp, ngettext ("%d unsent", "%d unsent", visible), visible);
			/* HACK: hardcoded inbox or maildir '.' folder */
		} else {
			if (!emfv->hide_deleted)
				visible += deleted - junked + junked_not_deleted;
			if (unread && selected->len <= 1)
				g_string_append_printf(tmp, ngettext ("%d unread, ", "%d unread, ", unread), unread);
			g_string_append_printf(tmp, ngettext ("%d total", "%d total", visible), visible);
		}

		message_list_free_uids(emfv->list, selected);

		if (emfv->folder->parent_store == mail_component_peek_local_store(NULL)
		    && (!strcmp (name, "Drafts") || !strcmp (name, "Inbox")
			|| !strcmp (name, "Outbox") || !strcmp (name, "Sent")))
			use_name = _(name);
		else
			use_name = name;

		e_info_label_set_info (el, use_name, tmp->str);
		title = g_strdup_printf ("%s (%s)", use_name, tmp->str);
		e_component_view_set_title(component_view, title);
		g_free(title);

		g_string_free(tmp, TRUE);
		camel_object_free(emfv->folder, CAMEL_FOLDER_NAME, name);
	} else {
		e_info_label_set_info(el, _("Mail"), "");
		e_component_view_set_title(component_view, _("Mail"));
	}
}

static void
view_changed_timeout_remove (EComponentView *component_view)
{
	gpointer v;
	EInfoLabel *el;
	EMFolderView *emfv;

	v = g_object_get_data((GObject *)component_view, "view-changed-timeout");
	if (v) {
		g_source_remove(GPOINTER_TO_INT(v));
		g_object_set_data((GObject *)component_view, "view-changed-timeout", NULL);

		el = g_object_get_data((GObject *)component_view, "info-label");
		emfv = g_object_get_data((GObject *)el, "folderview");
		g_object_unref(el);
		g_object_unref(emfv);
	}
}

static int
view_changed_timeout(void *d)
{
	EComponentView *component_view = d;
	EInfoLabel *el = g_object_get_data((GObject *)component_view, "info-label");
	EMFolderView *emfv = g_object_get_data((GObject *)el, "folderview");

	view_changed(emfv, component_view);

	g_object_set_data((GObject *)component_view, "view-changed-timeout", NULL);

	g_object_unref(el);
	g_object_unref(emfv);

	return 0;
}

static void
view_changed_cb(EMFolderView *emfv, EComponentView *component_view)
{
	MailComponent *mc = mail_component_peek ();
	void *v;
	EInfoLabel *el = g_object_get_data((GObject *)component_view, "info-label");

	v = g_object_get_data((GObject *)component_view, "view-changed-timeout");

	if (mc->priv->quit_state != -1) {
		if (v) {
			g_source_remove(GPOINTER_TO_INT(v));
			g_object_set_data((GObject *)component_view, "view-changed-timeout", NULL);
			g_object_unref (emfv);
			g_object_unref (el);
		}

		return;

	}
	/* This can get called 3 times every cursor move, so
	   we don't need to/want to run it immediately */

	/* NB: we should have a 'view' struct/object to manage this crap, but this'll do for now */
	if (v) {
		g_source_remove(GPOINTER_TO_INT(v));
	} else {
		g_object_ref(emfv);
		g_object_ref(el);
	}

	g_object_set_data((GObject *)component_view, "view-changed-timeout", GINT_TO_POINTER(g_timeout_add(250, view_changed_timeout, component_view)));
}

static void
disable_folder_tree (gpointer *emfb, GtkWidget *widget)
{
	gtk_widget_set_sensitive (widget, FALSE);
}

static void
enable_folder_tree (GtkWidget *emfb, GtkWidget *emft)
{
	EMFolderView *emfv = (EMFolderView *) emfb;
	CamelURL *selected_curl, *current_curl;
	CamelFolder *selected_folder;
	gchar *uri;

	/* Get the currently displayed folder. */
	uri = mail_tools_folder_to_url (emfv->list->folder);
	current_curl = uri ? camel_url_new (uri, NULL) : NULL;
	g_free (uri);

	/* Get the selected folder in the folder tree. */
	selected_folder = em_folder_tree_get_selected_folder(EM_FOLDER_TREE (emft));
	uri = mail_tools_folder_to_url (selected_folder);

	selected_curl = uri ? camel_url_new (uri, NULL) : NULL;

	if (current_curl && selected_curl && !camel_url_equal (selected_curl, current_curl)) {

		g_signal_emit_by_name (
			emft, "folder-selected", emft, uri,
			selected_folder->full_name, uri, selected_folder->folder_flags);
	}

	gtk_widget_set_sensitive (emft, TRUE);

	camel_url_free (current_curl);
	camel_url_free (selected_curl);
	g_free (uri);
}

/* Evolution::Component CORBA methods.  */

static GNOME_Evolution_ComponentView
impl_createView (PortableServer_Servant servant,
		 GNOME_Evolution_ShellView parent,
		 CORBA_Environment *ev)
{
	MailComponent *mail_component = MAIL_COMPONENT (bonobo_object_from_servant (servant));
	MailComponentPrivate *priv = mail_component->priv;
	EComponentView *component_view;
	GtkWidget *tree_widget, *vbox, *info;
	GtkWidget *view_widget;
	GtkWidget *statusbar_widget;
	char *uri;

	mail_session_set_interactive(TRUE);
	mc_startup(mail_component);

	view_widget = em_folder_browser_new ();

	tree_widget = (GtkWidget *) em_folder_tree_new_with_model (priv->model);
	em_folder_tree_set_excluded ((EMFolderTree *) tree_widget, 0);
	em_folder_tree_enable_drag_and_drop ((EMFolderTree *) tree_widget);

	if ((uri = em_folder_tree_model_get_selected (priv->model))) {
		gboolean expanded;

		expanded = em_folder_tree_model_get_expanded_uri (priv->model, uri);
		em_folder_tree_set_selected ((EMFolderTree *) tree_widget, uri, FALSE);
		em_folder_view_set_folder_uri ((EMFolderView *) view_widget, uri);

		if (!expanded)
			em_folder_tree_model_set_expanded_uri (priv->model, uri, expanded);

		g_free (uri);
	}

	em_format_set_session ((EMFormat *) ((EMFolderView *) view_widget)->preview, session);

	g_signal_connect (view_widget, "on-url", G_CALLBACK (view_on_url), mail_component);
	em_folder_view_set_statusbar ((EMFolderView*)view_widget, FALSE);

	statusbar_widget = e_task_bar_new ();
	e_activity_handler_attach_task_bar (priv->activity_handler, E_TASK_BAR (statusbar_widget));

	gtk_widget_show (tree_widget);
	gtk_widget_show (view_widget);
	gtk_widget_show (statusbar_widget);

	vbox = gtk_vbox_new(FALSE, 0);
	info = e_info_label_new("stock_mail");
	e_info_label_set_info((EInfoLabel *)info, _("Mail"), "");
	gtk_box_pack_start((GtkBox *)vbox, info, FALSE, TRUE, 0);
	gtk_box_pack_start((GtkBox *)vbox, tree_widget, TRUE, TRUE, 0);

	gtk_widget_show(info);
	gtk_widget_show(vbox);

	component_view = e_component_view_new(parent, "mail", vbox, view_widget, statusbar_widget);

	g_object_set_data((GObject *)component_view, "info-label", info);

	g_object_set_data_full((GObject *)view_widget, "e-creatable-items-handler",
			       e_user_creatable_items_handler_new("mail", create_local_item_cb, tree_widget),
			       (GDestroyNotify)g_object_unref);


	g_signal_connect (component_view->view_control, "activate", G_CALLBACK (view_control_activate_cb), view_widget);
	g_signal_connect (tree_widget, "folder-selected", G_CALLBACK (folder_selected_cb), view_widget);

	g_signal_connect((EMFolderBrowser *)view_widget, "account_search_cleared", G_CALLBACK (enable_folder_tree), tree_widget);
	g_signal_connect(((EMFolderBrowser *)view_widget), "account_search_activated", G_CALLBACK (disable_folder_tree), tree_widget);
	g_signal_connect(view_widget, "changed", G_CALLBACK(view_changed_cb), component_view);
	g_signal_connect(view_widget, "loaded", G_CALLBACK(view_changed_cb), component_view);

	g_object_set_data((GObject*)info, "folderview", view_widget);
	g_object_set_data((GObject*)view_widget, "foldertree", tree_widget);

	priv->component_view = component_view;

	return BONOBO_OBJREF(component_view);
}

static CORBA_boolean
impl_requestQuit(PortableServer_Servant servant, CORBA_Environment *ev)
{
	/*MailComponent *mc = MAIL_COMPONENT(bonobo_object_from_servant(servant));*/
	CamelFolder *folder;
	guint32 unsent;

	if (!e_msg_composer_request_close_all())
		return FALSE;

	folder = mc_default_folders[MAIL_COMPONENT_FOLDER_OUTBOX].folder;
	if (folder != NULL
	    && camel_session_is_online(session)
	    && camel_object_get(folder, NULL, CAMEL_FOLDER_VISIBLE, &unsent, 0) == 0
	    && unsent > 0
	    && e_error_run(NULL, "mail:exit-unsaved", NULL) != GTK_RESPONSE_YES)
		return FALSE;

	return TRUE;
}

static void
mc_quit_sync_done(CamelStore *store, void *data)
{
	MailComponent *mc = data;

	mc->priv->quit_count--;
}

static void
mc_quit_sync(CamelStore *store, struct _store_info *si, MailComponent *mc)
{
	mc->priv->quit_count++;
	mail_sync_store(store, mc->priv->quit_expunge, mc_quit_sync_done, mc);
}

static void
mc_quit_delete (CamelStore *store, struct _store_info *si, MailComponent *mc)
{
	CamelFolder *folder = camel_store_get_junk (store, NULL);

	if (folder) {
		GPtrArray *uids;
		int i;

		uids =  camel_folder_get_uids (folder);
		camel_folder_freeze(folder);
		for (i=0;i<uids->len;i++)
			camel_folder_set_message_flags(folder, uids->pdata[i], CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN);
		camel_folder_thaw(folder);
		camel_folder_free_uids (folder, uids);
	}
}

static CORBA_boolean
impl_quit(PortableServer_Servant servant, CORBA_Environment *ev)
{
	MailComponent *mc = MAIL_COMPONENT(bonobo_object_from_servant(servant));

	if (mc->priv->quit_state == -1)
		mc->priv->quit_state = MC_QUIT_START;

	mail_config_prune_proxies ();
	switch (mc->priv->quit_state) {
	case MC_QUIT_START: {
		extern int camel_application_is_exiting;
		int now = time(NULL)/60/60/24, days;
		gboolean empty_junk;

		GConfClient *gconf = mail_config_get_gconf_client();

		camel_application_is_exiting = TRUE;

		mail_vfolder_shutdown();

		mc->priv->quit_expunge = gconf_client_get_bool(gconf, "/apps/evolution/mail/trash/empty_on_exit", NULL)
			&& ((days = gconf_client_get_int(gconf, "/apps/evolution/mail/trash/empty_on_exit_days", NULL)) == 0
			    || (days + gconf_client_get_int(gconf, "/apps/evolution/mail/trash/empty_date", NULL)) <= now);

		empty_junk = gconf_client_get_bool(gconf, "/apps/evolution/mail/junk/empty_on_exit", NULL)
			&& ((days = gconf_client_get_int(gconf, "/apps/evolution/mail/junk/empty_on_exit_days", NULL)) == 0
			    || (days + gconf_client_get_int(gconf, "/apps/evolution/mail/junk/empty_date", NULL)) <= now);

		if (empty_junk) {
			g_hash_table_foreach(mc->priv->store_hash, (GHFunc)mc_quit_delete, mc);
			gconf_client_set_int(gconf, "/apps/evolution/mail/junk/empty_date", now, NULL);
		}

		g_hash_table_foreach(mc->priv->store_hash, (GHFunc)mc_quit_sync, mc);

		if (mc->priv->quit_expunge)
			gconf_client_set_int(gconf, "/apps/evolution/mail/trash/empty_date", now, NULL);

		mc->priv->quit_state = MC_QUIT_SYNC;
	}
		/* Falls through */
	case MC_QUIT_SYNC:
		if (mc->priv->quit_count > 0)
			return FALSE;

		mail_cancel_all();
		mc->priv->quit_state = MC_QUIT_THREADS;

		/* Falls through */
	case MC_QUIT_THREADS:
		/* should we keep cancelling? */
		return !mail_msg_active((unsigned int)-1);
	}

	return TRUE;
}

static GNOME_Evolution_CreatableItemTypeList *
impl__get_userCreatableItems (PortableServer_Servant servant, CORBA_Environment *ev)
{
	GNOME_Evolution_CreatableItemTypeList *list = GNOME_Evolution_CreatableItemTypeList__alloc ();

	list->_length  = 2;
	list->_maximum = list->_length;
	list->_buffer  = GNOME_Evolution_CreatableItemTypeList_allocbuf (list->_length);

	CORBA_sequence_set_release (list, FALSE);

	list->_buffer[0].id = "message";
	list->_buffer[0].description = _("New Mail Message");
	list->_buffer[0].menuDescription = _("_Mail Message");
	list->_buffer[0].tooltip = _("Compose a new mail message");
	list->_buffer[0].menuShortcut = 'm';
	list->_buffer[0].iconName = "mail-message-new";
	list->_buffer[0].type = GNOME_Evolution_CREATABLE_OBJECT;

	list->_buffer[1].id = "folder";
	list->_buffer[1].description = _("New Mail Folder");
	list->_buffer[1].menuDescription = _("Mail _Folder");
	list->_buffer[1].tooltip = _("Create a new mail folder");
	list->_buffer[1].menuShortcut = '\0';
	list->_buffer[1].iconName = "folder-new";
	list->_buffer[1].type = GNOME_Evolution_CREATABLE_FOLDER;

	return list;
}

static int
create_item(const char *type, EMFolderTreeModel *model, const char *uri, gpointer tree)
{
	if (strcmp(type, "message") == 0) {
		if (!em_utils_check_user_can_send_mail(NULL))
			return 0;

		em_utils_compose_new_message(uri);
	} else if (strcmp(type, "folder") == 0) {
		em_folder_utils_create_folder(NULL, tree);
	} else
		return -1;

	return 0;
}

static void
create_local_item_cb(EUserCreatableItemsHandler *handler, const char *item_type_name, void *data)
{
	EMFolderTree *tree = data;
	char *uri = em_folder_tree_get_selected_uri(tree);

	create_item(item_type_name, em_folder_tree_get_model(tree), uri, (gpointer) tree);
	g_free(uri);
}

static void
impl_requestCreateItem (PortableServer_Servant servant,
			const CORBA_char *item_type_name,
			CORBA_Environment *ev)
{
	MailComponent *mc = MAIL_COMPONENT(bonobo_object_from_servant(servant));

	if (create_item(item_type_name, mc->priv->model, NULL, NULL) == -1) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_Component_UnknownType, NULL);
	}
}

static void
handleuri_got_folder(char *uri, CamelFolder *folder, void *data)
{
	CamelURL *url = data;
	EMMessageBrowser *emmb;

	if (folder != NULL) {
		const char *reply = camel_url_get_param(url, "reply");
		const char *forward = camel_url_get_param(url, "forward");
		int mode;

		if (reply) {

			if (!strcmp(reply, "all"))
				mode = REPLY_MODE_ALL;
			else if (!strcmp(reply, "list"))
				mode = REPLY_MODE_LIST;
			else /* if "sender" or anything else */
				mode = REPLY_MODE_SENDER;

			em_utils_reply_to_message(folder, camel_url_get_param(url, "uid"), NULL, mode, NULL);
		} else if (forward) {
			GPtrArray *uids;
			const char* uid;

			uid = camel_url_get_param(url, "uid");
			if (uid == NULL)
				g_warning("Could not forward the message. UID is NULL.");
			else {
				uids = g_ptr_array_new();
				g_ptr_array_add(uids, g_strdup(uid));

				if (!strcmp(forward, "attached"))
					em_utils_forward_attached(folder, uids, uri);
				else if (!strcmp(forward, "inline"))
					em_utils_forward_inline(folder, uids, uri);
				else if (!strcmp(forward, "quoted"))
					em_utils_forward_quoted(folder, uids, uri);
				else { /* Just the default forward */
					em_utils_forward_messages(folder, uids, uri);
				}
			}
		} else {
			emmb = (EMMessageBrowser *)em_message_browser_window_new();
			/*message_list_set_threaded(((EMFolderView *)emmb)->list, emfv->list->threaded);*/
			/* FIXME: session needs to be passed easier than this */
			em_format_set_session((EMFormat *)((EMFolderView *)emmb)->preview, session);
			em_folder_view_set_folder((EMFolderView *)emmb, folder, uri);
			em_folder_view_set_message((EMFolderView *)emmb, camel_url_get_param(url, "uid"), FALSE);
			gtk_widget_show(emmb->window);
		}
	} else {
		g_warning("Couldn't open folder '%s'", uri);
	}
	camel_url_free(url);
}

static void
impl_handleURI (PortableServer_Servant servant, const char *uri, CORBA_Environment *ev)
{
	if (!strncmp (uri, "mailto:", 7)) {
		if (!em_utils_check_user_can_send_mail(NULL))
			return;

		em_utils_compose_new_message_with_mailto (uri, NULL);
	} else if (!strncmp(uri, "email:", 6)) {
		CamelURL *url = camel_url_new(uri, NULL);

		if (camel_url_get_param(url, "uid") != NULL) {
			char *curi = em_uri_to_camel(uri);

			mail_get_folder(curi, 0, handleuri_got_folder, url, mail_msg_unordered_push);
			g_free(curi);
		} else {
			g_warning("email uri's must include a uid parameter");
			camel_url_free(url);
		}
	}
}

static void
impl_sendAndReceive (PortableServer_Servant servant, CORBA_Environment *ev)
{
	em_utils_clear_get_password_canceled_accounts_flag ();
	mail_send_receive ();
}

static void
impl_upgradeFromVersion (PortableServer_Servant servant, const short major, const short minor, const short revision, CORBA_Environment *ev)
{
	MailComponent *component;
	CamelException ex;

	component = mail_component_peek ();

	camel_exception_init (&ex);
	if (em_migrate (e_get_user_data_dir (), major, minor, revision, &ex) == -1) {
		GNOME_Evolution_Component_UpgradeFailed *failedex;

		failedex = GNOME_Evolution_Component_UpgradeFailed__alloc();
		failedex->what = CORBA_string_dup(_("Failed upgrading Mail settings or folders."));
		failedex->why = CORBA_string_dup(ex.desc);
		CORBA_exception_set(ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Component_UpgradeFailed, failedex);
	}

	camel_exception_clear (&ex);
}

struct _setline_data {
	GNOME_Evolution_Listener listener;
	CORBA_boolean status;
	int pending;
};

static void
setline_done(CamelStore *store, void *data)
{
	struct _setline_data *sd = data;

	g_return_if_fail (sd->pending > 0);

	sd->pending--;
	if (sd->pending == 0) {
		CORBA_Environment ev = { NULL };

		GNOME_Evolution_Listener_complete(sd->listener, &ev);
		CORBA_exception_free(&ev);
		CORBA_Object_release(sd->listener, &ev);
		CORBA_exception_free(&ev);
		if (!sd->status)
			camel_session_set_online(session, sd->status);
		g_free(sd);
	}
}

static void
setline_check(void *key, void *value, void *data)
{
	CamelService *service = key;
	struct _setline_data *sd = data;

	if (CAMEL_IS_DISCO_STORE(service)
	    || CAMEL_IS_OFFLINE_STORE(service)) {
		sd->pending++;
		mail_store_set_offline((CamelStore *)service, !sd->status, setline_done, sd);
	}
}

int
status_check (GNOME_Evolution_ShellState shell_state)
{
	int status = 0;

	switch (shell_state)
	{
	    case GNOME_Evolution_USER_OFFLINE:
		    status = OFFLINE;
		    if (em_utils_prompt_user (NULL, "/apps/evolution/mail/prompts/quick_offline", "mail:ask-quick-offline", NULL))
		    	break;
	    case GNOME_Evolution_FORCED_OFFLINE:
		    /*Network is down so change network state on the camel session*/
		    status = OFFLINE;
		    /* Cancel all operations as they wont happen anyway cos Network is down*/
		    mail_cancel_all ();
		    camel_session_set_network_state (session, FALSE);
		    break;
	    case GNOME_Evolution_USER_ONLINE:
		    camel_session_set_network_state (session, TRUE);
		    status = ONLINE;
	}

	return status;
}

static void
impl_setLineStatus(PortableServer_Servant servant, GNOME_Evolution_ShellState shell_state, GNOME_Evolution_Listener listener, CORBA_Environment *ev)
{
	struct _setline_data *sd;
	int status = status_check(shell_state);

	/* This will dis/enable further auto-mail-check action. */
	/* FIXME: If send/receive active, wait for it to finish? */
	if (status)
		camel_session_set_online(session, status);

	sd = g_malloc0(sizeof(*sd));
	sd->status = status;
	sd->listener = CORBA_Object_duplicate(listener, ev);
	if (ev->_major == CORBA_NO_EXCEPTION)
		mail_component_stores_foreach(mail_component_peek(), setline_check, sd);
	else
		CORBA_exception_free(ev);

	if (sd->pending == 0) {
		if (sd->listener) {
			CORBA_Object_release(sd->listener, ev);
			CORBA_exception_free(ev);
		}

		g_free(sd);

		if (!status)
			camel_session_set_online(session, status);
		GNOME_Evolution_Listener_complete(listener, ev);
	}
}

static void
impl_mail_test(PortableServer_Servant servant, CORBA_Environment *ev)
{
	printf("*** Testing mail interface!! ***\n");
}

/* Initialization.  */

static void
mail_component_class_init (MailComponentClass *class)
{
	POA_GNOME_Evolution_Component__epv *epv = &((EvolutionComponentClass *)class)->epv;
	POA_GNOME_Evolution_MailComponent__epv *mepv = &class->epv;
	GObjectClass *object_class = G_OBJECT_CLASS (class);

	parent_class = g_type_class_peek_parent (class);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	epv->createView          = impl_createView;
	epv->requestQuit = impl_requestQuit;
	epv->quit = impl_quit;
	epv->_get_userCreatableItems = impl__get_userCreatableItems;
	epv->requestCreateItem       = impl_requestCreateItem;
	epv->handleURI               = impl_handleURI;
	epv->sendAndReceive          = impl_sendAndReceive;
	epv->upgradeFromVersion      = impl_upgradeFromVersion;
	epv->setLineStatus	     = impl_setLineStatus;

	mepv->test = impl_mail_test;
}

static void
store_hash_free (struct _store_info *si)
{
	si->removed = 1;
	store_info_unref(si);
}

static void
mail_component_init (MailComponent *component)
{
	MailComponentPrivate *priv;

	priv = g_new0 (MailComponentPrivate, 1);
	component->priv = priv;

	priv->lock = g_mutex_new();
	priv->quit_state = -1;

	/* FIXME This is used as both a filename and URI path throughout
	 *       the mail code.  Need to clean this up; maybe provide a
	 *       mail_component_get_base_uri() function. */
	priv->base_directory = g_build_filename (e_get_user_data_dir (), "mail", NULL);
#ifdef G_OS_WIN32
	{
		char *p = priv->base_directory;
		while ((p = strchr(p, '\\')))
			*p++ = '/';
	}
#endif

	if (g_mkdir_with_parents (e_get_user_data_dir (), 0777) == -1 && errno != EEXIST)
		abort ();

	priv->model = em_folder_tree_model_new (e_get_user_data_dir ());
	priv->logger = e_logger_create ("mail");
	priv->activity_handler = e_activity_handler_new ();
	e_activity_handler_set_logger (priv->activity_handler, priv->logger);
	e_activity_handler_set_error_flush_time (priv->activity_handler, mail_config_get_error_timeout ()*1000);

	mail_session_init (e_get_user_data_dir ());

	priv->async_event = mail_async_event_new();
	priv->store_hash = g_hash_table_new_full (
		NULL, NULL,
		(GDestroyNotify) NULL,
		(GDestroyNotify) store_hash_free);

	mail_autoreceive_init();
}

/* Public API.  */
MailComponent *
mail_component_peek (void)
{
	static MailComponent *component = NULL;

	if (component == NULL)
		component = g_object_new(mail_component_get_type(), NULL);

	return component;
}

const char *
mail_component_peek_base_directory (MailComponent *component)
{
	MAIL_COMPONENT_DEFAULT(component);

	return component->priv->base_directory;
}

RuleContext *
mail_component_peek_search_context (MailComponent *component)
{
	MAIL_COMPONENT_DEFAULT(component);

	setup_search_context(component);

	return component->priv->search_context;
}

EActivityHandler *
mail_component_peek_activity_handler (MailComponent *component)
{
	MAIL_COMPONENT_DEFAULT(component);

	return component->priv->activity_handler;
}

struct _CamelSession *mail_component_peek_session(MailComponent *component)
{
	MAIL_COMPONENT_DEFAULT(component);

	return session;
}

void
mail_component_add_store (MailComponent *component, CamelStore *store, const char *name)
{
	mc_add_store(component, store, name, NULL);
}

/**
 * mail_component_load_store_by_uri:
 * @component: mail component
 * @uri: uri of store
 * @name: name of store (used for display purposes)
 *
 * Return value: Pointer to the newly added CamelStore.  The caller is supposed
 * to ref the object if it wants to store it.
 **/
CamelStore *
mail_component_load_store_by_uri (MailComponent *component, const char *uri, const char *name)
{
	CamelException ex;
	CamelStore *store;
	CamelProvider *prov;

	MAIL_COMPONENT_DEFAULT(component);

	camel_exception_init (&ex);

	/* Load the service (don't connect!). Check its provider and
	 * see if this belongs in the shell's folder list. If so, add
	 * it.
	 */

	prov = camel_provider_get(uri, &ex);
	if (prov == NULL) {
		/* EPFIXME: real error dialog */
		g_warning ("couldn't get service %s: %s\n", uri,
			   camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
		return NULL;
	}

	if (!(prov->flags & CAMEL_PROVIDER_IS_STORAGE))
		return NULL;

	store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex);
	if (store == NULL) {
		/* EPFIXME: real error dialog */
		g_warning ("couldn't get service %s: %s\n", uri,
			   camel_exception_get_description (&ex));
		camel_exception_clear (&ex);
		return NULL;
	}

	mail_component_add_store(component, store, name);
	camel_object_unref (store);

	return store;
}

static void
store_disconnect (CamelStore *store, void *event_data, void *user_data)
{
	camel_service_disconnect (CAMEL_SERVICE (store), TRUE, NULL);
	camel_object_unref (store);
}

void
mail_component_remove_store (MailComponent *component, CamelStore *store)
{
	MailComponentPrivate *priv;

	MAIL_COMPONENT_DEFAULT(component);

	priv = component->priv;

	/* Because the store_hash holds a reference to each store
	 * used as a key in it, none of them will ever be gc'ed, meaning
	 * any call to camel_session_get_{service,store} with the same
	 * URL will always return the same object. So this works.
	 */

	if (g_hash_table_lookup (priv->store_hash, store) == NULL)
		return;

	camel_object_ref (store);
	g_hash_table_remove (priv->store_hash, store);

	/* so i guess potentially we could have a race, add a store while one
	   being removed.  ?? */
	mail_note_store_remove (store);

	em_folder_tree_model_remove_store (priv->model, store);

	mail_async_event_emit (priv->async_event, MAIL_ASYNC_THREAD, (MailAsyncFunc) store_disconnect, store, NULL, NULL);
}

void
mail_component_remove_store_by_uri (MailComponent *component, const char *uri)
{
	CamelProvider *prov;
	CamelStore *store;

	MAIL_COMPONENT_DEFAULT(component);

	if (!(prov = camel_provider_get(uri, NULL)))
		return;

	if (!(prov->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;

	store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, NULL);
	if (store != NULL) {
		mail_component_remove_store (component, store);
		camel_object_unref (store);
	}
}

int
mail_component_get_store_count (MailComponent *component)
{
	MAIL_COMPONENT_DEFAULT(component);

	return g_hash_table_size (component->priv->store_hash);
}

/* need to map from internal struct to external api */
struct _store_foreach_data {
	GHFunc func;
	void *data;
};

static void
mc_stores_foreach(CamelStore *store, struct _store_info *si, struct _store_foreach_data *data)
{
	data->func((void *)store, (void *)si->name, data->data);
}

void
mail_component_stores_foreach (MailComponent *component, GHFunc func, void *user_data)
{
	struct _store_foreach_data data = { func, user_data };

	MAIL_COMPONENT_DEFAULT(component);

	g_hash_table_foreach (component->priv->store_hash, (GHFunc)mc_stores_foreach, &data);
}

void
mail_component_remove_folder (MailComponent *component, CamelStore *store, const char *path)
{
	MAIL_COMPONENT_DEFAULT(component);

	/* FIXME: implement me. but first, am I really even needed? */
}

EMFolderTreeModel *
mail_component_peek_tree_model (MailComponent *component)
{
	MAIL_COMPONENT_DEFAULT(component);

	return component->priv->model;
}

CamelStore *
mail_component_peek_local_store (MailComponent *mc)
{
	MAIL_COMPONENT_DEFAULT (mc);
	mc_setup_local_store (mc);

	return mc->priv->local_store;
}

/**
 * mail_component_get_folder:
 * @mc:
 * @id:
 *
 * Get a standard/default folder by id.  This call is thread-safe.
 *
 * Return value:
 **/
struct _CamelFolder *
mail_component_get_folder(MailComponent *mc, enum _mail_component_folder_t id)
{
	g_return_val_if_fail (id <= MAIL_COMPONENT_FOLDER_LOCAL_INBOX, NULL);

	MAIL_COMPONENT_DEFAULT(mc);
	mc_setup_local_store(mc);

	return mc_default_folders[id].folder;
}

/**
 * mail_component_get_folder_uri:
 * @mc:
 * @id:
 *
 * Get a standard/default folder's uri.  This call is thread-safe.
 *
 * Return value:
 **/
const char *
mail_component_get_folder_uri(MailComponent *mc, enum _mail_component_folder_t id)
{
	g_return_val_if_fail (id <= MAIL_COMPONENT_FOLDER_LOCAL_INBOX, NULL);

	MAIL_COMPONENT_DEFAULT(mc);
	mc_setup_local_store(mc);

	return mc_default_folders[id].uri;
}

/**
 * mail_indicate_new_mail
 * Indicates new mail in a shell window.
 * @param have_new_mail TRUE when have new mail, false otherwise.
 **/
void
mail_indicate_new_mail (gboolean have_new_mail)
{
	const char *icon = NULL;
	MailComponent *mc = mail_component_peek ();

	g_return_if_fail (mc != NULL);

	if (have_new_mail)
		icon = "mail-unread";

	if (mc->priv->component_view)
		e_component_view_set_button_icon (mc->priv->component_view, icon);
}

struct _log_data {
	int level;
	char *key;
	char *text;
	char *stock_id;
	GdkPixbuf *pbuf;
} ldata [] = {
	{ E_LOG_ERROR, N_("Error"), N_("Errors"), GTK_STOCK_DIALOG_ERROR },
	{ E_LOG_WARNINGS, N_("Warning"), N_("Warnings and Errors"), GTK_STOCK_DIALOG_WARNING },
	{ E_LOG_DEBUG, N_("Debug"), N_("Error, Warnings and Debug messages"), GTK_STOCK_DIALOG_INFO }
};

enum
{
	COL_LEVEL = 0,
	COL_TIME,
	COL_DATA
};

static gboolean
query_tooltip_cb (GtkTreeView *view,
                  gint x,
                  gint y,
                  gboolean keyboard_mode,
                  GtkTooltip *tooltip)
{
	GtkTreeViewColumn *column;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gint level;

	if (!gtk_tree_view_get_tooltip_context (
		view, &x, &y, keyboard_mode, NULL, &path, &iter))
		return FALSE;

	/* Figure out which column we're pointing at. */
	if (keyboard_mode)
		gtk_tree_view_get_cursor (view, NULL, &column);
	else
		gtk_tree_view_get_path_at_pos (
			view, x, y, NULL, &column, NULL, NULL);

	/* Restrict the tip area to a single cell. */
	gtk_tree_view_set_tooltip_cell (view, tooltip, path, column, NULL);

	/* This only works if the tree view is NOT reorderable. */
	if (column != gtk_tree_view_get_column (view, 0))
		return FALSE;

	model = gtk_tree_view_get_model (view);
	gtk_tree_model_get (model, &iter, COL_LEVEL, &level, -1);
	gtk_tooltip_set_text (tooltip, ldata[level].key);

	return TRUE;
}

static void
render_pixbuf (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
	       GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	gint level;

	gtk_tree_model_get (model, iter, COL_LEVEL, &level, -1);
	g_object_set (
		renderer, "stock-id", ldata[level].stock_id,
		"stock-size", GTK_ICON_SIZE_MENU, NULL);
}

static void
render_date (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
	      GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	time_t t;
	char sdt[100]; /* Should be sufficient? */
	
	gtk_tree_model_get (model, iter, COL_TIME, &t, -1);
	strftime (sdt, 100, "%x %X", localtime (&t));
	g_object_set (renderer, "text", sdt, NULL);
}



static void
append_logs (const char *txt, GtkListStore *store)
{
	char **str;
	
	str = g_strsplit (txt, 	":", 3);
	if (str[0] && str[1] && str[2]) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			COL_LEVEL, atoi (str[0]),
			COL_TIME, atol (str[1]),
			COL_DATA, g_strstrip (str[2]),
			-1);
	} else 
		g_printerr ("Unable to decode error log: %s\n", txt);
	
	g_strfreev (str);
}

static void
spin_value_changed (GtkSpinButton *b, gpointer data)
{
	int value = gtk_spin_button_get_value_as_int (b);
	GConfClient *client = mail_config_get_gconf_client ();

	gconf_client_set_int (client, "/apps/evolution/mail/display/error_timeout", value, NULL);
}

void
mail_component_show_logger (gpointer top)
{
	MailComponent *mc = mail_component_peek ();
	GConfBridge *bridge;
	GtkWidget *container;
	GtkWidget *label;
	GtkWidget *toplevel;
	GtkWidget *vbox;
	GtkWidget *widget;
	GtkWidget *window;
	ELogger *logger = mc->priv->logger;
	int i;
	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	bridge = gconf_bridge_get ();
	toplevel = gtk_widget_get_toplevel (top);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 500, 400);
	gtk_window_set_title (GTK_WINDOW (window), _("Debug Logs"));
	gtk_window_set_transient_for (
		GTK_WINDOW (window), GTK_WINDOW (toplevel));
	gtk_container_set_border_width (GTK_CONTAINER (window), 12);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	container = gtk_hbox_new (FALSE, 6);	
	gtk_box_pack_start (GTK_BOX (vbox), container, FALSE, FALSE, 0);

	/* Translators: This is the first part of the sentence
	 * "Show _errors in the status bar for" - XXX - "second(s)." */
	widget = gtk_label_new_with_mnemonic (
		_("Show _errors in the status bar for"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	label = widget;

	widget = gtk_spin_button_new_with_range (1.0, 60.0, 1.0);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (widget),
		(gdouble) mail_config_get_error_timeout ());
	g_signal_connect (
		widget, "value-changed",
		G_CALLBACK (spin_value_changed), NULL);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	/* Translators: This is the second part of the sentence
	 * "Show _errors in the status bar for" - XXX - "second(s)." */
	widget = gtk_label_new_with_mnemonic (_("second(s)."));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	container = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (vbox), container, FALSE, FALSE, 0);

	widget = gtk_label_new_with_mnemonic (_("Log Messages:"));
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	label = widget;

	widget = gtk_combo_box_new_text ();
	for (i = E_LOG_ERROR; i <= E_LOG_DEBUG; i++)
		gtk_combo_box_append_text (
			GTK_COMBO_BOX (widget), ldata[i].text);
	gconf_bridge_bind_property (
		bridge, "/apps/evolution/mail/display/error_level",
		G_OBJECT (widget), "active");
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	store = gtk_list_store_new (3, G_TYPE_INT, G_TYPE_LONG, G_TYPE_STRING);
	e_logger_get_logs (logger, (ELogFunction) append_logs, store); 
	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (store), COL_TIME, GTK_SORT_DESCENDING);

	container = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (container),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (container), GTK_SHADOW_IN);
	gtk_box_pack_start (GTK_BOX (vbox), container, TRUE, TRUE, 0);

	widget = gtk_tree_view_new();
	gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (widget), TRUE);	
	gtk_tree_view_set_reorderable (GTK_TREE_VIEW (widget), FALSE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (widget), GTK_TREE_MODEL (store));
	gtk_tree_view_set_search_column (GTK_TREE_VIEW (widget), COL_DATA);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (widget), TRUE);
	gtk_widget_set_has_tooltip (widget, TRUE);
	gtk_container_add (GTK_CONTAINER (container), widget);

	g_signal_connect (
		widget, "query-tooltip",
		G_CALLBACK (query_tooltip_cb), NULL);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Log Level"));
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (
		column, renderer, render_pixbuf, NULL, NULL);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, _("Time"));	
	gtk_tree_view_append_column (GTK_TREE_VIEW (widget), column);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (
		column, renderer, render_date, NULL, NULL);

	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_insert_column_with_attributes(
		GTK_TREE_VIEW (widget), -1, _("Messages"),
		renderer, "markup", COL_DATA, NULL);

	container = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (
		GTK_BUTTON_BOX (container), GTK_BUTTONBOX_END);
	gtk_box_pack_start (GTK_BOX (vbox), container, FALSE, FALSE, 0);

	widget = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
	gtk_widget_set_tooltip_text (widget, _("Close this window"));
	g_signal_connect_swapped (
		widget, "clicked",
		G_CALLBACK (gtk_widget_destroy), window);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	gtk_widget_show_all (window);
}

BONOBO_TYPE_FUNC_FULL (MailComponent, GNOME_Evolution_MailComponent, PARENT_TYPE, mail_component)

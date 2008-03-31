#include "e-account-combo-box.h"

#include <string.h>
#include <camel/camel-store.h>

#define E_ACCOUNT_COMBO_BOX_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_ACCOUNT_COMBO_BOX, EAccountComboBoxPrivate))

enum {
	COLUMN_STRING,
	COLUMN_ACCOUNT
};

struct _EAccountComboBoxPrivate {
	EAccountList *account_list;
};

static gpointer parent_class;
static CamelSession *camel_session;

static gboolean
account_combo_box_has_dupes (GList *list,
                             const gchar *address)
{
	GList *iter;
	guint count = 0;

	/* Look for duplicates of the given email address. */
	for (iter = list; iter != NULL; iter = iter->next) {
		EAccount *account = iter->data;

		if (strcmp (account->id->address, address) == 0)
			count++;
	}

	return (count > 1);
}

static gboolean
account_combo_box_test_account (EAccount *account)
{
	CamelStore *store;
	CamelException ex;
	const gchar *url;
	gboolean writable = FALSE;

	/* Account must be enabled. */
	if (!account->enabled)
		return FALSE;

	/* Account must have a non-empty email address. */
	if (account->id->address == NULL || *account->id->address == '\0')
		return FALSE;

	/* XXX Not sure I understand this part. */
	if (account->parent_uid == NULL)
		return TRUE;

	/* Account must be writable. */
	camel_exception_init (&ex);
	url = e_account_get_string (account, E_ACCOUNT_SOURCE_URL);
	store = CAMEL_STORE (camel_session_get_service (
		camel_session, url, CAMEL_PROVIDER_STORE, &ex));
	if (store != NULL) {
		writable = (store->mode & CAMEL_STORE_WRITE);
		camel_object_unref (store);
	}
	camel_exception_clear (&ex);

	return writable;
}

static void
account_combo_box_refresh_cb (EAccountList *account_list,
                              EAccount *unused,
                              EAccountComboBox *combo_box)
{
	GtkListStore *store;
	GtkTreeModel *model;
	EIterator *account_iter;
	EAccount *account;
	GHashTable *index;
	GList *list = NULL;
	GList *iter;

	store = gtk_list_store_new (2, G_TYPE_STRING, E_TYPE_ACCOUNT);
	model = GTK_TREE_MODEL (store);

	/* Embed a reverse-lookup index into the list store. */
	index = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) gtk_tree_row_reference_free);
	g_object_set_data_full (
		G_OBJECT (combo_box), "index", index,
		(GDestroyNotify) g_hash_table_destroy);

	if (account_list == NULL)
		goto skip;

	/* Build a list of EAccounts to display. */
	account_iter = e_list_get_iterator (E_LIST (account_list));
	while (e_iterator_is_valid (account_iter)) {
		EAccount *account;

		/* XXX EIterator misuses const. */
		account = (EAccount *) e_iterator_get (account_iter);
		if (account_combo_box_test_account (account))
			list = g_list_prepend (list, account);
		e_iterator_next (account_iter);
	}
	g_object_unref (account_iter);

	list = g_list_reverse (list);

	/* Populate the list store and index. */
	for (iter = list; iter != NULL; iter = iter->next) {
		GtkTreeRowReference *reference;
		GtkTreeIter tree_iter;
		GtkTreePath *path;
		gchar *string;

		account = iter->data;

		/* Show the account name for duplicate email addresses. */
		if (account_combo_box_has_dupes (list, account->id->address))
			string = g_strdup_printf (
				"%s <%s> (%s)",
				account->id->name,
				account->id->address,
				account->name);
		else
			string = g_strdup_printf (
				"%s <%s>",
				account->id->name,
				account->id->address);

		gtk_list_store_append (store, &tree_iter);
		gtk_list_store_set (
			store, &tree_iter,
			COLUMN_STRING, string,
			COLUMN_ACCOUNT, account, -1);

		path = gtk_tree_model_get_path (model, &tree_iter);
		reference = gtk_tree_row_reference_new (model, path);
		g_hash_table_insert (index, account, reference);
		gtk_tree_path_free (path);

		g_free (string);
	}

	g_list_free (list);

skip:
	/* Restore the previously selected account. */
	account = e_account_combo_box_get_active (combo_box);
	if (account != NULL)
		g_object_ref (account);
	gtk_combo_box_set_model (GTK_COMBO_BOX (combo_box), model);
	e_account_combo_box_set_active (combo_box, account);
	if (account != NULL)
		g_object_unref (account);
}

static GObject *
account_combo_box_constructor (GType type,
                               guint n_construct_properties,
                               GObjectConstructParam *construct_properties)
{
	GObject *object;
	GtkCellRenderer *renderer;

	/* Chain up to parent's constructor() method. */
	object = G_OBJECT_CLASS (parent_class)->constructor (
		type, n_construct_properties, construct_properties);

	renderer = gtk_cell_renderer_text_new ();

	gtk_cell_layout_pack_start (
		GTK_CELL_LAYOUT (object), renderer, TRUE);
	gtk_cell_layout_add_attribute (
		GTK_CELL_LAYOUT (object), renderer, "text", COLUMN_STRING);

	return object;
}

static void
account_combo_box_dispose (GObject *object)
{
	EAccountComboBoxPrivate *priv;

	priv = E_ACCOUNT_COMBO_BOX_GET_PRIVATE (object);

	if (priv->account_list != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->account_list,
			account_combo_box_refresh_cb, object);
		g_object_unref (priv->account_list);
		priv->account_list = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
account_combo_box_class_init (EAccountComboBoxClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EAccountComboBoxPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructor = account_combo_box_constructor;
	object_class->dispose = account_combo_box_dispose;
}

static void
account_combo_box_init (EAccountComboBox *combo_box)
{
	combo_box->priv = E_ACCOUNT_COMBO_BOX_GET_PRIVATE (combo_box);
}

GType
e_account_combo_box_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EAccountComboBoxClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) account_combo_box_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EAccountComboBox),
			0,     /* n_preallocs */
			(GInstanceInitFunc) account_combo_box_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_COMBO_BOX, "EAccountComboBox", &type_info, 0);
	}

	return type;
}

GtkWidget *
e_account_combo_box_new (void)
{
	return g_object_new (E_TYPE_ACCOUNT_COMBO_BOX, NULL);
}

void
e_account_combo_box_set_session (CamelSession *session)
{
	/* XXX Really gross hack.
	 *
	 * We need a CamelSession to test whether a given EAccount is
	 * writable.  The global CamelSession object is defined in the
	 * mailer, but we're too far down the stack to access it.  So
	 * we have to rely on someone passing us a reference to it.
	 *
	 * A much cleaner solution would be to store the writeability
	 * of an account directly into the EAccount, but this would likely
	 * require breaking ABI and all the fun that goes along with that.
	 */

	camel_session = session;
}

void
e_account_combo_box_set_account_list (EAccountComboBox *combo_box,
                                      EAccountList *account_list)
{
	EAccountComboBoxPrivate *priv;

	g_return_if_fail (E_IS_ACCOUNT_COMBO_BOX (combo_box));

	if (account_list != NULL)
		g_return_if_fail (E_IS_ACCOUNT_LIST (account_list));

	priv = E_ACCOUNT_COMBO_BOX_GET_PRIVATE (combo_box);

	if (priv->account_list != NULL) {
		g_signal_handlers_disconnect_by_func (
			priv->account_list,
			account_combo_box_refresh_cb, combo_box);
		g_object_unref (priv->account_list);
		priv->account_list = NULL;
	}

	if (account_list != NULL) {
		priv->account_list = g_object_ref (account_list);

		/* Listen for changes to the account list. */
		g_signal_connect (
			priv->account_list, "account-added",
			G_CALLBACK (account_combo_box_refresh_cb), combo_box);
		g_signal_connect (
			priv->account_list, "account-changed",
			G_CALLBACK (account_combo_box_refresh_cb), combo_box);
		g_signal_connect (
			priv->account_list, "account-removed",
			G_CALLBACK (account_combo_box_refresh_cb), combo_box);
	}

	account_combo_box_refresh_cb (account_list, NULL, combo_box);
}

EAccount *
e_account_combo_box_get_active (EAccountComboBox *combo_box)
{
	EAccount *account;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean iter_set;

	g_return_val_if_fail (E_IS_ACCOUNT_COMBO_BOX (combo_box), NULL);

	iter_set = gtk_combo_box_get_active_iter (
		GTK_COMBO_BOX (combo_box), &iter);
	if (!iter_set)
		return NULL;

	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	gtk_tree_model_get (model, &iter, COLUMN_ACCOUNT, &account, -1);

	return account;
}

gboolean
e_account_combo_box_set_active (EAccountComboBox *combo_box,
                                EAccount *account)
{
	GHashTable *index;
	EAccountList *account_list;
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	gboolean iter_set;

	g_return_val_if_fail (E_IS_ACCOUNT_COMBO_BOX (combo_box), FALSE);

	if (account != NULL)
		g_return_val_if_fail (E_IS_ACCOUNT (account), FALSE);

	account_list = combo_box->priv->account_list;
	g_return_val_if_fail (account_list != NULL, FALSE);

	/* Failure here indicates a programming error. */
	index = g_object_get_data (G_OBJECT (combo_box), "index");
	g_assert (index != NULL);

	/* NULL means select the default account. */
	/* XXX EAccountList misuses const. */
	if (account == NULL)
		account = (EAccount *)
			e_account_list_get_default (account_list);

	/* Lookup the tree row reference for the account. */
	reference = g_hash_table_lookup (index, account);
	if (reference == NULL)
		return FALSE;

	/* Convert the reference to a tree iterator. */
	path = gtk_tree_row_reference_get_path (reference);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combo_box));
	iter_set = gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	if (!iter_set)
		return FALSE;

	/* Activate the corresponding combo box item. */
	gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combo_box), &iter);

	return TRUE;
}

const gchar *
e_account_combo_box_get_active_name (EAccountComboBox *combo_box)
{
	EAccount *account;

	g_return_val_if_fail (E_IS_ACCOUNT_COMBO_BOX (combo_box), NULL);

	account = e_account_combo_box_get_active (combo_box);
	return (account != NULL) ? account->name : NULL;
}

gboolean
e_account_combo_box_set_active_name (EAccountComboBox *combo_box,
                                     const gchar *account_name)
{
	EAccountList *account_list;
	EAccount *account;

	g_return_val_if_fail (E_IS_ACCOUNT_COMBO_BOX (combo_box), FALSE);

	account_list = combo_box->priv->account_list;
	g_return_val_if_fail (account_list != NULL, FALSE);

	/* XXX EAccountList misuses const. */
	account = (EAccount *) e_account_list_find (
		account_list, E_ACCOUNT_FIND_NAME, account_name);

	if (account == NULL)
		return FALSE;

	return e_account_combo_box_set_active (combo_box, account);
}

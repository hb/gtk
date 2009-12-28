/* gtkundo.c
 * Copyright (C) 2009  Holger Berndt <berndth@gmx.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "gtkundo.h"
#include "gtkintl.h"
#include "gtkmarshalers.h"
#include "gtkprivate.h"


/**
 * SECTION:gtkundo
 * @title: GtkUndo
 * @short_description: Undo stack
 *
 * The #GtkUndo class implements an undo stack.
 *
 * TODO: Verbose description here
 *
 * Since: 2.20
 */

enum {
  PROP_0,
  PROP_MAX_LENGTH,
};

enum {
  CAN_UNDO,
  CAN_REDO,
  CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _GtkUndoPrivate
{
  gint max_length;

  /* these are lists of GNode's */
  GList *undo_stack;
  GList *redo_stack;
  gint undo_length;
  gint redo_length;
  GHashTable *method_hash;
  guint group_depth;
};

typedef struct _GtkUndoEntry GtkUndoEntry;
struct _GtkUndoEntry {
  gchar *description;
  GtkUndoSet *set;
  gpointer data;
};


G_DEFINE_TYPE (GtkUndo, gtk_undo, G_TYPE_OBJECT);


/* --------------------------------------------------------------------------------
 *
 */

/* Free an undo entry itself and all members. */
static void
free_entry (GtkUndoEntry *entry)
{
  /* Call virtual functions for data entries */
  if(entry->set) {
    if (entry->set->do_free)
      entry->set->do_free(entry->data);
  }
  else
    g_print ("hhb: freeing a non-set entry\n"); // TODO
  g_free(entry->description);
  g_free(entry);
}

/* Change length of the undo stack */
static void
change_len_undo (GtkUndo *undo, gint num)
{
  undo->priv->undo_length = undo->priv->undo_length + num;

  if ((num > 0) && (undo->priv->undo_length == 1))
    g_signal_emit(undo, signals[CAN_UNDO], 0, TRUE);
  else if ((num < 0) && (undo->priv->undo_length == 0))
    g_signal_emit(undo, signals[CAN_UNDO], 0, FALSE);
}

/* Change length of the redo stack */
static void
change_len_redo(GtkUndo *undo, gint num)
{
  undo->priv->redo_length = undo->priv->redo_length + num;

  if ((num > 0) && (undo->priv->redo_length == 1))
    g_signal_emit (undo, signals[CAN_REDO], 0, TRUE);
  else if ((num < 0) && (undo->priv->redo_length == 0))
    g_signal_emit (undo, signals[CAN_REDO], 0, FALSE);
}

static gboolean
traverse_free_entry (GNode *node, gpointer data)
{
  if (node && node->data)
    free_entry ((GtkUndoEntry*)node->data);
  return FALSE;
}

/* Clear the undo stack */
static void
clear_undo (GtkUndo *undo)
{
  GList *walk;

  change_len_undo (undo, -undo->priv->undo_length);

  for (walk = undo->priv->undo_stack; walk; walk = walk->next) {
    g_node_traverse ((GNode*)walk->data, G_POST_ORDER, G_TRAVERSE_ALL, -1, traverse_free_entry, NULL);
    g_node_destroy (walk->data);
  }
  g_list_free (undo->priv->undo_stack);
  undo->priv->undo_stack = NULL;
}

static void
clear_redo (GtkUndo *undo)
{
  GList *walk;

  change_len_redo (undo, -undo->priv->redo_length);

  for (walk = undo->priv->redo_stack; walk; walk = walk->next) {
    g_node_traverse ((GNode*)walk->data, G_POST_ORDER, G_TRAVERSE_ALL, -1, traverse_free_entry, NULL);
    g_node_destroy (walk->data);
  }
  g_list_free (undo->priv->redo_stack);
  undo->priv->redo_stack = NULL;
}

static void
free_stack_entry (GList **stack, GList *element)
{
  if (!element || !element->data)
    return;

  g_node_traverse ((GNode*)element->data, G_POST_ORDER, G_TRAVERSE_ALL, -1, traverse_free_entry, NULL);
  g_node_destroy (element->data);
  *stack = g_list_delete_link (*stack, element);
}

/* Free last element of undo stack (usually because the stack
 * grew beyond its maximum length). */
static void
free_last_entry (GtkUndo *undo)
{
  free_stack_entry (&(undo->priv->undo_stack), g_list_last (undo->priv->undo_stack));
  change_len_undo(undo, -1);
}

/* Free first element of undo stack (usually because an undo operation failed */
static void
free_first_entry (GtkUndo *undo, gboolean of_undo)
{
  if (of_undo) {
    free_stack_entry (&(undo->priv->undo_stack), undo->priv->undo_stack);
    change_len_undo(undo, -1);
  }
  else {
    free_stack_entry (&(undo->priv->redo_stack), undo->priv->redo_stack);
    change_len_redo(undo, -1);
  }
}

static void
destroy_undoset(gpointer data)
{
  GtkUndoSet *set = data;
  g_free(set->description);
  g_free(set);
}

static gboolean
traverse_undo_node (GNode *node, gpointer data)
{
  GtkUndoEntry *entry;
  gpointer *success;

  success = data;
  entry = node->data;

  /* call callabck function */
  if (entry && entry->set && entry->set->do_undo) {
    if (!entry->set->do_undo(entry->data))
      *success = FALSE;
  }

  return FALSE;
}

static gboolean
traverse_redo_node (GNode *node, gpointer data)
{
  GtkUndoEntry *entry;
  gpointer *success;

  success = data;
  entry = node->data;

  /* call callabck function */
  if (entry && entry->set && entry->set->do_redo) {
    if (!entry->set->do_redo(entry->data))
      *success = FALSE;
  }

  return FALSE;
}

static gboolean
traverse_reverse_children (GNode *node, gpointer data)
{
  g_node_reverse_children (node);
  return FALSE;
}

/* get best-fitting description for an entry. This is the
 * description of the entry, or the description of the associated
 * set, if no entry-description is available, or a dummy string
 * if no set description is available either. */
static char*
get_entry_description (GtkUndoEntry *entry)
{
  gchar *desc;

  if(entry->description)
    desc = entry->description;
  else if(entry->set && entry->set->description)
    desc = entry->set->description;
  else
    desc = N_("<no description available>");
  return desc;
}

static void
get_descriptions_from_stack_add_node (GtkTreeStore *store, GNode *node, GtkTreeIter *parent_iter)
{
  GtkTreeIter iter;
  guint n_children, ii;

  gtk_tree_store_append (store, &iter, parent_iter);
  gtk_tree_store_set (store, &iter, 0, get_entry_description (node->data), -1);

  n_children = g_node_n_children (node);
  for (ii = 0; ii < n_children; ii++) {
    GNode *child;
    child = g_node_nth_child (node, ii);
    get_descriptions_from_stack_add_node (store, child, &iter);
  }
}

static GtkTreeStore*
get_descriptions_from_stack (GList *stack)
{
  GtkTreeStore *store;
  GList *walk;

  store = gtk_tree_store_new (1, G_TYPE_STRING);
  for (walk = stack; walk; walk = walk->next)
    get_descriptions_from_stack_add_node (store, walk->data, NULL);

  return store;
}

static GNode*
get_node_level (GNode *root, guint depth)
{
  GNode *child;
  depth--;
  for(child = root; depth; depth--)
    child = g_node_first_child(child);
  return child;
}

/* --------------------------------------------------------------------------------
 *
 */

static void
gtk_undo_init (GtkUndo *undo)
{
  GtkUndoPrivate *pv;

  pv = undo->priv = G_TYPE_INSTANCE_GET_PRIVATE (undo, GTK_TYPE_UNDO, GtkUndoPrivate);

  pv->max_length = -1;
  pv->undo_stack = NULL;
  pv->redo_stack = NULL;
  pv->undo_length = 0;
  pv->redo_length = 0;
  pv->method_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, destroy_undoset);
  pv->group_depth = 0;
}

static void
gtk_undo_finalize (GObject *obj)
{
  GtkUndo *undo = GTK_UNDO (obj);

  gtk_undo_clear (undo);
  g_hash_table_destroy (undo->priv->method_hash);
  G_OBJECT_CLASS (gtk_undo_parent_class)->finalize (obj);
}

static void
gtk_undo_set_property (GObject      *obj,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GtkUndo *undo = GTK_UNDO (obj);

  switch (prop_id)
    {
    case PROP_MAX_LENGTH:
      gtk_undo_set_max_length (undo, g_value_get_int (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
gtk_undo_get_property (GObject    *obj,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GtkUndo *undo = GTK_UNDO (obj);

  switch (prop_id)
    {
    case PROP_MAX_LENGTH:
      g_value_set_int (value, gtk_undo_get_max_length (undo));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
gtk_undo_class_init (GtkUndoClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_undo_finalize;
  gobject_class->set_property = gtk_undo_set_property;
  gobject_class->get_property = gtk_undo_get_property;

  g_type_class_add_private (gobject_class, sizeof (GtkUndoPrivate));

  /**
   * GtkUndo:max-length:
   *
   * The maximum number of toplevel entries in the undo stack. -1 means no limit
   *
   * Since: 2.20
   */
  g_object_class_install_property (gobject_class,
                                   PROP_MAX_LENGTH,
                                   g_param_spec_int ("max-length",
                                                     P_("Maximum length"),
                                                     P_("Maximum number of toplevel entries in the undo stack. -1 if no maximum"),
                                   -1, GTK_UNDO_MAX_SIZE, -1,
                                   GTK_PARAM_READWRITE));

  /**
   * GtkUndo::can-undo:
   * @undo: a #GtkUndo
   * @can_undo: TRUE if undo is possible
   *
   * This signal is emitted when the undo changes possibility state
   *
   * Since: 2.20
   */
  signals[CAN_UNDO] = g_signal_new (I_("can-undo"),
                                       GTK_TYPE_UNDO,
                                       G_SIGNAL_RUN_FIRST,
                                       G_STRUCT_OFFSET (GtkUndoClass, can_undo),
                                       NULL, NULL,
                                       _gtk_marshal_VOID__BOOLEAN,
                                       G_TYPE_NONE, 1,
                                       G_TYPE_BOOLEAN);

  /**
   * GtkUndo::can-redo:
   * @undo: a #GtkUndo
   * @can_redo: TRUE if redo is possible
   *
   * This signal is emitted when the redo changes possibility state
   *
   * Since: 2.20
   */
  signals[CAN_REDO] = g_signal_new (I_("can-redo"),
                                       GTK_TYPE_UNDO,
                                       G_SIGNAL_RUN_FIRST,
                                       G_STRUCT_OFFSET (GtkUndoClass, can_redo),
                                       NULL, NULL,
                                       _gtk_marshal_VOID__BOOLEAN,
                                       G_TYPE_NONE, 1,
                                       G_TYPE_BOOLEAN);

  /**
   * GtkUndo::changed:
   * @undo: a #GtkUndo
   *
   * This signal is emitted whenever the undo stack changes
   *
   * Since: 2.20
   */
  signals[CHANGED] = g_signal_new (I_("changed"),
                                      GTK_TYPE_UNDO,
                                      G_SIGNAL_RUN_FIRST,
                                      G_STRUCT_OFFSET (GtkUndoClass, can_redo),
                                      NULL, NULL,
                                      _gtk_marshal_VOID__VOID,
                                      G_TYPE_NONE, 0);
}

/* --------------------------------------------------------------------------------
 *
 */

/**
 * gtk_undo_new:
 *
 * Create a new GtkUndo object.
 *
 * Return value: A new GtkUndo object.
 *
 * Since: 2.20
 **/
GtkUndo*
gtk_undo_new (void)
{
  return g_object_new (GTK_TYPE_UNDO, NULL);
}

/**
 * gtk_undo_new:
 * @undo: a #GtkUndo
 * @name: name for the set
 * @set: the set
 *
 * Register an undo set. A set is a collection of functions
 * that deal with undo/redo operations.
 *
 * Since: 2.20
 **/
void
gtk_undo_register_set (GtkUndo *undo, const char *name, const GtkUndoSet *set)
{
  GtkUndoSet *val;

  g_return_if_fail (GTK_IS_UNDO (undo) && name && set);

  /* add the set to the method hash if it's not present yet */
  if (g_hash_table_lookup (undo->priv->method_hash, name))
    g_warning ("A set with the name '%s' has already been registered. Overriding.\n", name);

  val = g_new0 (GtkUndoSet, 1);
  *val = *set;
  val->description = g_strdup (set->description);
  g_hash_table_insert (undo->priv->method_hash, g_strdup (name), val);
}

/**
 * gtk_undo_add:
 * @undo: a #GtkUndo
 * @set_name: name of the set dealing with this data
 * @data: the data of the set
 * @description: a human-readable description of what this data does
 *
 * Add add an entry to the undo stack. The @set_name has to have
 * been registered before with gtk_undo_register_set.
 *
 * Return value: TRUE if the adding was successful, FALSE otherwise
 *               (e.g. if a set with the given @set_name was not registered,
 *               or the maximum allowed length of the undo stack is zero)
 *
 * Since: 2.20
 **/
gboolean
gtk_undo_add (GtkUndo *undo, const char *set_name, gpointer data, const gchar *description)
{
  GtkUndoSet *set;
  GtkUndoEntry *entry;

  g_return_val_if_fail (GTK_IS_UNDO (undo) && set_name, FALSE);

  if (undo->priv->max_length == 0)
    return FALSE;

  set = g_hash_table_lookup (undo->priv->method_hash, set_name);
  if (!set) {
    g_warning ("A set with the name '%s' has not been registered\n", set_name);
    return FALSE;
  }

  entry = g_new0 (GtkUndoEntry, 1);
  entry->description = g_strdup (description);
  entry->set = set;
  entry->data = data;

  if (!undo->priv->group_depth) {
    undo->priv->undo_stack = g_list_prepend (undo->priv->undo_stack, g_node_new (entry));
    change_len_undo (undo, 1);
    clear_redo (undo);
    if ((undo->priv->max_length != -1) && (undo->priv->undo_length > undo->priv->max_length))
      free_last_entry (undo);
    g_signal_emit (undo, signals[CHANGED], 0);
  }
  else {
    if (!(undo->priv->undo_stack && undo->priv->undo_stack->data)) {
      g_warning ("Could not add grouped entry.\n");
      return FALSE;
    }
    g_node_insert (get_node_level (undo->priv->undo_stack->data, undo->priv->group_depth), 0, g_node_new (entry));
  }

  return TRUE;
}

/**
 * gtk_undo_undo:
 * @undo: a #GtkUndo
 *
 * Undo the last operation.
 *
 * Return value: TRUE if undo was performed.
 *
 * Since: 2.20
 **/
gboolean
gtk_undo_undo (GtkUndo *undo)
{
  gboolean success = TRUE;

  g_return_val_if_fail (GTK_IS_UNDO (undo), FALSE);

  if (!gtk_undo_can_undo(undo)) {
    g_warning("Cannot undo.\n");
    return FALSE;
  }

  g_node_traverse (undo->priv->undo_stack->data, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1, traverse_undo_node, &success);
  if (success) {
    /* move data to redo stack */
    g_node_traverse (undo->priv->undo_stack->data, G_POST_ORDER, G_TRAVERSE_ALL, -1, traverse_reverse_children, NULL);
    undo->priv->redo_stack = g_list_prepend (undo->priv->redo_stack, undo->priv->undo_stack->data);
    change_len_redo (undo, 1);
    undo->priv->undo_stack = g_list_delete_link (undo->priv->undo_stack, undo->priv->undo_stack);
  }
  else {
    free_first_entry (undo, TRUE);
    g_warning("undo operation failed\n");
  }
  change_len_undo(undo, -1);
  g_signal_emit(undo, signals[CHANGED], 0);
  return TRUE;
}

/**
 * gtk_undo_redo:
 * @undo: a #GtkUndo
 *
 * Redo the last operation.
 *
 * Return value: TRUE if redo was successful.
 *
 * Since: 2.20
 **/
gboolean
gtk_undo_redo (GtkUndo *undo)
{
  gboolean success;

  g_return_val_if_fail (GTK_IS_UNDO (undo), FALSE);

  if (!gtk_undo_can_redo(undo)) {
    g_warning("Cannot redo.\n");
    return FALSE;
  }

  g_node_traverse (undo->priv->redo_stack->data, G_PRE_ORDER, G_TRAVERSE_LEAVES, -1, traverse_redo_node, &success);
  if (success) {
    /* move data back to undo stack */
    g_node_traverse (undo->priv->redo_stack->data, G_POST_ORDER, G_TRAVERSE_ALL, -1, traverse_reverse_children, NULL);
    undo->priv->undo_stack = g_list_prepend (undo->priv->undo_stack, undo->priv->redo_stack->data);
    change_len_undo (undo, 1);
    undo->priv->redo_stack = g_list_delete_link (undo->priv->redo_stack, undo->priv->redo_stack);
  }
  else {
    free_first_entry (undo, FALSE);
    g_warning ("redo operation failed\n");
  }
  change_len_redo (undo, -1);
  g_signal_emit(undo, signals[CHANGED], 0);
  return TRUE;
}

/**
 * gtk_undo_can_undo:
 * @undo: a #GtkUndo
 *
 * Check if there's an object on the undo stack that can be undone
 *
 * Return value: TRUE if an object can be undone, FALSE otherwise.
 *
 * Since: 2.20
 **/
gboolean
gtk_undo_can_undo (GtkUndo *undo)
{
  g_return_val_if_fail (GTK_IS_UNDO (undo), FALSE);
  return ((undo->priv->undo_stack != NULL) && (undo->priv->group_depth == 0));
}

/**
 * gtk_undo_can_redo:
 * @undo: a #GtkUndo
 *
 * Check if there's an object on the redo stack that can be undone
 *
 * Return value: TRUE if an object can be redone, FALSE otherwise.
 *
 * Since: 2.20
 **/
gboolean
gtk_undo_can_redo (GtkUndo *undo)
{
  g_return_val_if_fail (GTK_IS_UNDO (undo), FALSE);
  return ((undo->priv->redo_stack != NULL) && (undo->priv->group_depth == 0));
}

/**
 * gtk_undo_set_max_length:
 * @undo: a #GtkUndo
 * @max_length: the maximum length of the undo stack, or -1 for no maximum.
 *   The value passed in will be clamped to the range -1 - 65536.
 *
 * Sets the maximum allowed length of the undo stack. If
 * the current contents are longer than the given length, then they
 * will be truncated to fit.
 *
 * Since: 2.20
 **/
void
gtk_undo_set_max_length (GtkUndo *undo,
                         gint     max_length)
{
  g_return_if_fail (GTK_IS_UNDO (undo));

  if (undo->priv->group_depth != 0) {
    g_warning ("Currently in group add mode. Cannot modify maximum length.\n");
    return;
  }

  max_length = CLAMP (max_length, 0, GTK_UNDO_MAX_SIZE);

  if (max_length != undo->priv->max_length) {
    /* truncate list if necessary */
    if (max_length != -1) {
      while (undo->priv->undo_length > max_length)
        free_last_entry (undo);
    }
    undo->priv->max_length = max_length;
    g_object_notify (G_OBJECT (undo), "max-length");
  }
}

/**
 * gtk_undo_get_max_length:
 * @undo: a #GtkUndo
 *
 * Retrieves the maximum allowed length of the @undo
 * stack. See gtk_undo_set_max_length().
 *
 * Return value: the maximum length in the #GtkUndo stack,
 *               or -1 if there is no maximum.
 *
 * Since: 2.20
 */
gint
gtk_undo_get_max_length (GtkUndo *undo)
{
  g_return_val_if_fail (GTK_IS_UNDO (undo), -1);
  return undo->priv->max_length;
}

/**
 * gtk_undo_clear:
 * @undo: a #GtkUndo
 *
 * Clears the undo stack.
 *
 * Return value: TRUE if clearing was successful. Clearing can
 * fail e.g. if the stack is currently in group add mode.
 *
 * Since: 2.20
 */
gboolean
gtk_undo_clear (GtkUndo *undo)
{
  gboolean something_changed;

  g_return_val_if_fail (GTK_IS_UNDO (undo), FALSE);

  if (undo->priv->group_depth != 0) {
    return FALSE;
  }

  if(undo->priv->undo_stack || undo->priv->redo_stack)
    something_changed = TRUE;
  else
    something_changed = FALSE;

  clear_undo (undo);
  clear_redo (undo);

  if (something_changed)
    g_signal_emit (undo, signals[CHANGED], 0);
  return TRUE;
}

/**
 * gtk_undo_start_group:
 * @undo: a #GtkUndo
 * @description: a human readable description of what the group will undo
 *
 * Starts an undo group. The group must be ended with gtk_undo_end_group
 * before anything can be undone. Groups can be nested, however.
 *
 * Since: 2.20
 */
void
gtk_undo_start_group (GtkUndo *undo, const gchar *description)
{
  GtkUndoEntry *entry;

  entry = g_new0 (GtkUndoEntry, 1);
  entry->description = g_strdup(description);
  entry->set = NULL;
  entry->data = NULL;

  if (undo->priv->group_depth == 0) {
    // new toplevel entry
    undo->priv->undo_stack = g_list_prepend (undo->priv->undo_stack, g_node_new (entry));
  }
  else {
    // descend into tree at the top of the stack
    if (!(undo->priv->undo_stack && undo->priv->undo_stack->data)) {
      g_warning ("Could not start group.\n");
      return;
    }
    g_node_insert (get_node_level (undo->priv->undo_stack->data, undo->priv->group_depth), 0, g_node_new (entry));
  }

  undo->priv->group_depth++;
}

/**
 * gtk_undo_end_group:
 * @undo: a #GtkUndo
 *
 * Ends the innermost undo group.
 *
 * Since: 2.20
 */
void
gtk_undo_end_group (GtkUndo *undo)
{
  g_return_if_fail (undo->priv->group_depth > 0);
  undo->priv->group_depth--;
  if (undo->priv->group_depth == 0) {
    change_len_undo (undo, 1);
    clear_redo (undo);
    if ((undo->priv->max_length != -1) && (undo->priv->undo_length > undo->priv->max_length))
      free_last_entry (undo);
    g_signal_emit (undo, signals[CHANGED], 0);
  }
}

/**
 * gtk_undo_is_in_group:
 * @undo: a #GtkUndo
 *
 * Checks whether the stack is currently in group add mode (that is,
 * gtk_undo_start_group has been called more often than gtk_undo_end_group).
 *
 * Return value: TRUE if stack is in group add mode.
 *
 * Since: 2.20
 */
gboolean
gtk_undo_is_in_group (GtkUndo *undo)
{
  return (undo->priv->group_depth != 0);
}

/**
 * gtk_undo_get_undo_descriptions:
 * @undo: a #GtkUndo
 *
 * Get descriptions of the entries of the undo stack
 *
 * Return value: A GtkTreeModel of description strings.
 *
 * Since: 2.20
 */
GtkTreeStore*
gtk_undo_get_undo_descriptions (GtkUndo *undo)
{
  g_return_val_if_fail (GTK_IS_UNDO (undo), NULL);
  return get_descriptions_from_stack (undo->priv->undo_stack);
}

/**
 * gtk_undo_get_redo_descriptions:
 * @undo: a #GtkUndo
 *
 * Get descriptions of the entries of the redo stack
 *
 * Return value: A GtkTreeModel of description strings.
 *
 * Since: 2.20
 */
GtkTreeStore*
gtk_undo_get_redo_descriptions (GtkUndo *undo)
{
  g_return_val_if_fail (GTK_IS_UNDO (undo), NULL);
  return get_descriptions_from_stack (undo->priv->redo_stack);
}

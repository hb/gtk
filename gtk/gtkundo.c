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
  g_return_if_fail(entry->set);
  if (entry->set && entry->set->do_free && entry->data)
    entry->set->do_free(entry->data);

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

/* Clear the undo stack */
static void
clear_undo (GtkUndo *undo)
{
  GList *walk;

  change_len_undo (undo, -undo->priv->undo_length);

  for (walk = undo->priv->undo_stack; walk; walk = walk->next) {
    GtkUndoEntry *entry = walk->data;
    if (entry)
      free_entry (entry);
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
    GtkUndoEntry *entry = walk->data;
    if (entry)
      free_entry (entry);
  }
  g_list_free (undo->priv->redo_stack);
  undo->priv->redo_stack = NULL;
}

static gboolean
traverse_free_entry (GNode *node, gpointer data)
{
  free_entry ((GtkUndoEntry*)node->data);
  return FALSE;
}

/* Free last element of undo stack (usually because the stack
 * grew beyond its maximum length). */
static void
free_last_entry (GtkUndo *undo)
{
  GList *last;

  last = g_list_last (undo->priv->undo_stack);
  if (!last)
    return;

  g_node_traverse ((GNode*)last->data, G_POST_ORDER, G_TRAVERSE_ALL, -1, traverse_free_entry, NULL);
  undo->priv->undo_stack = g_list_delete_link (undo->priv->undo_stack, last);
  change_len_undo(undo, -1);
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
}

static void
gtk_undo_finalize (GObject *obj)
{
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
                                   0, GTK_UNDO_MAX_SIZE, -1,
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
 * Since: 2.20
 */
void
gtk_undo_clear (GtkUndo *undo)
{
  gboolean something_changed;

  g_return_if_fail (GTK_IS_UNDO (undo));

  if(undo->priv->undo_stack || undo->priv->redo_stack)
    something_changed = TRUE;
  else
    something_changed = FALSE;

  clear_undo (undo);
  clear_redo (undo);

  if (something_changed)
    g_signal_emit (undo, signals[CHANGED], 0);
}

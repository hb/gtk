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
};

G_DEFINE_TYPE (GtkUndo, gtk_undo, G_TYPE_OBJECT);


/* --------------------------------------------------------------------------------
 *
 */

static void
gtk_undo_init (GtkUndo *undo)
{
  GtkUndoPrivate *pv;

  pv = undo->priv = G_TYPE_INSTANCE_GET_PRIVATE (undo, GTK_TYPE_UNDO, GtkUndoPrivate);

  pv->max_length = -1;
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

  // TODO: this may need truncating the list

  undo->priv->max_length = max_length;
  g_object_notify (G_OBJECT (undo), "max-length");
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

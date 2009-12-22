/* gtkundo.h
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

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif


#ifndef __GTK_UNDO_H__
#define __GTK_UNDO_H__

#include <glib-object.h>

G_BEGIN_DECLS

/* Maximum size of text buffer, in bytes */
#define GTK_UNDO_MAX_SIZE        G_MAXINT

#define GTK_TYPE_UNDO            (gtk_undo_get_type ())
#define GTK_UNDO(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_UNDO, GtkUndo))
#define GTK_UNDO_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_UNDO, GtkUndoClass))
#define GTK_IS_UNDO(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_UNDO))
#define GTK_IS_UNDO_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_UNDO))
#define GTK_UNDO_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_UNDO, GtkUndoClass))

typedef struct _GtkUndo            GtkUndo;
typedef struct _GtkUndoClass       GtkUndoClass;
typedef struct _GtkUndoPrivate     GtkUndoPrivate;

struct _GtkUndo
{
  GObject parent_instance;

  /*< private >*/
  GtkUndoPrivate *priv;
};

struct _GtkUndoClass
{
  GObjectClass parent_class;

  /* Signals */

  void (*can_undo) (GtkUndo *undo,
                    gboolean can_undo);

  void (*can_redo) (GtkUndo *undo,
                    gboolean can_redo);

  void (*changed) (GtkUndo *undo);

  /* Virtual Methods */
  /* none, currently */

  /* Padding for future expansion */
  void (*_gtk_reserved0) (void);
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
};

GType    gtk_undo_get_type (void) G_GNUC_CONST;

GtkUndo* gtk_undo_new            (void);

void     gtk_undo_set_max_length (GtkUndo *buffer,
                                           gint max_length);

gint     gtk_undo_get_max_length (GtkUndo *undo);

G_END_DECLS

#endif /* __GTK_UNDO_H__ */

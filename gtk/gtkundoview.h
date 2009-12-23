/* gtkundoview.h
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

#ifndef __GTK_UNDO_VIEW_H__
#define __GTK_UNDO_VIEW_H__

#include <glib-object.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkundo.h>

G_BEGIN_DECLS

#define GTK_TYPE_UNDO_VIEW            (gtk_undo_view_get_type ())
#define GTK_UNDO_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_UNDO_VIEW, GtkUndoView))
#define GTK_UNDO_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_UNDO_VIEW, GtkUndoViewClass))
#define GTK_IS_UNDO_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_UNDO_VIEW))
#define GTK_IS_UNDO_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_UNDO_VIEW))
#define GTK_UNDO_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_UNDO_VIEW, GtkUndoViewClass))

typedef struct _GtkUndoView            GtkUndoView;
typedef struct _GtkUndoViewClass       GtkUndoViewClass;
typedef struct _GtkUndoViewPrivate     GtkUndoViewPrivate;


struct _GtkUndoView
{
  GtkVBox parent;

  /*< private >*/
  GtkUndoViewPrivate *priv;
};

struct _GtkUndoViewClass
{
  GtkVBoxClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved0) (void);
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
  void (*_gtk_reserved5) (void);
};

GType      gtk_undo_view_get_type (void) G_GNUC_CONST;

GtkWidget* gtk_undo_view_new      (GtkUndo *undo);

G_END_DECLS


#endif /* __GTK_UNDO_VIEW_H__ */

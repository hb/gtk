/* gtkundoview.c
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

#include "gtkundoview.h"
#include "gtkintl.h"
#include "gtkhbox.h"
#include "gtkbutton.h"
#include "gtkstock.h"
#include "gtkhpaned.h"
#include "gtktreestore.h"
#include "gtkcellrenderertext.h"
#include "gtktreeviewcolumn.h"
#include "gtktreeselection.h"
#include "gtkscrolledwindow.h"
#include "gtkinfobar.h"
#include "gtklabel.h"

/**
 * SECTION:gtkundoview
 * @title: GtkUndoView
 * @short_description: View for displaying a #GtkUndo stack
 *
 * The #GtkUndoView class implements a view for an #GtkUndo stack.
 *
 * TODO: Verbose description here
 *
 * Since: 2.20
 */

enum {
  PROP_0,
  PROP_UNDO,
};

struct _GtkUndoViewPrivate
{
  GtkUndo *undo;

  GtkWidget *undo_button;
  GtkWidget *redo_button;
  GtkWidget *clear_button;

  GtkWidget *undo_view;
  GtkWidget *redo_view;

  GtkWidget *info_bar;
};

G_DEFINE_TYPE (GtkUndoView, gtk_undo_view, GTK_TYPE_VBOX);


/* --------------------------------------------------------------------------------
 *
 */

static void
update_list_displays(GtkUndoView *view)
{
  GtkTreeStore *store;

  g_return_if_fail (GTK_IS_UNDO (view->priv->undo));

  store = gtk_undo_get_undo_descriptions (view->priv->undo);
  if (store) {
    gtk_tree_view_set_model (GTK_TREE_VIEW (view->priv->undo_view), GTK_TREE_MODEL (store));
    g_object_unref (store);
  }

  store = gtk_undo_get_redo_descriptions (view->priv->undo);
  if (store) {
    gtk_tree_view_set_model (GTK_TREE_VIEW (view->priv->redo_view), GTK_TREE_MODEL (store));
    g_object_unref (store);
  }

  gtk_widget_set_sensitive (view->priv->clear_button, gtk_undo_can_undo(view->priv->undo) || gtk_undo_can_redo(view->priv->undo));

  if (gtk_undo_is_in_group (view->priv->undo))
    gtk_widget_show (view->priv->info_bar);
  else
    gtk_widget_hide (view->priv->info_bar);
}

static GtkWidget*
create_list_display(GtkUndoView *view, gboolean undo_side)
{
  GtkWidget *vbox;
  GtkTreeStore *model;
  GtkWidget *treeview;
  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  GtkWidget *scrolledwin;
  gchar *title;
  GtkTreeSelection *selection;

  vbox = gtk_vbox_new (FALSE, 0);

  /* scrolled window */
  scrolledwin = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start (GTK_BOX (vbox), scrolledwin, TRUE, TRUE, 0);

  model = gtk_tree_store_new (1, G_TYPE_STRING);
  treeview = gtk_tree_view_new_with_model (GTK_TREE_MODEL(model));
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(treeview));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_NONE);
  renderer = gtk_cell_renderer_text_new ();
  if (undo_side)
    title = N_("Undo stack");
  else
    title = N_("Redo stack");
  column = gtk_tree_view_column_new_with_attributes (title, renderer, "text", 0, NULL);
  gtk_tree_view_column_set_clickable (column, FALSE);
  gtk_tree_view_append_column (GTK_TREE_VIEW (treeview), column);

  gtk_container_add (GTK_CONTAINER (scrolledwin), treeview);

  if(undo_side)
    view->priv->undo_view = treeview;
  else
    view->priv->redo_view = treeview;

  return vbox;
}

static void
on_info_bar_show_signal (GtkWidget *info_bar, gpointer data)
{
  GtkUndoView *view;
  view = data;
  if (view->priv->undo && gtk_undo_is_in_group (view->priv->undo))
    gtk_widget_show (info_bar);
  else
    gtk_widget_hide (info_bar);
}

/* --------------------------------------------------------------------------------
 *
 */

static void
gtk_undo_view_init (GtkUndoView *view)
{
  GtkUndoViewPrivate *pv;
  GtkWidget *hbox;
  GtkWidget *paned;
  GtkWidget *list_display;

  pv = view->priv = G_TYPE_INSTANCE_GET_PRIVATE (view, GTK_TYPE_UNDO_VIEW, GtkUndoViewPrivate);

  pv->undo = NULL;
  pv->undo_button = NULL;
  pv->redo_button = NULL;
  pv->clear_button = NULL;
  pv->undo_view = NULL;
  pv->redo_view = NULL;

  /* set up widget */
  hbox = gtk_hbox_new (FALSE, 4);

  pv->undo_button = gtk_button_new_from_stock (GTK_STOCK_UNDO);
  gtk_widget_set_sensitive (pv->undo_button, FALSE);
  gtk_box_pack_start (GTK_BOX (hbox), pv->undo_button, FALSE, FALSE, 0);

  pv->redo_button = gtk_button_new_from_stock (GTK_STOCK_REDO);
  gtk_widget_set_sensitive (pv->redo_button, FALSE);
  gtk_box_pack_start (GTK_BOX (hbox), pv->redo_button, FALSE, FALSE, 0);

  pv->clear_button = gtk_button_new_from_stock (GTK_STOCK_CLEAR);
  gtk_widget_set_sensitive (pv->clear_button, FALSE);
  gtk_box_pack_start (GTK_BOX (hbox), pv->clear_button, FALSE, FALSE, 0);

  gtk_box_pack_start (GTK_BOX (view), hbox, FALSE, FALSE, 0);
  gtk_widget_show_all (hbox);

  /* paned */
  paned = gtk_hpaned_new ();
  // TODO: adjust to requisition size
  gtk_paned_set_position (GTK_PANED (paned), 300);

  /* first pane: undo list */
  list_display = create_list_display (view, TRUE);
  gtk_paned_add1 (GTK_PANED (paned), list_display);

  /* second pane: redo list */
  list_display = create_list_display (view, FALSE);
  gtk_paned_add2 (GTK_PANED (paned), list_display);

  gtk_box_pack_start (GTK_BOX (view), paned, TRUE, TRUE, 0);

  /* info bar */
  pv->info_bar = gtk_info_bar_new ();
  g_signal_connect_after (G_OBJECT (pv->info_bar), "show", G_CALLBACK (on_info_bar_show_signal), view);
  gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (pv->info_bar))), gtk_label_new (N_("Currently in group add mode")));
  gtk_box_pack_start (GTK_BOX (view), pv->info_bar, FALSE, FALSE, 0);

  gtk_widget_show_all (GTK_WIDGET (view));
}

static void
gtk_undo_view_set_property (GObject      *obj,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
  GtkUndoView *view = GTK_UNDO_VIEW (obj);

  switch (prop_id)
    {
    case PROP_UNDO:
      /* construct-only property */
      if (view->priv->undo)
        g_object_unref (view->priv->undo);
      view->priv->undo = g_value_get_pointer (value);
      if (view->priv->undo) {
        g_object_ref (view->priv->undo);

        g_signal_connect_swapped (G_OBJECT (view->priv->undo_button), "clicked", G_CALLBACK (gtk_undo_undo), view->priv->undo);
        g_signal_connect_swapped (G_OBJECT (view->priv->undo), "can-undo", G_CALLBACK (gtk_widget_set_sensitive), view->priv->undo_button);
        gtk_widget_set_sensitive (view->priv->undo_button, gtk_undo_can_undo (view->priv->undo));

        g_signal_connect_swapped (G_OBJECT (view->priv->redo_button), "clicked", G_CALLBACK (gtk_undo_redo), view->priv->undo);
        g_signal_connect_swapped (G_OBJECT (view->priv->undo), "can-redo", G_CALLBACK (gtk_widget_set_sensitive), view->priv->redo_button);
        gtk_widget_set_sensitive (view->priv->redo_button, gtk_undo_can_redo (view->priv->undo));

        g_signal_connect_swapped (G_OBJECT (view->priv->clear_button), "clicked", G_CALLBACK (gtk_undo_clear), view->priv->undo);
        gtk_widget_set_sensitive (view->priv->clear_button, gtk_undo_can_undo (view->priv->undo) || gtk_undo_can_redo (view->priv->undo));

        g_signal_connect_swapped (G_OBJECT (view->priv->undo), "changed", G_CALLBACK (update_list_displays), view);

        update_list_displays(view);
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
gtk_undo_view_get_property (GObject    *obj,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
  GtkUndoView *view = GTK_UNDO_VIEW (obj);

  switch (prop_id)
    {
    case PROP_UNDO:
      g_value_set_pointer (value, view->priv->undo);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
gtk_undo_view_finalize (GObject *obj)
{
  G_OBJECT_CLASS (gtk_undo_view_parent_class)->finalize (obj);
}

static void
gtk_undo_view_dispose (GObject *obj)
{
  GtkUndoView *view = GTK_UNDO_VIEW (obj);

  if (view->priv->undo) {
    g_object_unref (view->priv->undo);
    view->priv->undo = NULL;
  }

  G_OBJECT_CLASS (gtk_undo_view_parent_class)->dispose (obj);
}

static void
gtk_undo_view_class_init (GtkUndoViewClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_undo_view_set_property;
  gobject_class->get_property = gtk_undo_view_get_property;
  gobject_class->dispose = gtk_undo_view_dispose;
  gobject_class->finalize = gtk_undo_view_finalize;

  g_type_class_add_private (gobject_class, sizeof (GtkUndoViewPrivate));

  /**
   * GtkUndoView:undo:
   *
   * The #GtkUndo class.
   *
   * Since: 2.20
   */
  g_object_class_install_property (gobject_class,
                                   PROP_UNDO,
                                   g_param_spec_pointer ("undo",
                                                     P_("undo stack"),
                                                     P_("The undo stack for the view."),
                                   G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/* --------------------------------------------------------------------------------
 *
 */

/**
 * gtk_undo_view_new:
 * @undo: a #GtkUndo
 *
 * Create a new GtkUndoView object.
 *
 * Return value: A new GtkUndoView object.
 *
 * Since: 2.20
 **/
GtkWidget*
gtk_undo_view_new (GtkUndo *undo)
{
  return g_object_new(GTK_TYPE_UNDO_VIEW, "undo", undo, NULL);
}

/* testundo.c: Test application undo code
 *
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

#include <gtk/gtk.h>

#define UNDO_SET_NAME "myUndoSet"
#define UNDO_DEFAULT_MAX_LENGTH 10

typedef struct _UndoDataTst1 UndoDataTst1;
struct _UndoDataTst1 {
};

GtkWidget *g_ok_fail_checkbutton;


static void
do_free (gpointer data)
{
  g_print("do free called\n");
  g_free(data);
}

static gboolean
do_undo (gpointer data)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (g_ok_fail_checkbutton))) {
    g_print("FAIL do undo called\n");
    return FALSE;
  }
  else {
    g_print("OK   do undo called\n");
    return TRUE;
  }
}

static gboolean
do_redo (gpointer data)
{
  if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (g_ok_fail_checkbutton))) {
    g_print("FAIL do redo called\n");
    return FALSE;
  }
  else {
    g_print("OK   do redo called\n");
    return TRUE;
  }
}

static void
add (GtkUndo *undo, const gchar *msg)
{
  UndoDataTst1 *dat;

  dat = g_new0 (UndoDataTst1, 1);
  gtk_undo_add (undo, UNDO_SET_NAME, dat, msg);
}

static void
add_with_description_cb (GtkButton *button, gpointer data)
{
  static guint counter = 0;
  gchar *msg;

  msg = g_strdup_printf ("entry description: %d", counter++);
  add ((GtkUndo*)data, msg);
  g_free (msg);
}

static void
add_without_description_cb (GtkButton *button, gpointer data)
{
  add ((GtkUndo*)data, NULL);
}

static void
start_group_with_description_cb (GtkButton *button, gpointer data)
{
  static guint counter = 0;
  gchar *msg;

  msg = g_strdup_printf ("group description: %d", counter++);
  gtk_undo_start_group ((GtkUndo*)data, msg);
  g_free (msg);
}

static void
start_group_without_description_cb (GtkButton *button, gpointer data)
{
  gtk_undo_start_group ((GtkUndo*)data, NULL);
}

void
max_size_value_changed_cb (GtkSpinButton *spinner, gpointer data)
{
  gtk_undo_set_max_length ((GtkUndo*)data, gtk_spin_button_get_value_as_int (spinner));
}

static GtkWidget*
create_main_window (GtkUndo *undo)
{
  GtkWidget *win;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *view;
  GtkWidget *button;
  GtkWidget *sep;
  GtkWidget *spinner;
  GtkActionGroup *action_group;
  GtkUIManager *ui_manager;

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (win, 600, 600);
  g_signal_connect (win, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_window_set_title (GTK_WINDOW (win), "Undo Test");

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (win), vbox);

  /* menu and toolbar */
  action_group = gtk_action_group_new ("actiongroupname");
  gtk_action_group_add_action (action_group, gtk_action_new ("EditMenu", "Edit", NULL, NULL));
  gtk_action_group_add_action (action_group, gtk_undo_get_undo_action (undo));
  gtk_action_group_add_action (action_group, gtk_undo_get_redo_action (undo));
  ui_manager = gtk_ui_manager_new ();
  gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);
  gtk_ui_manager_add_ui_from_string (ui_manager,
      "<ui>"
      "  <menubar name='MenuBar'>"
      "    <menu action='EditMenu'>"
      "      <menuitem action='" GTK_UNDO_UNDO_ACTION_NAME "'/>"
      "      <menuitem action='" GTK_UNDO_REDO_ACTION_NAME "'/>"
      "    </menu>"
      "  </menubar>"
      "  <toolbar name='ToolBar'>"
      "    <toolitem action='" GTK_UNDO_UNDO_ACTION_NAME "'/>"
      "    <toolitem action='" GTK_UNDO_REDO_ACTION_NAME "'/>"
      "  </toolbar>"
      "</ui>", -1, NULL);
  gtk_box_pack_start (GTK_BOX (vbox), gtk_ui_manager_get_widget (ui_manager, "/MenuBar"), FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), gtk_ui_manager_get_widget (ui_manager, "/ToolBar"), FALSE, FALSE, 0);

  /* buttons */
  button = gtk_button_new_with_label ("add with description");
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (add_with_description_cb), undo);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("add without description");
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (add_without_description_cb), undo);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("start group with description");
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (start_group_with_description_cb), undo);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("start group without description");
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (start_group_without_description_cb), undo);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label ("end group");
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (gtk_undo_end_group), undo);
  gtk_box_pack_start (GTK_BOX (vbox), button, FALSE, FALSE, 0);

  /* checkbox */
  g_ok_fail_checkbutton = gtk_check_button_new_with_label ("make undo/redo fail");
  gtk_box_pack_start (GTK_BOX (vbox), g_ok_fail_checkbutton, FALSE, FALSE, 0);

  /* spinner */
  hbox = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), gtk_label_new ("Maximum stack size"), FALSE, FALSE, 0);
  spinner = gtk_spin_button_new_with_range (-1., 100000., 1.);
  g_signal_connect (G_OBJECT (spinner), "value-changed", G_CALLBACK (max_size_value_changed_cb), undo);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (spinner), UNDO_DEFAULT_MAX_LENGTH);
  gtk_box_pack_start (GTK_BOX (hbox), spinner, FALSE, FALSE, 0);

  /* separator */
  sep = gtk_hseparator_new();
  gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 5);

  /* undo view */
  view = gtk_undo_view_new (undo);
  gtk_box_pack_start (GTK_BOX (vbox), view, TRUE, TRUE, 0);

  gtk_widget_show_all (win);
  g_object_unref (undo);
  return win;
}

int
main (int argc, char *argv[])
{
  GtkUndo *undo;
  GtkUndoSet set;

  gtk_init (&argc, &argv);

  undo = gtk_undo_new ();
  gtk_undo_set_max_length (undo, UNDO_DEFAULT_MAX_LENGTH);

  set.do_undo = do_undo;
  set.do_redo = do_redo;
  set.do_free = do_free;
  set.description = "set description";
  gtk_undo_register_set (undo, UNDO_SET_NAME, &set);

  create_main_window (undo);

  gtk_main ();
  return 0;
}

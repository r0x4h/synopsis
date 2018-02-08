#include <gtk/gtk.h>
#include <sqlite3.h>
#include <string.h>
#include "nopslib.h"
#include "util.h"

GtkWidget *window;
GtkToggleButton *search_toggle;
GtkSearchBar *search_bar;
GtkSearchEntry *search_entry;
GtkTreeView *tree_view;
GtkListStore *list_store;
GtkTreeModelFilter *filter_model;

void show_dialog (char *message) {
  GtkWidget *dialog = gtk_message_dialog_new (GTK_WINDOW(window),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_ERROR,
                        GTK_BUTTONS_CLOSE,
                        message);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static int populate_row_callback (void *data, int colCount, char *values[], char *colNames[]) {
  GtkTreeIter row_iter;
  gtk_list_store_append (list_store, &row_iter);
  gtk_list_store_set (list_store, &row_iter, 0, values[0], // TitleID
                                             1, values[1], // Region
                                             2, values[2], // Name
                                             3, values[8], // Size
                                             4, values[3], // Url
                                             -1);
  return 0;
}

void populate_grid () {
  gtk_list_store_clear (list_store);
  syn_get_data (populate_row_callback);
}

static gboolean search_filter_func (GtkTreeModel *model, GtkTreeIter *row, gpointer data) {
  gboolean visible = FALSE;

  // get row
  gchar *titleId, *region, *name;
  gtk_tree_model_get (model, row, 0, &titleId, 1, &region, 2, &name, -1);

  // does row match?
  const gchar *search_text = gtk_entry_get_text (GTK_ENTRY (search_entry));
  if (titleId && strstr_case_insensitive (titleId, search_text) != NULL) {
    visible = TRUE;
  } else if (name && strstr_case_insensitive (name, search_text) != NULL) {
    visible = TRUE;
  }

  // cleanup
  g_free (titleId);
  g_free (region);
  g_free (name);

  return visible;
}

static void activate (GtkApplication* app, gpointer user_data) {
  GtkBuilder *builder = gtk_builder_new_from_file ("synopsis.ui");
  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  search_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "search_toggle"));
  search_bar = GTK_SEARCH_BAR (gtk_builder_get_object (builder, "search_bar"));
  search_entry = GTK_SEARCH_ENTRY (gtk_builder_get_object (builder, "search_entry"));
  tree_view = GTK_TREE_VIEW (gtk_builder_get_object (builder, "tree_view"));
  list_store = GTK_LIST_STORE (gtk_builder_get_object (builder, "list_store"));
  filter_model = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL (list_store), NULL));

  gtk_builder_connect_signals (builder, NULL);
  g_object_unref (builder);
  gtk_widget_show_all (window);

  populate_grid ();
  gtk_tree_model_filter_set_visible_func (filter_model, search_filter_func, NULL, NULL);
  gtk_tree_view_set_model (tree_view, GTK_TREE_MODEL (filter_model));

  gtk_main ();
}

int main (int argc, char *argv[]) {
  GtkApplication *app = gtk_application_new ("com.github.r0x4h.synopsis", G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
  int status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  return status;
}

void on_window_main_destroy () {
  gtk_main_quit ();
}

void on_search_button_toggled () {
  if (gtk_toggle_button_get_active (search_toggle)) {
    gtk_search_bar_set_search_mode (search_bar, TRUE);
  } else {
    gtk_search_bar_set_search_mode (search_bar, FALSE);
  }
}

void on_search_changed (GtkSearchEntry *entry, gpointer user_data) {
  gtk_tree_model_filter_refilter (filter_model);
}

void on_refresh_button_clicked (GtkButton *button, gpointer user_data) {
  int res = syn_refresh_db ();
  if (res == SYN_OK) {
    populate_grid ();
  }
}

void  on_row_doubleclicked (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *col, gpointer userdata) {
  GtkTreeModel *model = gtk_tree_view_get_model (treeview);
  GtkTreeIter rowIter;

  if (gtk_tree_model_get_iter (model, &rowIter, path)) {
    gchar *url;
    gtk_tree_model_get (model, &rowIter, 4, &url, -1);
    g_print ("URL: %s\n", url);
    g_free (url);
  }
}

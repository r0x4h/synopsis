#include <gtk/gtk.h>
#include <sqlite3.h>
#include <string.h>
#include "nopslib.h"
#include "util.h"

GtkWidget *window;
GtkRevealer *in_app_notification_revealer;
GtkLabel *in_app_notification_label;
GtkToggleButton *search_toggle;
GtkSearchBar *search_bar;
GtkSearchEntry *search_entry;
GtkTreeView *tree_view;
GtkListStore *list_store;
GtkTreeModelFilter *filter_model;

enum columns {
  COL_DOWNLOAD,
  COL_TITLE_ID,
  COL_REGION,
  COL_NAME,
  COL_SIZE,
  COL_URL,
  N_COLUMNS
};

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
  gtk_list_store_set (list_store, &row_iter, COL_DOWNLOAD, FALSE,
                                             COL_TITLE_ID, values[0],
                                             COL_REGION, values[1],
                                             COL_NAME, values[2],
                                             COL_SIZE, values[8],
                                             COL_URL, values[3],
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
  gtk_tree_model_get (model, row, 1, &titleId, 2, &region, 3, &name, -1);

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
  in_app_notification_revealer = GTK_REVEALER (gtk_builder_get_object (builder, "in_app_notification_revealer"));
  in_app_notification_label = GTK_LABEL (gtk_builder_get_object (builder, "in_app_notification_label"));
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

void show_queued_notification (char *titleId, char *name) {
  char *label = g_markup_printf_escaped ("Queued '%s - %s'", titleId, name);
  gtk_label_set_markup (GTK_LABEL (in_app_notification_label), label);
  gtk_revealer_set_reveal_child (GTK_REVEALER (in_app_notification_revealer), TRUE);
}

void add_to_download_queue (char *titleId, char *name, char *url) {
  show_queued_notification (titleId, name);
  // add to queue
}

void download_toggled (GtkCellRendererToggle *cell, gchar *path_string, gpointer user_data) {
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreePath *path = gtk_tree_path_new_from_string (path_string);
  gboolean checked = gtk_cell_renderer_toggle_get_active (cell);

  GtkTreeIter filterIter;
  if (gtk_tree_model_get_iter (model, &filterIter, path)) {
    GtkTreeIter listStoreIter;
    gtk_tree_model_filter_convert_iter_to_child_iter (filter_model, &listStoreIter, &filterIter);
    gtk_list_store_set (list_store, &listStoreIter, COL_DOWNLOAD, !checked, -1);


    // show notification
    gchar *titleId, *name, *url;
    gtk_tree_model_get (model, &filterIter, COL_TITLE_ID, &titleId, COL_NAME, &name, COL_URL, &url, -1);
    add_to_download_queue (titleId, name, url);
    g_free (titleId);
    g_free (name);
    g_free (url);

    // gtk_tree_model_get (model, &rowIter, COL_TITLE_ID, &titleId, COL_LINK, &link, -1);
    // g_print ("URL: %s\n", link);
    // download_file (link, titleId);
  }
}

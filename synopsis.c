#include <gtk/gtk.h>
#include <sqlite3.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include "nopslib.h"
#include "util.h"

#define NOTIFICATION_TIMEOUT 3 /*s */

GtkWidget *window;
GtkRevealer *in_app_notification_revealer;
GtkLabel *in_app_notification_label;
GtkToggleButton *search_toggle;
GtkSearchBar *search_bar;
GtkSearchEntry *search_entry;
GtkTreeView *tree_view;
GtkListStore *list_store;
GtkTreeModelFilter *filter_model;
GtkBox *downloads_box;
GtkWidget *download_progress_icon;

gdouble progress = 0.65;

typedef struct {
  gchar *titleId;
  gchar *name;
  gchar *url;
  gchar *zRIF;
} Download;

enum columns {
  COL_DOWNLOAD,
  COL_TITLE_ID,
  COL_REGION,
  COL_NAME,
  COL_SIZE,
  COL_URL,
  COL_ZRIF,
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
                                             COL_ZRIF, values[4],
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
  downloads_box = GTK_BOX (gtk_builder_get_object (builder, "downloads_box"));
  download_progress_icon = GTK_WIDGET (gtk_builder_get_object (builder, "download_progress_icon"));

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

static gboolean on_in_app_notification_undo_timeout (GtkWidget *window) {
  gtk_revealer_set_reveal_child (GTK_REVEALER (in_app_notification_revealer), FALSE);
  return G_SOURCE_REMOVE;
}

void show_queued_notification (char *titleId, char *name) {
  char *label = g_markup_printf_escaped ("Queued '%s'", name);
  gtk_label_set_markup (GTK_LABEL (in_app_notification_label), label);
  gtk_revealer_set_reveal_child (GTK_REVEALER (in_app_notification_revealer), TRUE);

  g_timeout_add_seconds (NOTIFICATION_TIMEOUT, (GSourceFunc) on_in_app_notification_undo_timeout, window);
}

void add_to_download_queue (char *titleId, char *name, char *url) {
  show_queued_notification (titleId, name);

  // add entry to popup
  GtkWidget *label = gtk_label_new (name);
  gtk_box_pack_start (downloads_box, label, FALSE, TRUE, 0);
  gtk_widget_show (label);
}

static int progress_callback (void* ptr, double downloadSize, double downloaded, double totalToUpload, double uploaded) {
  //progress = downloaded / downloadSize;
  //gtk_widget_queue_draw (download_progress_icon);
  return 0;
}

void *download_in_separate_thread (void *param) {
  Download *info = param;
  download_file (info->url, info->titleId, progress_callback);
  unpack_file (info->titleId, info->zRIF);
  remove (info->titleId);

  // cleanup
  g_free (info->titleId);
  g_free (info->name);
  g_free (info->url);
  g_free (info->zRIF);
  free (info);

  return 0;
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
    gchar *titleId, *name, *url, *zRIF;
    gtk_tree_model_get (model, &filterIter, COL_TITLE_ID, &titleId, COL_NAME, &name, COL_URL, &url, COL_ZRIF, &zRIF, -1);
    add_to_download_queue (titleId, name, url);

    // download
    Download *downloadInfo = malloc (sizeof *downloadInfo);
    downloadInfo->titleId = titleId;
    downloadInfo->name = name;
    downloadInfo->url = url;
    downloadInfo->zRIF = zRIF;
    pthread_t tid;
    pthread_create (&tid, NULL, download_in_separate_thread, downloadInfo);
  }
}

void notification_dismissed (GtkButton *button, gpointer user_data) {
  gtk_revealer_set_reveal_child (GTK_REVEALER (in_app_notification_revealer), FALSE);
}

gboolean on_operations_icon_draw (GtkWidget *widget, cairo_t *cr, gpointer user_data) {
  GtkStyleContext *styleContext = gtk_widget_get_style_context (download_progress_icon);

  GdkRGBA background;
  GdkRGBA foreground;
  gtk_style_context_get_color (styleContext, gtk_widget_get_state_flags (widget), &foreground);
  background = foreground;
  background.alpha *= 0.3;

  guint width = gtk_widget_get_allocated_width (widget);
  guint height = gtk_widget_get_allocated_height (widget);

  // draw background circle
  gdk_cairo_set_source_rgba (cr, &background);
  cairo_arc (cr, width / 2.0, height / 2.0, MIN (width, height) / 2.0, 0, 2 * G_PI);
  cairo_fill (cr);

  cairo_move_to (cr, width / 2.0, height / 2.0);

  // draw foreground arc
  if (progress > 0) {
    gdk_cairo_set_source_rgba (cr, &foreground);
    cairo_arc (cr, width / 2.0, height / 2.0, MIN (width, height) / 2.0, -G_PI / 2.0, progress * 2 * G_PI - G_PI / 2.0);
    cairo_fill (cr);
  }

  return FALSE;
}

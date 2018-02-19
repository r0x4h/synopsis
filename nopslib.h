#define SYN_OK 0
#define SYN_ERROR -1

void syn_get_data (void *callback);
int download_file (char *url, char *filename, void *progress_callback);
int syn_refresh_db ();

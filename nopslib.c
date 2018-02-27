#include <stdio.h>
#include <curl/curl.h>
#include <sqlite3.h>
#include <string.h>
#include <stdlib.h>
#include "nopslib.h"

#define NO_OF_FILES 1
const char *baseUrl = "https://nopaystation.com/tsv/";
const char *fileNames[NO_OF_FILES] = {
  "PSV_GAMES.tsv"
};

void syn_get_data (void *callback) {
  sqlite3 *db = NULL;
  int rc = sqlite3_open ("synopsis.db", &db);
  if (rc != SQLITE_OK) {
    puts ("could not open database");
    return;
  }

  // get data
  char *err_msg = NULL;
  char *sql = "select * from Content";
  rc = sqlite3_exec (db, sql, callback, NULL, &err_msg);
  if (rc != SQLITE_OK) {
    puts (err_msg);
    sqlite3_free (err_msg);
  }

  sqlite3_close(db);
}

int unpack_file (char *filename, char *zRIF) {
  if (strcmp ("MISSING", zRIF) != 0 || strcmp ("NOT REQUIRED", zRIF) != 0) {
    zRIF = "";
  }

  char commandBuf[200];
  snprintf(commandBuf, sizeof commandBuf, "%s %s %s", "./pkg2zip", filename, zRIF);

  FILE *output = popen (commandBuf, "r");
  if (output == NULL) {
    return 1;
  }

  if (pclose (output) != 0) {
    return 1;
  }

  return 0;
}

int download_file (char *url, char *filename, void *progress_callback, void *progress_data) {
  CURL *curl = curl_easy_init();
  if (!curl) {
    curl_easy_cleanup (curl);
    return SYN_ERROR;
  }

  FILE *file = fopen (filename, "wb");
  curl_easy_setopt (curl, CURLOPT_URL, url);
  curl_easy_setopt (curl, CURLOPT_WRITEDATA, file);

  if (progress_callback != NULL) {
    curl_easy_setopt (curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt (curl, CURLOPT_PROGRESSFUNCTION, progress_callback);
    curl_easy_setopt (curl, CURLOPT_PROGRESSDATA, progress_data);
  }

  CURLcode res = curl_easy_perform (curl);

  fclose (file);
  curl_easy_cleanup (curl);

  return res == CURLE_OK ? SYN_OK : SYN_ERROR;
}

int download_files () {
  for( int i = 0; i < NO_OF_FILES; i++) {
    // work out full path
    char *fileName = (char *)fileNames[i];
    char fullUrl[80];
    strcpy(fullUrl, baseUrl);
    strcat(fullUrl, fileName);

    int res = download_file (fullUrl, fileName, NULL, NULL);
    if (res != SYN_OK) {
      return SYN_ERROR;
    }
  }

  return SYN_OK;
}

sqlite3 *create_new_db () {
  sqlite3 *db = NULL;
  int rc = sqlite3_open ("synopsis.db", &db);
  if (rc != SQLITE_OK) {
    return db;
  }

  // Title ID	Region	Name	PKG direct link	zRIF	Content ID	Last Modification Date	Original Name	File Size	SHA256
  char *err_msg = NULL;
  char *sql = "CREATE TABLE Content (TitleId TEXT PRIMARY KEY, Region TEXT, Name TEXT, Link TEXT, zRIF TEXT, ContentId TEXT, LastModified TEXT, OriginalName TEXT, Size INT, SHA256 TEXT)";
  rc = sqlite3_exec (db, sql, 0, 0, &err_msg);
  if (rc != SQLITE_OK ) {
    sqlite3_free (err_msg);
    return db;
  }

  return db;
}

int populate_db (sqlite3 *db) {
  // open data file
  FILE *file = fopen ("PSV_GAMES.tsv", "r");
  if (file == NULL) {
    return SYN_ERROR;
  }

  // prepare insert statement
  sqlite3_stmt *stmt;
  char *sql = "INSERT INTO Content VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
  sqlite3_prepare_v2 (db, sql, -1, &stmt, 0);

  // read and insert
  size_t len = 0;
  char *line = NULL;
  char *delim = "\t";
  int lineNo = 0;

  while (getline (&line, &len, file) != -1) {
    lineNo++;

    // skip header
    if (lineNo == 1) {
      continue;
    }

    sqlite3_reset (stmt);

    int colNo = 1;
    char *colData;
    while ((colData = strsep (&line, delim)) != NULL) {
      sqlite3_bind_text (stmt, colNo, colData, -1, 0);
      colNo++;
    }

    sqlite3_step (stmt);
  }

  sqlite3_finalize (stmt);
  fclose (file);
  return SYN_OK;
}

int syn_refresh_db () {
  // download files
  int res = download_files ();
  if (res != SYN_OK) {
    return SYN_ERROR;
  }

  // delete existing db
  remove ("synopsis.db");

  // create a new db
  sqlite3 *db = create_new_db ();
  if (db == NULL) {
    return SYN_ERROR;
  }

  // populate db
  res = populate_db (db);
  if (res != SYN_OK) {
    return SYN_ERROR;
  }

  sqlite3_close (db);
  return SYN_OK;
}

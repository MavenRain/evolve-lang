#define _POSIX_C_SOURCE 200809L
#include "front/check.h"
#include "front/front.h"
#include "sql.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Exercise full checked protocols, including the no-output guarantee on a
   name refusal. Each case owns an empty temporary output directory. */
static int protocol_case(const char *label, const char *source, const char *code,
                         const char *filename, const char *contains) {
  static unsigned int serial = 0;
  char directory[128];
  char path[512];
  Arena arena;
  Diag diag;
  Front front;
  Machine machine;
  Report report;
  memset(&report, 0, sizeof report);
  report.protocol = "probe";
  arena_init(&arena, (size_t)1 << 28);
  diag_init(&diag);
  snprintf(directory, sizeof directory, "/tmp/evolve-sql-%ld-%u", (long)getpid(), serial++);
  if (mkdir(directory, 0700) != 0) {
    fprintf(stderr, "FAIL sql protocol %s: temporary directory\n", label);
    arena_release(&arena);
    return 0;
  }
  int ok = front_load(&arena, label, source, strlen(source), &front, &diag) &&
           check_program_report(&arena, &front.decls, &machine, &diag, &report) && report.found;
  if (ok) {
    int status = sql_write(&arena, &diag, &front.decls, &report, source, strlen(source), directory);
    ok = code == NULL ? status == 0 : status == 1 && diag.set && strcmp(diag.code, code) == 0;
  }
  if (ok && filename != NULL) {
    snprintf(path, sizeof path, "%s/%s", directory, filename);
    FILE *file = fopen(path, "rb");
    char text[16384];
    size_t len = file == NULL ? 0 : fread(text, 1, sizeof text - 1, file);
    text[len] = '\0';
    ok = file != NULL && (contains == NULL || strstr(text, contains) != NULL);
    if (file != NULL && fclose(file) != 0) ok = 0;
  }
  DIR *dir = opendir(directory);
  size_t files = 0;
  if (dir == NULL) ok = 0;
  else {
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
      files++;
      snprintf(path, sizeof path, "%s/%s", directory, entry->d_name);
      if (remove(path) != 0) ok = 0;
    }
    if (closedir(dir) != 0) ok = 0;
  }
  if (code != NULL && files != 0) ok = 0;
  if (rmdir(directory) != 0) ok = 0;
  if (!ok) {
    fprintf(stderr, "FAIL sql protocol %s\n", label);
    if (diag.set) diag_print(&diag, stderr);
  }
  arena_release(&arena);
  return ok;
}

int main(void) {
  char name[65], source[8192];
  memset(name, 'a', 64);
  name[64] = '\0';
  snprintf(source, sizeof source, "table users (id : bigint key)\n\nprotocol probe where\n  addColumn users %s text\n", name);
  int ok = protocol_case("long column", source, "SQL_NAME_LONG", NULL, NULL);
  snprintf(source, sizeof source, "table %s (id : bigint key)\n\nprotocol probe where\n  createIndexConcurrently %s id\n", name, name);
  ok &= protocol_case("long table", source, "SQL_NAME_LONG", NULL, NULL);
  snprintf(source, sizeof source, "table users (%s : bigint key, name : text)\n\nprotocol probe where\n  addColumn users full text\n  backfill users full ← name batch 1\n", name);
  ok &= protocol_case("long backfill key", source, "SQL_NAME_LONG", NULL, NULL);
  name[63] = '\0';
  snprintf(source, sizeof source, "table users (id : bigint key)\n\nprotocol probe where\n  addColumn users %s text\n", name);
  ok &= protocol_case("63-byte column", source, NULL, "01-addColumn.sql", name);
  name[63] = 'a';
  snprintf(source, sizeof source, "table users (id : bigint key)\n\napp %s ()\n\nprotocol probe where\n  deploy %s\n", name, name);
  ok &= protocol_case("long registry value", source, NULL, "01-deploy.sql", name);
  strcpy(source, "table users (id : bigint key)\n\nprotocol probe where\n");
  for (size_t i = 0; i < 100; i++) strcat(source, "  createIndexConcurrently users id\n");
  ok &= protocol_case("wide setup filename", source, NULL, "00-setup.sql", NULL);
  ok &= protocol_case("wide stage filename", source, NULL, "100-createIndexConcurrently.sql", "evolve_probe_100_idx");
  ok &= protocol_case("exact setup tenant", source, NULL, "00-setup.sql", "nspname::text = current_setting('evolve.tenant')");
  ok &= protocol_case("exact stage tenant", source, NULL, "001-createIndexConcurrently.sql", "EVOLVE_TENANT %");
  ok &= protocol_case("index definition", source, NULL, "001-createIndexConcurrently.sql", "pg_get_indexdef(i.indexrelid, 1, false) = quote_ident('id')");
  ok &= protocol_case("tenant table resolution", source, NULL, "001-createIndexConcurrently.sql", "IS DISTINCT FROM evolve_table");
  ok &= protocol_case("builtin precedence", source, NULL, "00-setup.sql", "SET search_path TO pg_catalog, :\"tenant\", pg_temp;");
  strcpy(source, "table users (id : bigint key, name : text)\n\nprotocol probe where\n  addColumn users full text\n  backfill users full ← name batch 1\n");
  ok &= protocol_case("tenant procedure namespace", source, NULL, "02-backfill.sql", "CREATE OR REPLACE PROCEDURE :\"tenant\".");
  if (ok) puts("sql protocol regressions: 13 cases passed");
  return ok ? 0 : 1;
}

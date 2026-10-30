/* The PostgreSQL writer of `langc sql`. See sql.h. The texts use the JsonOut
   buffer of json.h: it is one text on an arena, and the writer adds no JSON. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "json.h"
#include "sql.h"

/* PostgreSQL keeps 63 bytes of a name (NAMEDATALEN - 1). */
#define SQL_NAME_MAX_BYTES 63u
/* The lock timeout of a protocol with no lockTimeout clause. */
#define SQL_LOCK_TIMEOUT_MS 2000u

/* One write. PROTOCOL is NULL when DECLS has no such declaration. Stage I (1
   to LAST) is the step item I - 1; STAGE is the stage of the current file. */
typedef struct {
  Arena *arena;
  Diag *diag;
  const char *name; /* the protocol name */
  const DeclList *decls; /* for the key column of a table (a name lookup) */
  const Decl *protocol;
  const Report *report;
  char hash[17];
  size_t last;
  size_t stage;
  int width; /* the digits of NN */
} SqlWriter;

typedef struct {
  JsonOut path;
  JsonOut text;
} SqlFile;

/* Compare the tenant as text before using it as an identifier. A quoted
   identifier is still truncated by PostgreSQL, and search_path accepts a
   schema that does not exist. Neither case may select another tenant. */
static const char TENANT_SQL[] =
    "SELECT set_config('evolve.tenant', :'tenant', false) AS tenant \\gset evolve_\n"
    "DO $evolve$\nBEGIN\n"
    "  IF NOT EXISTS (SELECT 1 FROM pg_catalog.pg_namespace WHERE nspname::text = current_setting('evolve.tenant')) THEN\n"
    "    RAISE EXCEPTION 'EVOLVE_TENANT %', current_setting('evolve.tenant');\n"
    "  END IF;\nEND\n$evolve$;\n"
    "SET search_path TO pg_catalog, :\"tenant\", pg_temp;\n";

/* The tables and functions of all protocols. The file can run again. The
   retry of evolve.ddl: 5 tries in all, 1 s apart, on SQLSTATE 55P03
   (Q-S6-17 (a)). */
static const char SETUP_SQL[] =
    "BEGIN;\n"
    "CREATE SCHEMA IF NOT EXISTS evolve;\n"
    "CREATE TABLE IF NOT EXISTS evolve.progress (\n"
    "  tenant text NOT NULL,\n"
    "  protocol text NOT NULL,\n"
    "  stage integer NOT NULL,\n"
    "  done_at timestamptz NOT NULL DEFAULT now(),\n"
    "  PRIMARY KEY (tenant, protocol, stage)\n"
    ");\n"
    "CREATE OR REPLACE FUNCTION evolve.stage_done(p text, s integer) RETURNS boolean\n"
    "LANGUAGE sql STABLE AS $evolve$\n"
    "  SELECT EXISTS (SELECT 1 FROM evolve.progress\n"
    "    WHERE tenant = current_setting('evolve.tenant') AND protocol = p AND stage = s)\n"
    "$evolve$;\n"
    "CREATE OR REPLACE FUNCTION evolve.ddl(ddl_text text) RETURNS void\n"
    "LANGUAGE plpgsql AS $evolve$\n"
    "DECLARE\n"
    "  tries integer := 1;\n"
    "BEGIN\n"
    "  LOOP\n"
    "    BEGIN\n"
    "      EXECUTE ddl_text;\n"
    "      RETURN;\n"
    "    EXCEPTION WHEN SQLSTATE '55P03' THEN\n"
    "      IF tries >= 5 THEN\n"
    "        RAISE;\n"
    "      END IF;\n"
    "    END;\n"
    "    tries := tries + 1;\n"
    "    PERFORM pg_sleep(1);\n"
    "  END LOOP;\n"
    "END\n"
    "$evolve$;\n";

/* The tables of the guards and of the backfill (Q-S6-3, Q-S6-4, Q-S6-7 (a))
   and the tenant row. fleetAt reads the tenant list. */
static const char SETUP_TABLES_SQL[] =
    "CREATE TABLE IF NOT EXISTS evolve.tenants (tenant text PRIMARY KEY);\n"
    "CREATE TABLE IF NOT EXISTS evolve.backfill (\n"
    "  tenant text NOT NULL,\n"
    "  protocol text NOT NULL,\n"
    "  stage integer NOT NULL,\n"
    "  next_key text,\n"
    "  done boolean NOT NULL DEFAULT false,\n"
    "  PRIMARY KEY (tenant, protocol, stage)\n"
    ");\n"
    "CREATE TABLE IF NOT EXISTS evolve.app_versions (\n"
    "  app text PRIMARY KEY,\n"
    "  live boolean NOT NULL,\n"
    "  changed_at timestamptz NOT NULL DEFAULT now(),\n"
    "  seen_at timestamptz\n"
    ");\n"
    "INSERT INTO evolve.tenants (tenant) VALUES (:'tenant') ON CONFLICT DO NOTHING;\n";

/* FNV-1a 64 of the LEN bytes at TEXT. */
static uint64_t fnv1a64(const char *text, size_t len) {
  uint64_t hash = 0xcbf29ce484222325ull;
  for (size_t i = 0; i < len; i++) {
    hash ^= (unsigned char)text[i];
    hash *= 0x100000001b3ull;
  }
  return hash;
}

/* Adds N with leading zeros to WIDTH digits. */
static int put_number(JsonOut *out, unsigned long long n, int width) {
  char buf[32];
  int len = snprintf(buf, sizeof buf, "%0*llu", width, n);
  return json_put(out, buf, (size_t)len);
}

int sql_put_quoted(JsonOut *out, const char *text, char q) {
  int ok = json_put(out, &q, 1);
  for (const char *s = text; ok && *s != '\0'; s++) ok = (*s != q || json_put(out, s, 1)) && json_put(out, s, 1);
  return ok && json_put(out, &q, 1);
}

int sql_name(Arena *arena, Diag *diag, JsonOut *name, const char *protocol, size_t stage, int width, const char *suffix) {
  json_out_init(name, arena, diag, protocol);
  int ok = json_put_text(name, "evolve_") && json_put_text(name, protocol) && json_put_text(name, "_") &&
           put_number(name, stage, width) && json_put_text(name, "_") && json_put_text(name, suffix);
  if (!ok) return 0;
  return name->len <= SQL_NAME_MAX_BYTES ||
         diag_fail(diag, "SQL_NAME_LONG", protocol, "the generated name %s is longer than 63 bytes; use a shorter protocol name",
                   name->data);
}

/* Makes NAME the generated name evolve_P_NN_SUFFIX of stage STAGE. */
static int make_name(SqlWriter *w, JsonOut *name, size_t stage, const char *suffix) {
  return sql_name(w->arena, w->diag, name, w->name, stage, w->width, suffix);
}

/* Adds the generated name of stage STAGE as a quoted name. */
static int put_name(SqlWriter *w, JsonOut *out, size_t stage, const char *suffix) {
  JsonOut name;
  return make_name(w, &name, stage, suffix) && sql_put_quoted(out, name.data, '"');
}

/* Adds SELECT evolve.ddl('DDL');. The statement runs again after a lock
   timeout (SETUP_SQL). */
static int put_ddl(JsonOut *out, const JsonOut *ddl) {
  return json_put_text(out, "SELECT evolve.ddl(") && sql_put_quoted(out, ddl->data, '\'') && json_put_text(out, ");\n");
}

/* Starts DDL with ALTER TABLE "t", the table of ITEM. */
static int alter_table(SqlWriter *w, JsonOut *ddl, const FormItem *item) {
  json_out_init(ddl, w->arena, w->diag, w->name);
  return json_put_text(ddl, "ALTER TABLE ") && sql_put_quoted(ddl, item->names[0].text, '"');
}

/* The step item of stage STAGE, or NULL. */
static const FormItem *step_item(const SqlWriter *w, size_t stage) {
  int has = w->protocol != NULL && stage >= 1 && stage <= w->protocol->item_count;
  return has ? &w->protocol->items[stage - 1] : NULL;
}

static int same_names(const FormItem *a, const FormItem *b) {
  int same = a->name_count == b->name_count;
  for (size_t i = 0; same && i < a->name_count; i++) same = strcmp(a->names[i].text, b->names[i].text) == 0;
  return same;
}

/* A sync pair is symmetric in State, including the arguments of dropSync. */
static int same_sync(const FormItem *a, const FormItem *b) {
  return same_names(a, b) ||
         (a->name_count == 3 && b->name_count == 3 && strcmp(a->names[0].text, b->names[0].text) == 0 &&
          strcmp(a->names[1].text, b->names[2].text) == 0 && strcmp(a->names[2].text, b->names[1].text) == 0);
}

/* The stage of the last WORD item before the current stage with the names
   of ITEM, or the current stage when there is none. This is a name lookup,
   not a fact: the NN of the check that validate and dropCheck name, and of
   the trigger that dropSync names. */
static size_t origin_stage(const SqlWriter *w, const char *word, const FormItem *item) {
  for (size_t s = w->stage - 1; s >= 1; s--) {
    const FormItem *o = step_item(w, s);
    if (strcmp(o->word, word) == 0 && (strcmp(word, "installSync") == 0 ? same_sync(o, item) : same_names(o, item))) return s;
  }
  return w->stage;
}

/* The key column of the table declaration D, or NULL. */
static const char *decl_key(const Decl *d) {
  for (size_t i = 0; i < d->item_count; i++) {
    if (d->items[i].word != NULL && strcmp(d->items[i].word, "key") == 0) return d->items[i].names[0].text;
  }
  return NULL;
}

/* The key column of table TABLE, or NULL. This is a name lookup. */
static const char *table_key(const SqlWriter *w, const char *table) {
  for (size_t i = 0; i < w->decls->count; i++) {
    const Decl *d = &w->decls->items[i];
    if (d->kind == DECL_TABLE && strcmp(d->name, table) == 0) return decl_key(d);
  }
  return NULL;
}

/* Quoting does not prevent PostgreSQL from truncating an identifier. Check
   the names used by this protocol before writing any file. App names are
   registry text values and do not have the identifier limit. */
static int check_identifier(SqlWriter *w, const char *name) {
  return strlen(name) <= SQL_NAME_MAX_BYTES ||
         diag_fail(w->diag, "SQL_NAME_LONG", w->name, "the SQL name %s is longer than 63 bytes; use a shorter table or column name", name);
}

static int check_identifiers(SqlWriter *w) {
  for (size_t i = 0; w->protocol != NULL && i < w->protocol->item_count; i++) {
    const FormItem *item = &w->protocol->items[i];
    if (strcmp(item->word, "deploy") == 0 || strcmp(item->word, "retire") == 0) continue;
    for (size_t j = 0; j < item->name_count; j++) {
      if (!check_identifier(w, item->names[j].text)) return 0;
    }
    if (strcmp(item->word, "backfill") == 0) {
      const char *key = table_key(w, item->names[0].text);
      if (key != NULL && !check_identifier(w, key)) return 0;
    }
  }
  return 1;
}

/* Relation lookup must reach the declared tenant table, even if the session
   has a temporary table or the tenant table is missing. */
static int put_tenant_table(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut table;
  json_out_init(&table, w->arena, w->diag, w->name);
  if (strcmp(item->word, "deploy") == 0 || strcmp(item->word, "retire") == 0) return 1;
  return sql_put_quoted(&table, item->names[0].text, '"') &&
         json_put_text(out, "DO $evolve$\nDECLARE\n  evolve_table regclass := to_regclass(format('%I.%I', current_setting('evolve.tenant'), ") &&
         sql_put_quoted(out, item->names[0].text, '\'') && json_put_text(out, "));\nBEGIN\n  IF evolve_table IS NULL OR to_regclass(") &&
         sql_put_quoted(out, table.data, '\'') && json_put_text(out, ") IS DISTINCT FROM evolve_table THEN\n    RAISE EXCEPTION 'EVOLVE_TABLE %', ") &&
         sql_put_quoted(out, item->names[0].text, '\'') && json_put_text(out, ";\n  END IF;\nEND\n$evolve$;\n");
}

/* The last backfill item before the current stage that fills column C of
   table T, or NULL; *STAGE is its stage. This is a name lookup (F6). */
static const FormItem *backfill_item(const SqlWriter *w, const char *t, const char *c, size_t *stage) {
  for (size_t s = w->stage - 1; s >= 1; s--) {
    const FormItem *o = step_item(w, s);
    *stage = s;
    if (strcmp(o->word, "backfill") == 0 && strcmp(o->names[0].text, t) == 0 && strcmp(o->names[1].text, c) == 0) return o;
  }
  return NULL;
}

/* The U3 seams (PLAN 4.5, 5.1): the SQL that the U2 step writers call.
   Each dollar-quoted body uses the tag $evolve$, as SETUP_SQL. */

/* The guard and backfill tables, the tenant row and the stage 0 ledger row
   of the protocol (fleetAt 0 reads it). The file runs once for each tenant
   (psql -v tenant=NAME). */
static int seam_setup(SqlWriter *w, JsonOut *out) {
  return json_put_text(out, SETUP_TABLES_SQL) &&
         json_put_text(out, "INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (:'tenant', ") &&
         sql_put_quoted(out, w->name, '\'') && json_put_text(out, ", 0) ON CONFLICT DO NOTHING;\n");
}

/* The words of a guard fact: the kind and at most two names. */
#define SQL_GUARD_WORDS 3u

/* The guard FACT of the current stage has no SQL (Q-S6-22 (a)). */
static int guard_no_sql(SqlWriter *w, const char *fact) {
  return diag_fail(w->diag, "INTERNAL", w->name, "stage %zu of protocol %s has a guard %s with no SQL", w->stage, w->name, fact);
}

/* The guard condition writers (PLAN 4.5). Each adds the condition that
   must hold before the step, for the guard G with the words WORDS. */

/* fleetAt N: the ledger of each tenant has stage N or a later stage (M4). */
static int guard_fleet_at(SqlWriter *w, JsonOut *out, const ReportFact *g, char **words) {
  (void)g;
  return json_put_text(out, "NOT EXISTS (SELECT 1 FROM evolve.tenants t WHERE NOT EXISTS (SELECT 1 FROM evolve.progress p "
                            "WHERE p.tenant = t.tenant AND p.protocol = ") &&
         sql_put_quoted(out, w->name, '\'') && json_put_text(out, " AND p.stage >= ") && json_put_text(out, words[1]) &&
         json_put_text(out, "))");
}

/* retired A: A is not live, and no instance of A was seen after the change
   (M5). */
static int guard_retired(SqlWriter *w, JsonOut *out, const ReportFact *g, char **words) {
  (void)w;
  (void)g;
  return json_put_text(out, "NOT EXISTS (SELECT 1 FROM evolve.app_versions WHERE app = ") && sql_put_quoted(out, words[1], '\'') &&
         json_put_text(out, " AND (live OR seen_at > changed_at))");
}

/* backfillDone T C: the stage B of the backfill T C <- S is done, and no
   row has C null and S not null (M3). */
static int guard_backfill_done(SqlWriter *w, JsonOut *out, const ReportFact *g, char **words) {
  size_t stage = 0;
  const FormItem *fill = backfill_item(w, words[1], words[2], &stage);
  if (fill == NULL) return guard_no_sql(w, g->fact);
  return json_put_text(out, "evolve.stage_done(") && sql_put_quoted(out, w->name, '\'') && json_put_text(out, ", ") &&
         put_number(out, stage, 1) && json_put_text(out, ") AND NOT EXISTS (SELECT 1 FROM ") &&
         sql_put_quoted(out, fill->names[0].text, '"') && json_put_text(out, " WHERE ") && sql_put_quoted(out, fill->names[1].text, '"') &&
         json_put_text(out, " IS NULL AND ") && sql_put_quoted(out, fill->names[2].text, '"') && json_put_text(out, " IS NOT NULL)");
}

/* The writer of each guard kind and its word count. */
static const struct {
  const char *kind;
  size_t words;
  int (*write)(SqlWriter *w, JsonOut *out, const ReportFact *g, char **words);
} GUARD_SQL[] = {
  {"fleetAt", 2, guard_fleet_at},
  {"retired", 2, guard_retired},
  {"backfillDone", 3, guard_backfill_done},
};

/* The GUARD_SQL row of KIND with COUNT words, or -1. */
static int find_guard_sql(const char *kind, size_t count) {
  for (size_t i = 0; i < sizeof GUARD_SQL / sizeof GUARD_SQL[0]; i++) {
    if (strcmp(GUARD_SQL[i].kind, kind) == 0 && GUARD_SQL[i].words == count) return (int)i;
  }
  return -1;
}

/* Splits a copy of FACT at the spaces into WORDS (SQL_GUARD_WORDS at most).
   *COUNT is the word count, or SQL_GUARD_WORDS + 1 when there are more. The
   names of a fact have no space (check.c fm_fact). */
static int split_fact(SqlWriter *w, const char *fact, char **words, size_t *count) {
  size_t len = strlen(fact);
  char *s = arena_alloc(w->arena, len + 1);
  if (s == NULL) return diag_fail(w->diag, "OOM", w->name, "the arena limit is reached");
  memcpy(s, fact, len + 1);
  size_t n = 0;
  for (; s != NULL && n < SQL_GUARD_WORDS; n++) {
    words[n] = s;
    s = strchr(s, ' ');
    if (s != NULL) *s++ = '\0';
  }
  *count = s == NULL ? n : n + 1;
  return 1;
}

/* The DO block that stops the stage when the guard G is false (PLAN 4.5).
   The first word of G->fact is the kind. The message is EVOLVE_GUARD FACT
   and the detail is the guard text (Q-S6-21 (a)). */
static int seam_guard(SqlWriter *w, JsonOut *out, const ReportFact *g) {
  char *words[SQL_GUARD_WORDS];
  size_t count = 0;
  if (!split_fact(w, g->fact, words, &count)) return 0;
  int row = find_guard_sql(words[0], count);
  if (row < 0) return guard_no_sql(w, g->fact);
  return json_put_text(out, "DO $evolve$\nBEGIN\n  IF NOT (") && GUARD_SQL[row].write(w, out, g, words) &&
         json_put_text(out, ") THEN\n    RAISE EXCEPTION 'EVOLVE_GUARD %', ") && sql_put_quoted(out, g->fact, '\'') &&
         json_put_text(out, " USING DETAIL = ") && sql_put_quoted(out, g->guard, '\'') &&
         json_put_text(out, ";\n  END IF;\nEND\n$evolve$;\n");
}

/* Adds ROW."COLUMN", a column of the trigger row NEW or OLD. */
static int put_row_column(JsonOut *out, const char *row, const char *column) {
  return json_put_text(out, row) && json_put_text(out, ".") && sql_put_quoted(out, column, '"');
}

/* Adds the line INDENT NEW."TO" := NEW."FROM";. */
static int put_copy(JsonOut *out, const char *indent, const char *to, const char *from) {
  return json_put_text(out, indent) && put_row_column(out, "NEW", to) && json_put_text(out, " := ") && put_row_column(out, "NEW", from) &&
         json_put_text(out, ";\n");
}

/* Adds NEW."COLUMN" IS DISTINCT FROM OLD."COLUMN". */
static int put_changed(JsonOut *out, const char *column) {
  return put_row_column(out, "NEW", column) && json_put_text(out, " IS DISTINCT FROM ") && put_row_column(out, "OLD", column);
}

/* The trigger function NAME for the names t c s of ITEM (Q-S6-8 (a)). An
   INSERT fills the null column from the other one. An UPDATE copies c to s
   when c changes, else s to c when s changes: c wins when both change (M8). */
static int seam_sync_function(SqlWriter *w, JsonOut *out, const FormItem *item, const char *name) {
  const char *c = item->names[1].text;
  const char *s = item->names[2].text;
  (void)w;
  return json_put_text(out, "CREATE OR REPLACE FUNCTION :\"tenant\".") && sql_put_quoted(out, name, '"') &&
         json_put_text(out, "() RETURNS trigger\nLANGUAGE plpgsql AS $evolve$\nBEGIN\n  IF TG_OP = 'INSERT' THEN\n    IF ") &&
         put_row_column(out, "NEW", c) && json_put_text(out, " IS NULL THEN\n") && put_copy(out, "      ", c, s) &&
         json_put_text(out, "    ELSIF ") && put_row_column(out, "NEW", s) && json_put_text(out, " IS NULL THEN\n") &&
         put_copy(out, "      ", s, c) && json_put_text(out, "    END IF;\n  ELSIF ") && put_changed(out, c) &&
         json_put_text(out, " THEN\n") && put_copy(out, "    ", s, c) && json_put_text(out, "  ELSIF ") && put_changed(out, s) &&
         json_put_text(out, " THEN\n") && put_copy(out, "    ", c, s) && json_put_text(out, "  END IF;\n  RETURN NEW;\nEND\n$evolve$;\n");
}

/* Adds the condition of the evolve.backfill row of the current stage. */
static int put_fill_row(SqlWriter *w, JsonOut *out) {
  return json_put_text(out, "tenant = current_setting('evolve.tenant') AND protocol = ") && sql_put_quoted(out, w->name, '\'') &&
         json_put_text(out, " AND stage = ") && put_number(out, w->stage, 1);
}

/* The backfill procedure NAME and its CALL (Q-S6-7 (a)): ITEM->nat rows a
   batch in key order, one transaction a batch. The evolve.backfill row keeps
   the last key, so a new run continues after the last batch. The CALL is a
   top-level statement of a bare stage, so the COMMIT is legal. */
static int seam_backfill(SqlWriter *w, JsonOut *out, const FormItem *item, const char *name) {
  const char *t = item->names[0].text;
  const char *key = table_key(w, t);
  if (key == NULL) return diag_fail(w->diag, "INTERNAL", w->name, "stage %zu of protocol %s has no step SQL", w->stage, w->name);
  return json_put_text(out, "CREATE OR REPLACE PROCEDURE :\"tenant\".") && sql_put_quoted(out, name, '"') &&
         json_put_text(out, "()\nLANGUAGE plpgsql AS $evolve$\n<<evolve_fill>>\nDECLARE\n  evolve_last_key ") && sql_put_quoted(out, t, '"') &&
         json_put_text(out, ".") && sql_put_quoted(out, key, '"') &&
         json_put_text(out, "%TYPE;\n  evolve_next_text text;\n  evolve_finished boolean;\nBEGIN\n"
                            "  INSERT INTO evolve.backfill (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), ") &&
         sql_put_quoted(out, w->name, '\'') && json_put_text(out, ", ") && put_number(out, w->stage, 1) &&
         json_put_text(out, ") ON CONFLICT DO NOTHING;\n  COMMIT;\n  LOOP\n"
                            "    SELECT next_key, done INTO evolve_next_text, evolve_finished FROM evolve.backfill WHERE ") &&
         put_fill_row(w, out) &&
         json_put_text(out, ";\n    EXIT WHEN evolve_finished;\n    evolve_last_key := evolve_next_text;\n    WITH batch AS (SELECT ") &&
         put_row_column(out, "evolve_rows", key) && json_put_text(out, " FROM ") && sql_put_quoted(out, t, '"') &&
         json_put_text(out, " evolve_rows WHERE evolve_fill.evolve_last_key IS NULL OR ") && put_row_column(out, "evolve_rows", key) &&
         json_put_text(out, " > evolve_fill.evolve_last_key ORDER BY ") &&
         put_row_column(out, "evolve_rows", key) && json_put_text(out, " LIMIT ") && put_number(out, item->nat, 1) &&
         json_put_text(out, "),\n    filled AS (UPDATE ") && sql_put_quoted(out, t, '"') && json_put_text(out, " u SET ") &&
         sql_put_quoted(out, item->names[1].text, '"') && json_put_text(out, " = ") && put_row_column(out, "u", item->names[2].text) &&
         json_put_text(out, " FROM batch WHERE ") && put_row_column(out, "u", key) && json_put_text(out, " = ") &&
         put_row_column(out, "batch", key) && json_put_text(out, " AND ") && put_row_column(out, "u", item->names[1].text) &&
         json_put_text(out, " IS DISTINCT FROM ") && put_row_column(out, "u", item->names[2].text) &&
         json_put_text(out, ")\n    SELECT max(") && put_row_column(out, "batch", key) &&
         json_put_text(out, ") INTO evolve_last_key FROM batch;\n"
                            "    UPDATE evolve.backfill SET next_key = coalesce(evolve_last_key::text, next_key),"
                            " done = evolve_last_key IS NULL WHERE ") &&
         put_fill_row(w, out) && json_put_text(out, ";\n    COMMIT;\n  END LOOP;\nEND\n$evolve$;\nCALL :\"tenant\".") && sql_put_quoted(out, name, '"') &&
         json_put_text(out, "();\n");
}

/* The drop of an INVALID index NAME that a failed CREATE INDEX CONCURRENTLY
   left (Q-S6-10 (a)). DROP INDEX CONCURRENTLY cannot run in a transaction
   block (PostgreSQL 18 DROP INDEX), so a psql \if selects it, not a DO
   block. */
static int seam_index_drop(SqlWriter *w, JsonOut *out, const FormItem *item, const char *name) {
  JsonOut quoted;
  (void)item;
  json_out_init(&quoted, w->arena, w->diag, w->name);
  return sql_put_quoted(&quoted, name, '"') &&
         json_put_text(out, "SELECT EXISTS (SELECT 1 FROM pg_index WHERE indexrelid = to_regclass(") &&
         sql_put_quoted(out, quoted.data, '\'') &&
         json_put_text(out, ") AND NOT indisvalid) AS invalid \\gset evolve_\n\\if :evolve_invalid\nDROP INDEX CONCURRENTLY IF EXISTS ") &&
         sql_put_quoted(out, name, '"') && json_put_text(out, ";\n\\endif\n");
}

/* IF NOT EXISTS only checks the relation name. Before removing an INVALID
   index, verify its definition; after CREATE, also require a usable index.
   This preserves retries without adopting or dropping an unrelated object. */
static int seam_index_check(SqlWriter *w, JsonOut *out, const FormItem *item, const char *name, int valid) {
  JsonOut index, table;
  json_out_init(&index, w->arena, w->diag, w->name);
  json_out_init(&table, w->arena, w->diag, w->name);
  if (!sql_put_quoted(&index, name, '"') || !sql_put_quoted(&table, item->names[0].text, '"') ||
      !json_put_text(out, "DO $evolve$\nBEGIN\n  IF ")) return 0;
  if (!valid && !(json_put_text(out, "to_regclass(") && sql_put_quoted(out, index.data, '\'') &&
                 json_put_text(out, ") IS NOT NULL AND "))) return 0;
  return json_put_text(out, "NOT EXISTS (SELECT 1 FROM pg_catalog.pg_index i JOIN pg_catalog.pg_class c ON c.oid = i.indexrelid "
                            "JOIN pg_catalog.pg_am a ON a.oid = c.relam "
                            "JOIN pg_catalog.pg_attribute v ON v.attrelid = i.indrelid AND v.attnum = i.indkey[0] "
                            "JOIN pg_catalog.pg_opclass o ON o.oid = i.indclass[0] WHERE i.indexrelid = to_regclass(") &&
         sql_put_quoted(out, index.data, '\'') && json_put_text(out, ") AND i.indrelid = to_regclass(") &&
         sql_put_quoted(out, table.data, '\'') &&
         json_put_text(out, ") AND a.amname = 'btree' AND i.indnkeyatts = 1 AND i.indnatts = 1 "
                            "AND NOT i.indisunique AND NOT i.indisexclusion AND i.indexprs IS NULL AND i.indpred IS NULL "
                            "AND i.indoption[0] = 0 AND i.indcollation[0] = v.attcollation "
                            "AND o.opcdefault AND o.opcmethod = c.relam AND o.opcintype = v.atttypid "
                            "AND pg_get_indexdef(i.indexrelid, 1, false) = quote_ident(") &&
         sql_put_quoted(out, item->names[1].text, '\'') && json_put_text(out, ")") &&
         (!valid || json_put_text(out, " AND i.indisvalid AND i.indisready AND i.indislive")) &&
         json_put_text(out, ") THEN\n    RAISE EXCEPTION 'EVOLVE_INDEX %', ") && sql_put_quoted(out, name, '\'') &&
         json_put_text(out, ";\n  END IF;\nEND\n$evolve$;\n");
}

/* The version registry row of a deploy (live) or a retire (not live).
   changed_at moves only when live changes, so the run for the next tenant
   does not hide a later seen_at (Q-S6-4 (a)). */
static int seam_registry(SqlWriter *w, JsonOut *out, const FormItem *item) {
  (void)w;
  return json_put_text(out, "INSERT INTO evolve.app_versions (app, live) VALUES (") && sql_put_quoted(out, item->names[0].text, '\'') &&
         json_put_text(out, strcmp(item->word, "deploy") == 0 ? ", true)" : ", false)") &&
         json_put_text(out, " ON CONFLICT (app) DO UPDATE SET live = EXCLUDED.live, changed_at = now()\n"
                            "  WHERE evolve.app_versions.live IS DISTINCT FROM EXCLUDED.live;\n");
}

/* The step writers (PLAN 5.1). The names of ITEM: t c ty (addColumn), t c
   c' (installSync, dropSync, backfill), a (deploy, retire), t c (the
   others). */

static int sql_add_column(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut ddl;
  return alter_table(w, &ddl, item) && json_put_text(&ddl, " ADD COLUMN ") && sql_put_quoted(&ddl, item->names[1].text, '"') &&
         json_put_text(&ddl, " ") && json_put_text(&ddl, item->names[2].text) && put_ddl(out, &ddl);
}

static int sql_install_sync(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut name;
  JsonOut ddl;
  if (!make_name(w, &name, w->stage, "sync") || !seam_sync_function(w, out, item, name.data)) return 0;
  json_out_init(&ddl, w->arena, w->diag, w->name);
  return json_put_text(&ddl, "CREATE TRIGGER ") && sql_put_quoted(&ddl, name.data, '"') && json_put_text(&ddl, " BEFORE INSERT OR UPDATE ON ") &&
         sql_put_quoted(&ddl, item->names[0].text, '"') && json_put_text(&ddl, " FOR EACH ROW EXECUTE FUNCTION ") &&
         sql_put_quoted(&ddl, name.data, '"') && json_put_text(&ddl, "()") && put_ddl(out, &ddl);
}

static int sql_drop_sync(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut name;
  JsonOut trigger;
  JsonOut function;
  if (!make_name(w, &name, origin_stage(w, "installSync", item), "sync")) return 0;
  json_out_init(&trigger, w->arena, w->diag, w->name);
  json_out_init(&function, w->arena, w->diag, w->name);
  return json_put_text(&trigger, "DROP TRIGGER ") && sql_put_quoted(&trigger, name.data, '"') && json_put_text(&trigger, " ON ") &&
         sql_put_quoted(&trigger, item->names[0].text, '"') && put_ddl(out, &trigger) && json_put_text(&function, "DROP FUNCTION ") &&
         sql_put_quoted(&function, name.data, '"') && json_put_text(&function, "()") && put_ddl(out, &function);
}

static int sql_backfill(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut name;
  return make_name(w, &name, w->stage, "fill") && seam_backfill(w, out, item, name.data);
}

static int sql_registry(SqlWriter *w, JsonOut *out, const FormItem *item) {
  return seam_registry(w, out, item);
}

static int sql_drop_column(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut ddl;
  return alter_table(w, &ddl, item) && json_put_text(&ddl, " DROP COLUMN ") && sql_put_quoted(&ddl, item->names[1].text, '"') &&
         put_ddl(out, &ddl);
}

static int sql_add_check(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut ddl;
  return alter_table(w, &ddl, item) && json_put_text(&ddl, " ADD CONSTRAINT ") && put_name(w, &ddl, w->stage, "nn") &&
         json_put_text(&ddl, " CHECK (") && sql_put_quoted(&ddl, item->names[1].text, '"') && json_put_text(&ddl, " IS NOT NULL) NOT VALID") &&
         put_ddl(out, &ddl);
}

static int sql_validate(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut ddl;
  return alter_table(w, &ddl, item) && json_put_text(&ddl, " VALIDATE CONSTRAINT ") &&
         put_name(w, &ddl, origin_stage(w, "addNotNullCheck", item), "nn") && put_ddl(out, &ddl);
}

static int sql_set_not_null(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut ddl;
  return alter_table(w, &ddl, item) && json_put_text(&ddl, " ALTER COLUMN ") && sql_put_quoted(&ddl, item->names[1].text, '"') &&
         json_put_text(&ddl, " SET NOT NULL") && put_ddl(out, &ddl);
}

static int sql_drop_check(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut ddl;
  return alter_table(w, &ddl, item) && json_put_text(&ddl, " DROP CONSTRAINT ") &&
         put_name(w, &ddl, origin_stage(w, "addNotNullCheck", item), "nn") && put_ddl(out, &ddl);
}

/* A bare statement: CREATE INDEX CONCURRENTLY cannot run in a function. */
static int sql_create_index(SqlWriter *w, JsonOut *out, const FormItem *item) {
  JsonOut name;
  return make_name(w, &name, w->stage, "idx") && seam_index_check(w, out, item, name.data, 0) && seam_index_drop(w, out, item, name.data) &&
         json_put_text(out, "CREATE INDEX CONCURRENTLY IF NOT EXISTS ") && sql_put_quoted(out, name.data, '"') && json_put_text(out, " ON ") &&
         sql_put_quoted(out, item->names[0].text, '"') && json_put_text(out, " (") && sql_put_quoted(out, item->names[1].text, '"') &&
         json_put_text(out, ");\n") && seam_index_check(w, out, item, name.data, 1);
}

/* The writer of each Step word. A bare stage has no BEGIN and COMMIT and no
   retry (Q-S6-5 (a)): a backfill commits each batch, and CREATE INDEX
   CONCURRENTLY cannot run in a transaction. */
static const struct {
  const char *word;
  int bare;
  int (*write)(SqlWriter *w, JsonOut *out, const FormItem *item);
} STEP_SQL[] = {
  {"addColumn", 0, sql_add_column},
  {"installSync", 0, sql_install_sync},
  {"dropSync", 0, sql_drop_sync},
  {"backfill", 1, sql_backfill},
  {"deploy", 0, sql_registry},
  {"retire", 0, sql_registry},
  {"dropColumn", 0, sql_drop_column},
  {"addNotNullCheck", 0, sql_add_check},
  {"validate", 0, sql_validate},
  {"setNotNull", 0, sql_set_not_null},
  {"dropCheck", 0, sql_drop_check},
  {"createIndexConcurrently", 1, sql_create_index},
};

/* The STEP_SQL row of WORD, or -1. */
static int find_step_sql(const char *word) {
  for (size_t i = 0; i < sizeof STEP_SQL / sizeof STEP_SQL[0]; i++) {
    if (strcmp(STEP_SQL[i].word, word) == 0) return (int)i;
  }
  return -1;
}

static const Decl *find_protocol(const DeclList *decls, const char *name) {
  for (size_t i = 0; i < decls->count; i++) {
    const Decl *d = &decls->items[i];
    if (d->kind == DECL_PROTOCOL && strcmp(d->name, name) == 0) return d;
  }
  return NULL;
}

/* Adds the first line of a file (Q-S6-2 (a)). */
static int put_header(SqlWriter *w, JsonOut *out, size_t stage, const char *step) {
  return json_put_text(out, "-- evolve 1; source fnv1a64 ") && json_put_text(out, w->hash) && json_put_text(out, "; stage ") &&
         put_number(out, stage, 1) && json_put_text(out, " of ") && put_number(out, w->last, 1) && json_put_text(out, ": ") &&
         json_put_text(out, step) && json_put_text(out, "\n");
}

static int put_setup(SqlWriter *w, JsonOut *out) {
  return put_header(w, out, 0, "setup") && json_put_text(out, "\\set ON_ERROR_STOP on\n") &&
         json_put_text(out, TENANT_SQL) && json_put_text(out, SETUP_SQL) && seam_setup(w, out) && json_put_text(out, "COMMIT;\n");
}

/* Adds the progress ledger row of the current stage. */
static int put_ledger(SqlWriter *w, JsonOut *out) {
  return json_put_text(out, "INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), ") &&
         sql_put_quoted(out, w->name, '\'') && json_put_text(out, ", ") && put_number(out, w->stage, 1) && json_put_text(out, ");\n");
}

/* Adds the text of stage STAGE (1 to LAST): the tenant, the lock timeout,
   the skip, the guards (comments in U2), the step SQL and the ledger row. */
static int put_stage(SqlWriter *w, JsonOut *out, size_t stage) {
  const FormItem *item = step_item(w, stage);
  int row = item == NULL ? -1 : find_step_sql(item->word);
  if (row < 0) return diag_fail(w->diag, "INTERNAL", w->name, "stage %zu of protocol %s has no step SQL", stage, w->name);
  const ReportStage *rs = &w->report->stages[stage];
  int bare = STEP_SQL[row].bare;
  uint64_t timeout = w->protocol->lock_timeout == 0 ? SQL_LOCK_TIMEOUT_MS : w->protocol->lock_timeout;
  w->stage = stage;
  int ok = put_header(w, out, stage, rs->step) &&
           json_put_text(out, "\\set ON_ERROR_STOP on\n") && json_put_text(out, TENANT_SQL) &&
           json_put_text(out, bare ? "SET lock_timeout = '" : "BEGIN;\nSET LOCAL lock_timeout = '") && put_number(out, timeout, 1) &&
           json_put_text(out, "ms';\nSELECT NOT evolve.stage_done(") && sql_put_quoted(out, w->name, '\'') && json_put_text(out, ", ") &&
           put_number(out, stage, 1) && json_put_text(out, ") AS run \\gset evolve_\n\\if :evolve_run\n") && put_tenant_table(w, out, item);
  for (size_t i = 0; ok && i < rs->guard_count; i++) {
    const ReportFact *g = &rs->guards[i];
    ok = json_put_text(out, "-- guard: ") && json_put_text(out, g->fact) && json_put_text(out, ": ") && json_put_text(out, g->guard) &&
         json_put_text(out, "\n") && seam_guard(w, out, g);
  }
  return ok && STEP_SQL[row].write(w, out, item) && put_ledger(w, out) && json_put_text(out, "\\endif\n") &&
         (bare || json_put_text(out, "COMMIT;\n"));
}

/* Writes TEXT to PATH. */
static int write_file(Diag *diag, const SqlFile *f) {
  FILE *file = fopen(f->path.data, "wb");
  size_t written = file == NULL ? 0 : fwrite(f->text.data, 1, f->text.len, file);
  int closed = file != NULL && fclose(file) == 0;
  return (written == f->text.len && closed) || diag_fail(diag, "IO", NULL, "cannot write %s", f->path.data);
}

int sql_write(Arena *arena, Diag *diag, const DeclList *decls, const Report *r, const char *text, size_t len,
              const char *outdir) {
  SqlWriter w;
  memset(&w, 0, sizeof w);
  w.arena = arena;
  w.diag = diag;
  w.name = r->protocol;
  w.decls = decls;
  w.protocol = find_protocol(decls, r->protocol);
  w.report = r;
  w.last = r->stage_count == 0 ? 0 : r->stage_count - 1;
  w.width = 2;
  for (size_t n = w.last; n >= 100; n /= 10) w.width++;
  snprintf(w.hash, sizeof w.hash, "%016llx", (unsigned long long)fnv1a64(text, len));
  SqlFile *files = arena_alloc(arena, (w.last + 1) * sizeof *files);
  if (files == NULL) return diag_fail(diag, "OOM", w.name, "the arena limit is reached") + 2;
  int ok = check_identifiers(&w);
  for (size_t i = 0; ok && i <= w.last; i++) {
    JsonOut *path = &files[i].path;
    json_out_init(path, arena, diag, w.name);
    json_out_init(&files[i].text, arena, diag, w.name);
    ok = (i == 0 ? put_setup(&w, &files[i].text) : put_stage(&w, &files[i].text, i)) && json_put_text(path, outdir) &&
         json_put_text(path, "/") && put_number(path, i, i == 0 ? 2 : w.width) && json_put_text(path, "-") &&
         json_put_text(path, i == 0 ? "setup" : step_item(&w, i)->word) && json_put_text(path, ".sql");
  }
  for (size_t i = 0; ok && i <= w.last; i++) ok = write_file(diag, &files[i]);
  return ok ? 0 : strcmp(diag->code, "SQL_NAME_LONG") == 0 ? 1 : 2;
}

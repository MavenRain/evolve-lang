-- evolve 1; source fnv1a64 87265edb501ee1a1; stage 3 of 7: backfill users display_name ← name batch 1000
\set ON_ERROR_STOP on
SELECT set_config('evolve.tenant', :'tenant', false) AS tenant \gset evolve_
DO $evolve$
BEGIN
  IF NOT EXISTS (SELECT 1 FROM pg_catalog.pg_namespace WHERE nspname::text = current_setting('evolve.tenant')) THEN
    RAISE EXCEPTION 'EVOLVE_TENANT %', current_setting('evolve.tenant');
  END IF;
END
$evolve$;
SET search_path TO pg_catalog, :"tenant", pg_temp;
SET lock_timeout = '2000ms';
SELECT NOT evolve.stage_done('long_name_of_forty_eight_bytes_for_sql_name_long', 3) AS run \gset evolve_
\if :evolve_run
DO $evolve$
DECLARE
  evolve_table regclass := to_regclass(format('%I.%I', current_setting('evolve.tenant'), 'users'));
BEGIN
  IF evolve_table IS NULL OR to_regclass('"users"') IS DISTINCT FROM evolve_table THEN
    RAISE EXCEPTION 'EVOLVE_TABLE %', 'users';
  END IF;
END
$evolve$;
CREATE OR REPLACE PROCEDURE :"tenant"."evolve_long_name_of_forty_eight_bytes_for_sql_name_long_03_fill"()
LANGUAGE plpgsql AS $evolve$
<<evolve_fill>>
DECLARE
  evolve_last_key "users"."id"%TYPE;
  evolve_next_text text;
  evolve_finished boolean;
BEGIN
  INSERT INTO evolve.backfill (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'long_name_of_forty_eight_bytes_for_sql_name_long', 3) ON CONFLICT DO NOTHING;
  COMMIT;
  LOOP
    SELECT next_key, done INTO evolve_next_text, evolve_finished FROM evolve.backfill WHERE tenant = current_setting('evolve.tenant') AND protocol = 'long_name_of_forty_eight_bytes_for_sql_name_long' AND stage = 3;
    EXIT WHEN evolve_finished;
    evolve_last_key := evolve_next_text;
    WITH batch AS (SELECT evolve_rows."id" FROM "users" evolve_rows WHERE evolve_fill.evolve_last_key IS NULL OR evolve_rows."id" > evolve_fill.evolve_last_key ORDER BY evolve_rows."id" LIMIT 1000),
    filled AS (UPDATE "users" u SET "display_name" = u."name" FROM batch WHERE u."id" = batch."id" AND u."display_name" IS DISTINCT FROM u."name")
    SELECT max(batch."id") INTO evolve_last_key FROM batch;
    UPDATE evolve.backfill SET next_key = coalesce(evolve_last_key::text, next_key), done = evolve_last_key IS NULL WHERE tenant = current_setting('evolve.tenant') AND protocol = 'long_name_of_forty_eight_bytes_for_sql_name_long' AND stage = 3;
    COMMIT;
  END LOOP;
END
$evolve$;
CALL :"tenant"."evolve_long_name_of_forty_eight_bytes_for_sql_name_long_03_fill"();
INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'long_name_of_forty_eight_bytes_for_sql_name_long', 3);
\endif

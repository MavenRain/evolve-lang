-- evolve 1; source fnv1a64 c7aaf2d1430fca61; stage 3 of 6: backfill users full ← name batch 1
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
SELECT NOT evolve.stage_done('edges', 3) AS run \gset evolve_
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
CREATE OR REPLACE PROCEDURE :"tenant"."evolve_edges_03_fill"()
LANGUAGE plpgsql AS $evolve$
<<evolve_fill>>
DECLARE
  evolve_last_key "users"."evolve_last_key"%TYPE;
  evolve_next_text text;
  evolve_finished boolean;
BEGIN
  INSERT INTO evolve.backfill (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'edges', 3) ON CONFLICT DO NOTHING;
  COMMIT;
  LOOP
    SELECT next_key, done INTO evolve_next_text, evolve_finished FROM evolve.backfill WHERE tenant = current_setting('evolve.tenant') AND protocol = 'edges' AND stage = 3;
    EXIT WHEN evolve_finished;
    evolve_last_key := evolve_next_text;
    WITH batch AS (SELECT evolve_rows."evolve_last_key" FROM "users" evolve_rows WHERE evolve_fill.evolve_last_key IS NULL OR evolve_rows."evolve_last_key" > evolve_fill.evolve_last_key ORDER BY evolve_rows."evolve_last_key" LIMIT 1),
    filled AS (UPDATE "users" u SET "full" = u."name" FROM batch WHERE u."evolve_last_key" = batch."evolve_last_key" AND u."full" IS DISTINCT FROM u."name")
    SELECT max(batch."evolve_last_key") INTO evolve_last_key FROM batch;
    UPDATE evolve.backfill SET next_key = coalesce(evolve_last_key::text, next_key), done = evolve_last_key IS NULL WHERE tenant = current_setting('evolve.tenant') AND protocol = 'edges' AND stage = 3;
    COMMIT;
  END LOOP;
END
$evolve$;
CALL :"tenant"."evolve_edges_03_fill"();
INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'edges', 3);
\endif

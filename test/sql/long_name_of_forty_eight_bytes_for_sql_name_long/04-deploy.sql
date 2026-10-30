-- evolve 1; source fnv1a64 87265edb501ee1a1; stage 4 of 7: deploy v3
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
BEGIN;
SET LOCAL lock_timeout = '2000ms';
SELECT NOT evolve.stage_done('long_name_of_forty_eight_bytes_for_sql_name_long', 4) AS run \gset evolve_
\if :evolve_run
-- guard: fleetAt 3: each tenant ledger shows stage 3 or later
DO $evolve$
BEGIN
  IF NOT (NOT EXISTS (SELECT 1 FROM evolve.tenants t WHERE NOT EXISTS (SELECT 1 FROM evolve.progress p WHERE p.tenant = t.tenant AND p.protocol = 'long_name_of_forty_eight_bytes_for_sql_name_long' AND p.stage >= 3))) THEN
    RAISE EXCEPTION 'EVOLVE_GUARD %', 'fleetAt 3' USING DETAIL = 'each tenant ledger shows stage 3 or later';
  END IF;
END
$evolve$;
-- guard: backfillDone users display_name: the progress ledger has done for the backfill of users.display_name; no row has display_name null where the source column is not null
DO $evolve$
BEGIN
  IF NOT (evolve.stage_done('long_name_of_forty_eight_bytes_for_sql_name_long', 3) AND NOT EXISTS (SELECT 1 FROM "users" WHERE "display_name" IS NULL AND "name" IS NOT NULL)) THEN
    RAISE EXCEPTION 'EVOLVE_GUARD %', 'backfillDone users display_name' USING DETAIL = 'the progress ledger has done for the backfill of users.display_name; no row has display_name null where the source column is not null';
  END IF;
END
$evolve$;
INSERT INTO evolve.app_versions (app, live) VALUES ('v3', true) ON CONFLICT (app) DO UPDATE SET live = EXCLUDED.live, changed_at = now()
  WHERE evolve.app_versions.live IS DISTINCT FROM EXCLUDED.live;
INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'long_name_of_forty_eight_bytes_for_sql_name_long', 4);
\endif
COMMIT;

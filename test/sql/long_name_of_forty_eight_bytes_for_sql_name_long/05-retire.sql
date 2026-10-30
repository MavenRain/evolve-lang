-- evolve 1; source fnv1a64 87265edb501ee1a1; stage 5 of 7: retire v1
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
SELECT NOT evolve.stage_done('long_name_of_forty_eight_bytes_for_sql_name_long', 5) AS run \gset evolve_
\if :evolve_run
-- guard: fleetAt 4: each tenant ledger shows stage 4 or later
DO $evolve$
BEGIN
  IF NOT (NOT EXISTS (SELECT 1 FROM evolve.tenants t WHERE NOT EXISTS (SELECT 1 FROM evolve.progress p WHERE p.tenant = t.tenant AND p.protocol = 'long_name_of_forty_eight_bytes_for_sql_name_long' AND p.stage >= 4))) THEN
    RAISE EXCEPTION 'EVOLVE_GUARD %', 'fleetAt 4' USING DETAIL = 'each tenant ledger shows stage 4 or later';
  END IF;
END
$evolve$;
INSERT INTO evolve.app_versions (app, live) VALUES ('v1', false) ON CONFLICT (app) DO UPDATE SET live = EXCLUDED.live, changed_at = now()
  WHERE evolve.app_versions.live IS DISTINCT FROM EXCLUDED.live;
INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'long_name_of_forty_eight_bytes_for_sql_name_long', 5);
\endif
COMMIT;

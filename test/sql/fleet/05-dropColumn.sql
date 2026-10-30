-- evolve 1; source fnv1a64 a5a58965a4dcb951; stage 5 of 5: dropColumn items label
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
SELECT NOT evolve.stage_done('fleet', 5) AS run \gset evolve_
\if :evolve_run
DO $evolve$
DECLARE
  evolve_table regclass := to_regclass(format('%I.%I', current_setting('evolve.tenant'), 'items'));
BEGIN
  IF evolve_table IS NULL OR to_regclass('"items"') IS DISTINCT FROM evolve_table THEN
    RAISE EXCEPTION 'EVOLVE_TABLE %', 'items';
  END IF;
END
$evolve$;
-- guard: retired v1: the version registry has no live row for v1
DO $evolve$
BEGIN
  IF NOT (NOT EXISTS (SELECT 1 FROM evolve.app_versions WHERE app = 'v1' AND (live OR seen_at > changed_at))) THEN
    RAISE EXCEPTION 'EVOLVE_GUARD %', 'retired v1' USING DETAIL = 'the version registry has no live row for v1';
  END IF;
END
$evolve$;
-- guard: retired v2: the version registry has no live row for v2
DO $evolve$
BEGIN
  IF NOT (NOT EXISTS (SELECT 1 FROM evolve.app_versions WHERE app = 'v2' AND (live OR seen_at > changed_at))) THEN
    RAISE EXCEPTION 'EVOLVE_GUARD %', 'retired v2' USING DETAIL = 'the version registry has no live row for v2';
  END IF;
END
$evolve$;
SELECT evolve.ddl('ALTER TABLE "items" DROP COLUMN "label"');
INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'fleet', 5);
\endif
COMMIT;

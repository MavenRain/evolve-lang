-- evolve 1; source fnv1a64 926f25f2192226a4; stage 5 of 8: addNotNullCheck users handle
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
SELECT NOT evolve.stage_done('strict', 5) AS run \gset evolve_
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
-- guard: retired v1: the version registry has no live row for v1
DO $evolve$
BEGIN
  IF NOT (NOT EXISTS (SELECT 1 FROM evolve.app_versions WHERE app = 'v1' AND (live OR seen_at > changed_at))) THEN
    RAISE EXCEPTION 'EVOLVE_GUARD %', 'retired v1' USING DETAIL = 'the version registry has no live row for v1';
  END IF;
END
$evolve$;
SELECT evolve.ddl('ALTER TABLE "users" ADD CONSTRAINT "evolve_strict_05_nn" CHECK ("handle" IS NOT NULL) NOT VALID');
INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'strict', 5);
\endif
COMMIT;

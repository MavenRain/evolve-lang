-- evolve 1; source fnv1a64 c7aaf2d1430fca61; stage 2 of 6: installSync users full name
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
SELECT NOT evolve.stage_done('edges', 2) AS run \gset evolve_
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
CREATE OR REPLACE FUNCTION :"tenant"."evolve_edges_02_sync"() RETURNS trigger
LANGUAGE plpgsql AS $evolve$
BEGIN
  IF TG_OP = 'INSERT' THEN
    IF NEW."full" IS NULL THEN
      NEW."full" := NEW."name";
    ELSIF NEW."name" IS NULL THEN
      NEW."name" := NEW."full";
    END IF;
  ELSIF NEW."full" IS DISTINCT FROM OLD."full" THEN
    NEW."name" := NEW."full";
  ELSIF NEW."name" IS DISTINCT FROM OLD."name" THEN
    NEW."full" := NEW."name";
  END IF;
  RETURN NEW;
END
$evolve$;
SELECT evolve.ddl('CREATE TRIGGER "evolve_edges_02_sync" BEFORE INSERT OR UPDATE ON "users" FOR EACH ROW EXECUTE FUNCTION "evolve_edges_02_sync"()');
INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'edges', 2);
\endif
COMMIT;

-- evolve 1; source fnv1a64 87265edb501ee1a1; stage 2 of 7: installSync users display_name name
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
SELECT NOT evolve.stage_done('long_name_of_forty_eight_bytes_for_sql_name_long', 2) AS run \gset evolve_
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
CREATE OR REPLACE FUNCTION :"tenant"."evolve_long_name_of_forty_eight_bytes_for_sql_name_long_02_sync"() RETURNS trigger
LANGUAGE plpgsql AS $evolve$
BEGIN
  IF TG_OP = 'INSERT' THEN
    IF NEW."display_name" IS NULL THEN
      NEW."display_name" := NEW."name";
    ELSIF NEW."name" IS NULL THEN
      NEW."name" := NEW."display_name";
    END IF;
  ELSIF NEW."display_name" IS DISTINCT FROM OLD."display_name" THEN
    NEW."name" := NEW."display_name";
  ELSIF NEW."name" IS DISTINCT FROM OLD."name" THEN
    NEW."display_name" := NEW."name";
  END IF;
  RETURN NEW;
END
$evolve$;
SELECT evolve.ddl('CREATE TRIGGER "evolve_long_name_of_forty_eight_bytes_for_sql_name_long_02_sync" BEFORE INSERT OR UPDATE ON "users" FOR EACH ROW EXECUTE FUNCTION "evolve_long_name_of_forty_eight_bytes_for_sql_name_long_02_sync"()');
INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'long_name_of_forty_eight_bytes_for_sql_name_long', 2);
\endif
COMMIT;

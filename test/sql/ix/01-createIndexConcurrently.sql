-- evolve 1; source fnv1a64 62d26062b87272c6; stage 1 of 1: createIndexConcurrently users email
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
SET lock_timeout = '5000ms';
SELECT NOT evolve.stage_done('ix', 1) AS run \gset evolve_
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
DO $evolve$
BEGIN
  IF to_regclass('"evolve_ix_01_idx"') IS NOT NULL AND NOT EXISTS (SELECT 1 FROM pg_catalog.pg_index i JOIN pg_catalog.pg_class c ON c.oid = i.indexrelid JOIN pg_catalog.pg_am a ON a.oid = c.relam JOIN pg_catalog.pg_attribute v ON v.attrelid = i.indrelid AND v.attnum = i.indkey[0] JOIN pg_catalog.pg_opclass o ON o.oid = i.indclass[0] WHERE i.indexrelid = to_regclass('"evolve_ix_01_idx"') AND i.indrelid = to_regclass('"users"') AND a.amname = 'btree' AND i.indnkeyatts = 1 AND i.indnatts = 1 AND NOT i.indisunique AND NOT i.indisexclusion AND i.indexprs IS NULL AND i.indpred IS NULL AND i.indoption[0] = 0 AND i.indcollation[0] = v.attcollation AND o.opcdefault AND o.opcmethod = c.relam AND o.opcintype = v.atttypid AND pg_get_indexdef(i.indexrelid, 1, false) = quote_ident('email')) THEN
    RAISE EXCEPTION 'EVOLVE_INDEX %', 'evolve_ix_01_idx';
  END IF;
END
$evolve$;
SELECT EXISTS (SELECT 1 FROM pg_index WHERE indexrelid = to_regclass('"evolve_ix_01_idx"') AND NOT indisvalid) AS invalid \gset evolve_
\if :evolve_invalid
DROP INDEX CONCURRENTLY IF EXISTS "evolve_ix_01_idx";
\endif
CREATE INDEX CONCURRENTLY IF NOT EXISTS "evolve_ix_01_idx" ON "users" ("email");
DO $evolve$
BEGIN
  IF NOT EXISTS (SELECT 1 FROM pg_catalog.pg_index i JOIN pg_catalog.pg_class c ON c.oid = i.indexrelid JOIN pg_catalog.pg_am a ON a.oid = c.relam JOIN pg_catalog.pg_attribute v ON v.attrelid = i.indrelid AND v.attnum = i.indkey[0] JOIN pg_catalog.pg_opclass o ON o.oid = i.indclass[0] WHERE i.indexrelid = to_regclass('"evolve_ix_01_idx"') AND i.indrelid = to_regclass('"users"') AND a.amname = 'btree' AND i.indnkeyatts = 1 AND i.indnatts = 1 AND NOT i.indisunique AND NOT i.indisexclusion AND i.indexprs IS NULL AND i.indpred IS NULL AND i.indoption[0] = 0 AND i.indcollation[0] = v.attcollation AND o.opcdefault AND o.opcmethod = c.relam AND o.opcintype = v.atttypid AND pg_get_indexdef(i.indexrelid, 1, false) = quote_ident('email') AND i.indisvalid AND i.indisready AND i.indislive) THEN
    RAISE EXCEPTION 'EVOLVE_INDEX %', 'evolve_ix_01_idx';
  END IF;
END
$evolve$;
INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (current_setting('evolve.tenant'), 'ix', 1);
\endif

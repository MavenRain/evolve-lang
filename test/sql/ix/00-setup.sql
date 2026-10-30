-- evolve 1; source fnv1a64 62d26062b87272c6; stage 0 of 1: setup
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
CREATE SCHEMA IF NOT EXISTS evolve;
CREATE TABLE IF NOT EXISTS evolve.progress (
  tenant text NOT NULL,
  protocol text NOT NULL,
  stage integer NOT NULL,
  done_at timestamptz NOT NULL DEFAULT now(),
  PRIMARY KEY (tenant, protocol, stage)
);
CREATE OR REPLACE FUNCTION evolve.stage_done(p text, s integer) RETURNS boolean
LANGUAGE sql STABLE AS $evolve$
  SELECT EXISTS (SELECT 1 FROM evolve.progress
    WHERE tenant = current_setting('evolve.tenant') AND protocol = p AND stage = s)
$evolve$;
CREATE OR REPLACE FUNCTION evolve.ddl(ddl_text text) RETURNS void
LANGUAGE plpgsql AS $evolve$
DECLARE
  tries integer := 1;
BEGIN
  LOOP
    BEGIN
      EXECUTE ddl_text;
      RETURN;
    EXCEPTION WHEN SQLSTATE '55P03' THEN
      IF tries >= 5 THEN
        RAISE;
      END IF;
    END;
    tries := tries + 1;
    PERFORM pg_sleep(1);
  END LOOP;
END
$evolve$;
CREATE TABLE IF NOT EXISTS evolve.tenants (tenant text PRIMARY KEY);
CREATE TABLE IF NOT EXISTS evolve.backfill (
  tenant text NOT NULL,
  protocol text NOT NULL,
  stage integer NOT NULL,
  next_key text,
  done boolean NOT NULL DEFAULT false,
  PRIMARY KEY (tenant, protocol, stage)
);
CREATE TABLE IF NOT EXISTS evolve.app_versions (
  app text PRIMARY KEY,
  live boolean NOT NULL,
  changed_at timestamptz NOT NULL DEFAULT now(),
  seen_at timestamptz
);
INSERT INTO evolve.tenants (tenant) VALUES (:'tenant') ON CONFLICT DO NOTHING;
INSERT INTO evolve.progress (tenant, protocol, stage) VALUES (:'tenant', 'ix', 0) ON CONFLICT DO NOTHING;
COMMIT;

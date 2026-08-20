-- Mini-DWN Initial Schema
CREATE EXTENSION IF NOT EXISTS pgcrypto;
CREATE TABLE IF NOT EXISTS dwn_tenants (
    did TEXT PRIMARY KEY,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
CREATE TABLE IF NOT EXISTS dwn_records (
    record_id TEXT PRIMARY KEY,
    target_did TEXT NOT NULL REFERENCES dwn_tenants(did) ON DELETE CASCADE,
    owner_did TEXT NOT NULL,
    schema TEXT,
    data_format TEXT NOT NULL,
    protocol TEXT,
    protocol_path TEXT,
    recipient TEXT,
    published BOOLEAN DEFAULT FALSE,
    date_created TIMESTAMPTZ NOT NULL,
    date_modified TIMESTAMPTZ NOT NULL,
    deleted BOOLEAN DEFAULT FALSE,
    metadata JSONB NOT NULL DEFAULT '{}'::jsonb,
    data_cid TEXT,
    data_size BIGINT DEFAULT 0
);
CREATE TABLE IF NOT EXISTS dwn_record_data (
    record_id TEXT PRIMARY KEY REFERENCES dwn_records(record_id) ON DELETE CASCADE,
    data BYTEA NOT NULL
);
CREATE TABLE IF NOT EXISTS dwn_events (
    event_id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    target_did TEXT NOT NULL,
    record_id TEXT NOT NULL,
    event_type TEXT NOT NULL,
    timestamp TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    metadata JSONB NOT NULL DEFAULT '{}'::jsonb
);
CREATE INDEX IF NOT EXISTS idx_records_target_did ON dwn_records(target_did);
CREATE INDEX IF NOT EXISTS idx_records_owner_did ON dwn_records(owner_did);
CREATE INDEX IF NOT EXISTS idx_records_schema ON dwn_records(schema);
CREATE INDEX IF NOT EXISTS idx_records_protocol ON dwn_records(protocol);
CREATE INDEX IF NOT EXISTS idx_records_date_created ON dwn_records(date_created DESC);
CREATE INDEX IF NOT EXISTS idx_events_target_did ON dwn_events(target_did);
INSERT INTO dwn_tenants (did) VALUES ('did:example:123') ON CONFLICT DO NOTHING;

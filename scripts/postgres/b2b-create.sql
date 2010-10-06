INSERT INTO version (table_name, table_version) values ('b2b_entities','1');
CREATE TABLE b2b_entities (
    id SERIAL PRIMARY KEY NOT NULL,
    type INTEGER NOT NULL,
    state INTEGER NOT NULL,
    ruri VARCHAR(128),
    from_uri VARCHAR(128) NOT NULL,
    to_uri VARCHAR(128) NOT NULL,
    from_dname VARCHAR(64),
    to_dname VARCHAR(64),
    tag0 VARCHAR(64) NOT NULL,
    tag1 VARCHAR(64),
    callid VARCHAR(64) NOT NULL,
    cseq0 INTEGER NOT NULL,
    cseq1 INTEGER,
    contact0 VARCHAR(128) NOT NULL,
    contact1 VARCHAR(128),
    route0 TEXT,
    route1 TEXT,
    sockinfo_srv VARCHAR(64),
    param VARCHAR(128) NOT NULL,
    lm INTEGER NOT NULL,
    lrc INTEGER,
    lic INTEGER,
    leg_cseq INTEGER,
    leg_route TEXT,
    leg_tag VARCHAR(64),
    leg_contact VARCHAR(128),
    leg_sockinfo VARCHAR(128),
    CONSTRAINT b2b_entities_b2b_entities_idx UNIQUE (type, tag0, tag1, callid)
);

INSERT INTO version (table_name, table_version) values ('b2b_logic','1');
CREATE TABLE b2b_logic (
    id SERIAL PRIMARY KEY NOT NULL,
    si_key VARCHAR(64) NOT NULL,
    scenario VARCHAR(64) NOT NULL,
    sstate INTEGER,
    next_sstate INTEGER,
    sparam0 VARCHAR(64) NOT NULL,
    sparam1 VARCHAR(64) NOT NULL,
    sparam2 VARCHAR(64) NOT NULL,
    sparam3 VARCHAR(64) NOT NULL,
    sparam4 VARCHAR(64) NOT NULL,
    sdp TEXT NOT NULL,
    e1_type INTEGER,
    e1_sid VARCHAR(64) NOT NULL,
    e1_from VARCHAR(128) NOT NULL,
    e1_to VARCHAR(128) NOT NULL,
    e1_key VARCHAR(64) NOT NULL,
    e2_type INTEGER,
    e2_sid VARCHAR(64) NOT NULL,
    e2_from VARCHAR(128) NOT NULL,
    e2_to VARCHAR(128) NOT NULL,
    e2_key VARCHAR(64) NOT NULL,
    e3_type INTEGER,
    e3_sid VARCHAR(64) NOT NULL,
    e3_from VARCHAR(128) NOT NULL,
    e3_to VARCHAR(128) NOT NULL,
    e3_key VARCHAR(64) NOT NULL,
    CONSTRAINT b2b_logic_b2b_logic_idx UNIQUE (si_key)
);

--on commit

    CREATE LOCAL TEMP TABLE mdt_on_commit (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) ON COMMIT PRESERVE ROWS
    DISTRIBUTED RANDOMLY;

--on commit

    CREATE LOCAL TEMP TABLE mdt_on_commit1 (
    text_col text,
    bigint_col bigint,
    char_vary_col character varying(30),
    numeric_col numeric
    ) ON COMMIT DELETE ROWS
    DISTRIBUTED RANDOMLY;

drop table mdt_on_commit;
drop table mdt_on_commit1;


# Test for lack of unexpected errors when an AO table is dropped while its
# auxiliary is being vacuumed.
#
setup
{
	CREATE TABLE ao_vac_conc (a INT) WITH (appendonly=true);
    INSERT INTO ao_vac_conc SELECT j FROM generate_series(1, 100)j;
}

teardown
{
	DROP TABLE IF EXISTS ao_vac_conc;
}

session s1
step begin
{
	BEGIN;
}
step drop_and_commit
{
    DROP TABLE ao_vac_conc;
	COMMIT;
}
step begin_and_drop
{
    BEGIN;
    DROP TABLE ao_vac_conc;
}
step commit
{
	COMMIT;
}

session s2
step vac_specified          { VACUUM AO_AUX_ONLY ao_vac_conc; }
step vac_all_aux_tables		{ VACUUM AO_AUX_ONLY; }
step vac_all_tables 		{ VACUUM; }

permutation begin_and_drop vac_specified commit
permutation begin_and_drop vac_all_aux_tables commit
permutation begin_and_drop vac_all_tables commit
permutation begin vac_specified drop_and_commit
permutation begin vac_all_aux_tables drop_and_commit
permutation begin vac_all_tables drop_and_commit

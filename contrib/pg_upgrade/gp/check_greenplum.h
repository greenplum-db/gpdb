#ifndef GPDB_CHECK_GREENPLUM_H
#define GPDB_CHECK_GREENPLUM_H

/*
 * contrib/pg_upgrade/gp/check_greenplum.h
 *
 * Declaration of an interface function to conduct Greenplum-specific pg_upgrade
 * checks
 */


/*
 * Conduct all greenplum checks, defined in GP_CHECKS_LIST
 *
 * This function should be executed after all PostgreSQL checks. The order of the checks should not matter.
 */
extern void check_greenplum(void);


#endif //GPDB_CHECK_GREENPLUM_H

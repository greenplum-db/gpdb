/*
 *
 *  check_greenplum.h
 *
 *	Copyright (c) 2019-Present Pivotal Software, Inc
 *
 */


#ifndef GPDB_CHECK_GREENPLUM_H
#define GPDB_CHECK_GREENPLUM_H

/*
 * contrib/pg_upgrade/gp/check_greenplum.h
 *
 * Declaration of an interface function to conduct Greenplum-specific pg_upgrade
 * checks
 */


/*
 * Conduct all greenplum checks
 *
 * This function should be executed after all PostgreSQL checks. The order of the checks should not matter.
 */
extern void check_greenplum(void);


#endif /* GPDB_CHECK_GREENPLUM_H */

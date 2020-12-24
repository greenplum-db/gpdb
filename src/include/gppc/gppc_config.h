/* ----------------------------------------------------------------------- 
 *
 * gppc_config.h
 *
 * Contains information that comes from build target database
 *
 * Portions Copyright (c) 2010-Present, VMware, Inc. or its affiliates
 *
 * ---------------------------------------------------------------------*/
#ifndef GPPC_CONFIG_H

#ifndef BUILD_GPPC_C
/**
 * \brief Indicates database version in numeric.
 *
 * The number consists of the first number as ten thousands,
 * the second number as hundreds and the third number.  For example,
 * if the backend is 4.2.1, the number is 40201
 */
#define GP_VERSION_NUM 70000
#endif

/**
 * \brief Indicates GPPC version in numeric.
 *
 * The number consists of major * 100 + minor.
 */
#define GPPC_VERSION_NUM 102

#endif  /* GPPC_CONFIG_H */

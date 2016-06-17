//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2016 Pivotal Software, Inc.
//
//	@filename:
//		codegen_wrapper.h
//
//	@doc:
//		C wrappers for initialization of code generation.
//
//---------------------------------------------------------------------------
#ifndef CODEGEN_WRAPPER_H_
#define CODEGEN_WRAPPER_H_

#include "pg_config.h"
#include "c.h"

#ifndef __cplusplus
#include "postgres.h"
#else
typedef int64 Datum;
#endif

/*
 * Code that needs to be shared irrespective of whether USE_CODEGEN is enabled or not.
 */
struct TupleTableSlot;
struct ProjectionInfo;
struct List;
struct ExprContext;

typedef void (*ExecVariableListFn) (struct ProjectionInfo *projInfo, Datum *values, bool *isnull);
typedef bool (*ExecQualFn) (struct List *qual, struct ExprContext *econtext, bool resultForNull);

#ifndef USE_CODEGEN

#define InitCodegen();
#define CodeGeneratorManagerCreate(module_name) NULL
#define CodeGeneratorManagerGenerateCode(manager);
#define CodeGeneratorManagerPrepareGeneratedFunctions(manager) 1
#define CodeGeneratorManagerNotifyParameterChange(manager) 1
#define CodeGeneratorManagerDestroy(manager);
#define GetActiveCodeGeneratorManager() NULL
#define SetActiveCodeGeneratorManager(manager);

#define START_CODE_GENERATOR_MANAGER(newManager)
#define END_CODE_GENERATOR_MANAGER()

#define init_codegen()

#define call_ExecVariableList(projInfo, values, isnull) ExecVariableList(projInfo, values, isnull)
#define enroll_ExecVariableList_codegen(regular_func, ptr_to_chosen_func, proj_info, slot)

#define call_ExecQual(planstate, qual, econtext, resultForNull) ExecQual(qual, econtext, resultForNull)
#define enroll_ExecQual_codegen(regular_func, ptr_to_chosen_func, planstate)
#else

/*
 * @brief Life span of Code generator instance
 *
 * @note Each code generator is responsible to generate code for one specific function.
 *       Each generated function has a life span to indicate to the manager about when to
 *       invalidate and regenerate this function. The enroller is responsible to know
 *       how long a generated code should be valid.
 *
 *
 */
typedef enum CodegenFuncLifespan
{
	// Does not depend on parameter changes
	CodegenFuncLifespan_Parameter_Invariant,
	// Has to be regenerated as the parameter changes
	CodegenFuncLifespan_Parameter_Variant
} CodegenFuncLifespan;


#ifdef __cplusplus
extern "C" {
#endif

/*
 * Forward extern declaration of code generated functions if code gen is enabled
 */
extern void ExecVariableList(struct ProjectionInfo *projInfo, Datum *values, bool *isnull);
extern bool ExecQual(struct List *qual, struct ExprContext *econtext, bool resultForNull);

/*
 * Do one-time global initialization of LLVM library. Returns 1
 * on success, 0 on error.
 */
unsigned int
InitCodegen();

/*
 * Creates a manager for an operator
 */
void*
CodeGeneratorManagerCreate(const char* module_name);

/*
 * Calls all the registered CodegenInterface to generate code
 */
unsigned int
CodeGeneratorManagerGenerateCode(void* manager);

/*
 * Compiles and prepares all the Codegen function pointers. Returns
 * number of successfully generated functions
 */
unsigned int
CodeGeneratorManagerPrepareGeneratedFunctions(void* manager);

/*
 * Notifies a manager that the underlying operator has a parameter change
 */
unsigned int
CodeGeneratorManagerNotifyParameterChange(void* manager);

/*
 * Destroys a manager for an operator
 */
void
CodeGeneratorManagerDestroy(void* manager);

/*
 * Get the active code generator manager
 */
void*
GetActiveCodeGeneratorManager();

/*
 * Set the active code generator manager
 */
void
SetActiveCodeGeneratorManager(void* manager);

/*
 * returns the pointer to the ExecVariableList
 */
void*
ExecVariableListCodegenEnroll(ExecVariableListFn regular_func_ptr,
                              ExecVariableListFn* ptr_to_regular_func_ptr,
                              struct ProjectionInfo* proj_info,
                              struct TupleTableSlot* slot);

/*
 * returns the pointer to the ExecQual
 */
void*
ExecQualCodegenEnroll(ExecQualFn regular_func_ptr,
                              ExecQualFn* ptr_to_regular_func_ptr,
                              struct PlanState *planstate);

#ifdef __cplusplus
}  // extern "C"
#endif

/*
 * START_CODE_GENERATOR_MANAGER would switch to the specified code generator manager,
 * saving the oldCodeGeneratorManager. Must be paired with END_CODE_GENERATOR_MANAGER
 */
#define START_CODE_GENERATOR_MANAGER(newManager)  \
	do { \
	  void *oldManager = NULL; \
	  if (codegen) { \
	    Assert(newManager != NULL); \
	    oldManager = GetActiveCodeGeneratorManager(); \
	    SetActiveCodeGeneratorManager(newManager);\
	  } \
/*
 * END_CODE_GENERATOR_MANAGER would restore the previous code generator manager that was
 * active at the time of START_CODE_GENERATOR_MANAGER call
 */
#define END_CODE_GENERATOR_MANAGER()  \
    if (codegen) { \
		  SetActiveCodeGeneratorManager(oldManager);\
    } \
	} while (0);


/*
 * Initialize LLVM library
 */
#define init_codegen() \
	if (init_codegen) { \
			if (InitCodegen() == 0) { \
				ereport(FATAL, \
					(errcode(ERRCODE_INTERNAL_ERROR), \
					errmsg("failed to initialize LLVM library"), \
					errhint("LLVM library for code generation failed " \
						"to initialize. You may wish to disable " \
						"code generation by turning off the " \
						"\"codegen\" GUC."))); \
			} \
		} \

/*
 * Call ExecVariableList using function pointer ExecVariableList_fn.
 * Function pointer may point to regular version or generated function
 */
#define call_ExecVariableList(projInfo, values, isnull) \
		projInfo->ExecVariableList_gen_info.ExecVariableList_fn(projInfo, values, isnull)

/*
 * Call ExecQual using function pointer ExecQual_fn.
 * Function pointer may point to regular version or generated function
 */
#define call_ExecQual(ps, qual, econtext, resultForNull) \
     ps.ExecQual_gen_info.ExecQual_fn(qual, econtext, resultForNull)

/*
 * Enrollment macros
 * The enrollment process also ensures that the generated function pointer
 * is set to the regular version initially
 */
#define enroll_ExecVariableList_codegen(regular_func, ptr_to_regular_func_ptr, proj_info, slot) \
		proj_info->ExecVariableList_gen_info.code_generator = ExecVariableListCodegenEnroll( \
				regular_func, ptr_to_regular_func_ptr, proj_info, slot); \
		Assert(proj_info->ExecVariableList_gen_info.ExecVariableList_fn == regular_func); \

#define enroll_ExecQual_codegen(regular_func, ptr_to_regular_func_ptr, planstate) \
		planstate->ExecQual_gen_info.code_generator = ExecQualCodegenEnroll( \
        regular_func, ptr_to_regular_func_ptr, planstate); \
    Assert(planstate->ExecQual_gen_info.ExecQual_fn == regular_func); \

#endif //USE_CODEGEN

#endif  // CODEGEN_WRAPPER_H_

#include "postgres.h"
#include "funcapi.h"
#include "miscadmin.h"

#include "pinv.h"

#include "catalog/pg_type.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
 * gp_Matrix
 *
 * A simple Matrix datatype.
 */
typedef struct
{
	int			rows;
	int			cols;
	double	   *arr;
} gp_Matrix;

/*
 * Evaluating the following condition will not cause an integer overflow and is
 * equivalent to:
 *
 *     sizeof(float8) * Max(m,n)^2 <= MaxAllocSize
 * and sizeof(integer) * 8 * m * n <= MaxAllocSize
 *
 * It is a precondition before palloc'ing the arrays that LAPACK's dgesdd needs.
 */
#define IS_FEASIBLE_MATRIX_SIZE(m,n) ( \
	(MaxAllocSize / sizeof(double) / Max(m,n) / Max(m,n) >= 1) && \
	(MaxAllocSize / sizeof(long int) / 8 / m / n >= 1) )

/*
 * Allocate the array to store a matrix.
 */
static void
allocMatrix(gp_Matrix *m, int rows, int cols)
{
	Assert(IS_FEASIBLE_MATRIX_SIZE(rows, cols));
	m->arr = palloc(sizeof(double) * (rows * cols));
	m->rows = rows;
	m->cols = cols;
}

/* Access a particular element in a matrix */
#define MATRIX_ELEM(m, row, col) ( \
		AssertMacro((row) < (m)->rows), \
		AssertMacro((col) < (m)->cols), \
		(col) * (m)->rows + (row) \
	)
#define ACCESS_ELEM(m, row, col) (m)->arr[MATRIX_ELEM(m, row, col)]

/*
 * These are convenience macros to access the specific matrixes used.
 * Makes the code much shorter and more readable than spelling out the
 * ACCESS_ELEM macro every time (even if we renamed ACCESS_ELEM to something
 * shorter).
 */
#define Ax(i, j) ACCESS_ELEM(Amatrix_p, i, j)
#define Sx(i, j) ACCESS_ELEM(Smatrix_p, i, j)
#define Ux(i, j) ACCESS_ELEM(Umatrix_p, i, j)
#define Vx(i, j) ACCESS_ELEM(Vmatrix_p, i, j)

static void
gp_svd(gp_Matrix *Arg, double **Sout, gp_Matrix *Uout, gp_Matrix *Vout);

Datum 
pseudoinverse(PG_FUNCTION_ARGS)
{
	/*
	 * A note on types: PostgreSQL array dimensions are of type int. See, e.g.,
	 * the macro definition ARR_DIMS
	 */
	int         rows, columns;
	float8     *A, *Aplus;
	ArrayType  *A_PG, *Aplus_PG;
	int lbs[2], dims[2];

	/* 
	 * Perform all the error checking needed to ensure that no one is
	 * trying to call this in some sort of crazy way. 
	 */
	if (PG_NARGS() != 1)
	{
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pseudoinverse called with %d arguments", 
						PG_NARGS())));
	}
	if (PG_ARGISNULL(0))
		PG_RETURN_NULL();	

	A_PG = PG_GETARG_ARRAYTYPE_P(0);

	if (ARR_ELEMTYPE(A_PG) != FLOAT8OID) 
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pseudoinverse only defined over float8[]")));
	if (ARR_NDIM(A_PG) != 2)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("pseudoinverse only defined over 2 dimensional arrays"))
			);
	if (ARR_NULLBITMAP(A_PG)) 
		ereport(ERROR,
				(errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
				 errmsg("null array element not allowed in this context")));

	/* Extract rows, columns, and data */
	rows = ARR_DIMS(A_PG)[0];
	columns = ARR_DIMS(A_PG)[1];
	A = (float8 *) ARR_DATA_PTR(A_PG);


	/*  Allocate a PG array for the result, "Aplus" is A+, the pseudo inverse of A */
	lbs[0] = 1; lbs[1] = 1;
	dims[0] = columns; dims[1] = rows;
	Aplus_PG = construct_md_array(NULL, NULL, 2, dims, lbs, FLOAT8OID, 
						   sizeof(float8), true, 'd');

	Aplus = (float8 *) ARR_DATA_PTR(Aplus_PG);

	pinv(rows,columns,A,Aplus);

	PG_RETURN_ARRAYTYPE_P(Aplus_PG);
}

/*

	float8[] *pseudoinverse(float8[])

	Compute the pseudo inverse of matrix A

	Author:  Luke Lonergan
	Date:    5/31/08
	License: Use pfreely

	We use the approach from here:
	   http://en.wikipedia.org/wiki/Moore-Penrose_pseudoinverse#Finding_the_\
pseudoinverse_of_a_matrix

	Synopsis:
	   A computationally simpler and more accurate way to get the pseudoinverse 
	   is by using the singular value decomposition.[1][5][6] If A = U Σ V* is 
	   the singular value decomposition of A, then A+ = V Σ+ U* . For a diagonal
	   matrix such as Σ, we get the pseudoinverse by taking the reciprocal of 
	   each non-zero element on the diagonal, and leaving the zeros in place. 
	   In numerical computation, only elements larger than some small tolerance 
	   are taken to be nonzero, and the others are replaced by zeros. For 
	   example, in the Matlab function pinv, the tolerance is taken to be
	   t = ε•max(rows,columns)•max(Σ), where ε is the machine epsilon.

	Input:  the matrix A with "rows" rows and "columns" columns, in column 
	        values consecutive order
	Output: the matrix A+ with "columns" rows and "rows" columns, the 
	        Moore-Penrose pseudo inverse of A

    The approach is summarized:
    - Compute the SVD (diagonalization) of A, yielding the U, S and V 
      factors of A
    - Compute the pseudo inverse A+ = U x S+ x Vt

	S+ is the pseudo inverse of the diagonal matrix S, which is gained by 
	inverting the non zero diagonals 

	Vt is the transpose of V

	Note that there is some fancy index rework in this implementation to deal 
	with the row values consecutive order used by the FORTRAN dgesdd_ routine.
	
	The return value of pinv is the condition number of the matrix A, namely
	the largest singular value divided by the smallest singular value.
*/
void
pinv(int rows, int columns, double *A, double *Aplus)
{
	int			minmn;
	int			i,
				j,
				k,
				ii;
	gp_Matrix	Amatrix;
	gp_Matrix  *Amatrix_p = &Amatrix;
	float8		epsilon,
				tolerance;
	double	   *S;
	gp_Matrix	Umatrix,
				Vmatrix;
	gp_Matrix  *Umatrix_p = &Umatrix,
			   *Vmatrix_p = &Vmatrix;
	double	   *Splus;

	if (!IS_FEASIBLE_MATRIX_SIZE(rows,columns))
		ereport(ERROR, 
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 errmsg("pseudoinverse: matrix dimensions too large")));

	/*
	 * Convert the input array to row order, as expected by gp_svd(). Note
	 * that gp_svd() is destructive to the entry array, so we'd need to make
	 * this copy anyway.
	 */
	allocMatrix(&Amatrix, rows, columns);
	for (i = 0; i < rows; i++)
	{
		for (j = 0; j < columns; j++)
		{
			Ax(i, j) = A[i * columns + j];
		}
	}

	/*
	 * The factors of A: S, U and Vt
	 * U, Sdiag and Vt are the factors of the pseudo inverse of A, the
	 * components of the singular value decomposition of A
	 */
	gp_svd(&Amatrix, &S, &Umatrix, &Vmatrix);

	minmn = Min(rows, columns); /* The dimension of S is min(rows,columns) */

#ifdef USE_ASSERT_CHECKING
	/*
	 * gp_svd is supposed to return singular values in descending order.
	 * It's not terribly important to check this ATM, but would be if we
	 * used an external SVD routine, e.g. from LAPACK.
	 */
	for (i = 1; i < minmn; i++)
		Assert(S[i] <= S[i-1]);
#endif

#ifdef DEBUG_SVD
	{
		for (i = 0; i < rows; i++)
			elog(WARNING, "S[%d] = %f", i, S[i]);

		for (i = 0; i < rows; i++)
		{
			for (j = 0; j < minmn; j++)
				elog(WARNING, "U[%d, %d] = %f", i, j, Ux(i, j));
		}

		for (i = 0; i < columns; i++)
		{
			for (j = 0; j < columns; j++)
				elog(WARNING, "V[%d, %d] = %f", i, j, Vx(i, j));
		}

	}
#endif

	/*
	 * Calculate the tolerance for "zero" values in the SVD
	 *    t = ε•max(rows,columns)•max(Σ)
	 *  (Need to multiply tolerance by max of the eigenvalues when they're
	 *   available)
	 */
	epsilon = pow(2,1-56);
	tolerance = epsilon * Max(rows,columns);

	/* Use the max of the eigenvalues to normalize the zero tolerance */
	tolerance *= S[0];

	/*
	 * Calculate the pseudo inverse of the eigenvalue matrix, Splus
	 * Use a tolerance to evaluate elements that are close to zero
	 */
	Splus = (float8 *) palloc(sizeof(float8)*minmn);
	for (ii = 0; ii < minmn; ii++)
	{
		Splus[ii] = (S[ii] > tolerance) ? (1.0 / S[ii]) : 0.0;
	}

	for (i = 0; i < columns; i++)
	{
		for (j = 0; j < rows; j++)
		{
			Aplus[i * rows + j] = 0.0;
			for (k = 0; k < minmn; k++)
				Aplus[i * rows + j] += Vx(i, k) * Splus[k] * Ux(j, k);
		}
	}

	pfree(Splus);
	pfree(Umatrix.arr);
	pfree(Vmatrix.arr);
	pfree(S);
}

/*------------
 *	gp_svd - Calculate Singular Value Decomposition of a matrix.
 *
 * For an m-by-n matrix A with m >= n, the singular value decomposition is
 * an m-by-n orthogonal matrix U, an n-by-n diagonal matrix S, and an n-by-n
 * orthogonal matrix V so that A = U*S*V'.  The singular values,
 * sigma[k] = S[k, k], are ordered so that
 * sigma[0] >= sigma[1] >= ... >= sigma[n-1].
 * The singular value decompostion always exists, so this operation will
 * never fail.
 *
 * Amatrix_p  - input matrix (modified destructively!)
 * Sout		  - Result Sigma values. palloc'd vector with length Min(rows, cols)
 * Umatrix_p  - Result matrix U
 * Vmatrix_p  - Result matrix V
 *
 *
 *
 * This function has been adapted from JAMA, version 1.0.3. JAMA is public
 * domain linear algebra package for Java. The copyright notice on the JAMA
 * website says:
 *
 * JAMA's initial design, as well as this reference implementation, was
 * developed by
 *
 * Joe Hicklin
 * Cleve Moler
 * Peter Webb	... from The MathWorks		Ronald F. Boisvert
 * Bruce Miller
 * Roldan Pozo
 * Karin Remington	... from NIST
 *
 * Copyright Notice
 *
 * This software is a cooperative product of The MathWorks and the
 * National Institute of Standards and Technology (NIST) which has been
 * released to the public domain. Neither The MathWorks nor NIST assumes
 * any responsibility whatsoever for its use by other parties, and makes
 * no guarantees, expressed or implied, about its quality, reliability, or
 * any other characteristic.

 *------------
 */
static void
gp_svd(gp_Matrix *Amatrix_p, double **Sout, gp_Matrix *Umatrix_p, gp_Matrix *Vmatrix_p)
{
	int			m = Amatrix_p->rows;
	int			n = Amatrix_p->cols;
	int			nu = Min(m, n);
	/* Array for internal storage of singular values. */
	double	   *s;
	double	   *e;
	double	   *work;
	bool		wantu = true;
	bool		wantv = true;

	/* Reduce A to bidiagonal form, storing the diagonal elements */
	/* in s and the super-diagonal elements in e. */

	int			nct = Min(m - 1, n);
	int			nrt = Max(0, Min(n - 2, m));
	int			k;

	/*
	 * Apparently the failing cases are only a proper subset of (m<n), so
	 * let's not throw error.  Correct fix to come later?
	 */
#ifdef BROKEN
	if (m<n)
	{
		throw new IllegalArgumentException("Jama SVD only works for m >= n");
	}
#endif

	e = palloc(sizeof(double) * n);
	s = palloc(sizeof(double) * Min(m + 1, n));
	allocMatrix(Umatrix_p, m, nu);
	allocMatrix(Vmatrix_p, n, n);

	work = palloc(sizeof(double) * m);

	for (k = 0; k < m*nu; k++)
		Umatrix_p->arr[k] = 0;
	for (k = 0; k < Min(m + 1, n); k++)
		s[k] = 0;

	for (k = 0; k < Max(nct, nrt); k++)
	{
		if (k < nct)
		{
			/*
			 * Compute the transformation for the k-th column and
			 * place the k-th diagonal in s[k].
			 * Compute 2-norm of k-th column without under/overflow.
			 */
			s[k] = 0;
			for (int i = k; i < m; i++)
			{
				s[k] = hypot(s[k], Ax(i, k));
			}
			if (s[k] != 0.0)
			{
				if (Ax(k, k) < 0.0)
				{
					s[k] = -s[k];
				}
				for (int i = k; i < m; i++)
				{
					Ax(i, k) /= s[k];
				}
				Ax(k, k) += 1.0;
			}
			s[k] = -s[k];
		}
		for (int j = k + 1; j < n; j++)
		{
			if ((k < nct) & (s[k] != 0.0))
			{
				/* Apply the transformation. */

				double		t = 0;

				for (int i = k; i < m; i++)
				{
					t += Ax(i, k) * Ax(i, j);
				}
				t = -t / Ax(k, k);
				for (int i = k; i < m; i++)
				{
					Ax(i, j) += t * Ax(i, k);
				}
			}

			/* Place the k-th row of A into e for the */
			/* subsequent calculation of the row transformation. */

			e[j] = Ax(k, j);
		}
		if (wantu & (k < nct))
		{
			/*
			 * Place the transformation in U for subsequent back
			 * multiplication.
			 */
			for (int i = k; i < m; i++)
			{
				Ux(i, k) = Ax(i, k);
			}
		}
		if (k < nrt)
		{

			/* Compute the k-th row transformation and place the */
			/* k-th super-diagonal in e[k]. */
			/* Compute 2-norm without under/overflow. */
			e[k] = 0;
			for (int i = k + 1; i < n; i++)
			{
				e[k] = hypot(e[k], e[i]);
			}
			if (e[k] != 0.0)
			{
				if (e[k + 1] < 0.0)
				{
					e[k] = -e[k];
				}
				for (int i = k + 1; i < n; i++)
				{
					e[i] /= e[k];
				}
				e[k + 1] += 1.0;
			}
			e[k] = -e[k];
			if ((k + 1 < m) & (e[k] != 0.0))
			{
				/* Apply the transformation. */

				for (int i = k + 1; i < m; i++)
				{
					work[i] = 0.0;
				}
				for (int j = k + 1; j < n; j++)
				{
					for (int i = k + 1; i < m; i++)
					{
						work[i] += e[j] * Ax(i, j);
					}
				}
				for (int j = k + 1; j < n; j++)
				{
					double		t = -e[j] / e[k + 1];

					for (int i = k + 1; i < m; i++)
					{
						Ax(i, j) += t * work[i];
					}
				}
			}
			if (wantv)
			{
				/*
				 * Place the transformation in V for subsequent back
				 * multiplication.
				 */
				for (int i = k + 1; i < n; i++)
				{
					Vx(i, k) = e[i];
				}
			}
		}
	}

	/* Set up the final bidiagonal matrix or order p. */

	int			p = Min(n, m + 1);

	if (nct < n)
	{
		s[nct] = Ax(nct, nct);
	}
	if (m < p)
	{
		s[p - 1] = 0.0;
	}
	if (nrt + 1 < p)
	{
		e[nrt] = Ax(nrt, p - 1);
	}
	e[p - 1] = 0.0;

	/* If required, generate U. */

	if (wantu)
	{
		for (int j = nct; j < nu; j++)
		{
			for (int i = 0; i < m; i++)
			{
				Ux(i, j) = 0.0;
			}
			Ux(j, j) = 1.0;
		}
		for (int k = nct - 1; k >= 0; k--)
		{
			if (s[k] != 0.0)
			{
				for (int j = k + 1; j < nu; j++)
				{
					double		t = 0;

					for (int i = k; i < m; i++)
					{
						t += Ux(i, k) * Ux(i, j);
					}
					t = -t / Ux(k, k);
					for (int i = k; i < m; i++)
					{
						Ux(i, j) += t * Ux(i, k);
					}
				}
				for (int i = k; i < m; i++)
				{
					Ux(i, k) = -Ux(i, k);
				}
				Ux(k, k) = 1.0 + Ux(k, k);
				for (int i = 0; i < k - 1; i++)
				{
					Ux(i, k) = 0.0;
				}
			}
			else
			{
				for (int i = 0; i < m; i++)
				{
					Ux(i, k) = 0.0;
				}
				Ux(k, k) = 1.0;
			}
		}
	}

	/* If required, generate V. */

	if (wantv)
	{
		for (int k = n - 1; k >= 0; k--)
		{
			if ((k < nrt) & (e[k] != 0.0))
			{
				for (int j = k + 1; j < n; j++)
				{
					double		t = 0;

					for (int i = k + 1; i < n; i++)
					{
						t += Vx(i, k) * Vx(i, j);
					}
					t = -t / Vx(k + 1, k);
					for (int i = k + 1; i < n; i++)
					{
						Vx(i, j) += t * Vx(i, k);
					}
				}
			}
			for (int i = 0; i < n; i++)
			{
				Vx(i, k) = 0.0;
			}
			Vx(k, k) = 1.0;
		}
	}

	/* Main iteration loop for the singular values. */

	int			pp = p - 1;
	int			iter = 0;
	double		eps = pow(2.0, -52.0);
	double		tiny = pow(2.0, -966.0);

	while (p > 0)
	{
		int			k,
					kase;

		CHECK_FOR_INTERRUPTS();

		/* Here is where a test for too many iterations would go. */

		/* This section of the program inspects for */
		/* negligible elements in the s and e arrays.  On */
		/* completion the variables kase and k are set as follows. */

		/* kase = 1		if s(p) and e[k-1] are negligible and k<p */
		/* kase = 2		if s(k) is negligible and k<p */
		/* kase = 3		if e[k-1] is negligible, k<p, and */
		/* s(k), ..., s(p) are not negligible (qr step). */
		/* kase = 4		if e(p-1) is negligible (convergence). */

		for (k = p - 2; k >= -1; k--)
		{
			if (k == -1)
			{
				break;
			}
			if (fabs(e[k])
				<= tiny + eps * (fabs(s[k]) + fabs(s[k + 1])))
			{
				e[k] = 0.0;
				break;
			}
		}
		if (k == p - 2)
		{
			kase = 4;
		}
		else
		{
			int			ks;

			for (ks = p - 1; ks >= k; ks--)
			{
				if (ks == k)
				{
					break;
				}
				double		t = (ks != p ? fabs(e[ks]) : 0.)
				+ (ks != k + 1 ? fabs(e[ks - 1]) : 0.);

				if (fabs(s[ks]) <= tiny + eps * t)
				{
					s[ks] = 0.0;
					break;
				}
			}
			if (ks == k)
			{
				kase = 3;
			}
			else if (ks == p - 1)
			{
				kase = 1;
			}
			else
			{
				kase = 2;
				k = ks;
			}
		}
		k++;

		/* Perform the task indicated by kase. */

		switch (kase)
		{

				/* Deflate negligible s(p). */

			case 1:
				{
					double		f = e[p - 2];

					e[p - 2] = 0.0;
					for (int j = p - 2; j >= k; j--)
					{
						double		t = hypot(s[j], f);
						double		cs = s[j] / t;
						double		sn = f / t;

						s[j] = t;
						if (j != k)
						{
							f = -sn * e[j - 1];
							e[j - 1] = cs * e[j - 1];
						}
						if (wantv)
						{
							for (int i = 0; i < n; i++)
							{
								t = cs * Vx(i, j) + sn * Vx(i, p - 1);
								Vx(i, p - 1) = -sn * Vx(i, j) + cs * Vx(i, p - 1);
								Vx(i, j) = t;
							}
						}
					}
				}
				break;

				/* Split at negligible s(k). */

			case 2:
				{
					double		f = e[k - 1];

					e[k - 1] = 0.0;
					for (int j = k; j < p; j++)
					{
						double		t = hypot(s[j], f);
						double		cs = s[j] / t;
						double		sn = f / t;

						s[j] = t;
						f = -sn * e[j];
						e[j] = cs * e[j];
						if (wantu)
						{
							for (int i = 0; i < m; i++)
							{
								t = cs * Ux(i, j) + sn * Ux(i, k - 1);
								Ux(i, k - 1) = -sn * Ux(i, j) + cs * Ux(i, k - 1);
								Ux(i, j) = t;
							}
						}
					}
				}
				break;

				/* Perform one qr step. */

			case 3:
				{

					/* Calculate the shift. */

					double		scale = Max(
												 Max(
														  Max(
												 Max(fabs(s[p - 1]),
														  fabs(s[p - 2])),
										fabs(e[p - 2])), fabs(s[k])),
												 fabs(e[k]));
					double		sp = s[p - 1] / scale;
					double		spm1 = s[p - 2] / scale;
					double		epm1 = e[p - 2] / scale;
					double		sk = s[k] / scale;
					double		ek = e[k] / scale;
					double		b = ((spm1 + sp) * (spm1 - sp) + epm1 * epm1) / 2.0;
					double		c = (sp * epm1) * (sp * epm1);
					double		shift = 0.0;

					if ((b != 0.0) | (c != 0.0))
					{
						shift = sqrt(b * b + c);
						if (b < 0.0)
						{
							shift = -shift;
						}
						shift = c / (b + shift);
					}
					double		f = (sk + sp) * (sk - sp) + shift;
					double		g = sk * ek;

					/* Chase zeros. */

					for (int j = k; j < p - 1; j++)
					{
						double		t = hypot(f, g);
						double		cs = f / t;
						double		sn = g / t;

						if (j != k)
						{
							e[j - 1] = t;
						}
						f = cs * s[j] + sn * e[j];
						e[j] = cs * e[j] - sn * s[j];
						g = sn * s[j + 1];
						s[j + 1] = cs * s[j + 1];
						if (wantv)
						{
							for (int i = 0; i < n; i++)
							{
								t = cs * Vx(i, j) + sn * Vx(i, j + 1);
								Vx(i, j + 1) = -sn * Vx(i, j) + cs * Vx(i, j + 1);
								Vx(i, j) = t;
							}
						}
						t = hypot(f, g);
						cs = f / t;
						sn = g / t;
						s[j] = t;
						f = cs * e[j] + sn * s[j + 1];
						s[j + 1] = -sn * e[j] + cs * s[j + 1];
						g = sn * e[j + 1];
						e[j + 1] = cs * e[j + 1];
						if (wantu && (j < m - 1))
						{
							for (int i = 0; i < m; i++)
							{
								t = cs * Ux(i, j) + sn * Ux(i, j + 1);
								Ux(i, j + 1) = -sn * Ux(i, j) + cs * Ux(i, j + 1);
								Ux(i, j) = t;
							}
						}
					}
					e[p - 2] = f;
					iter = iter + 1;
				}
				break;

				/* Convergence. */

			case 4:
				{

					/* Make the singular values positive. */

					if (s[k] <= 0.0)
					{
						s[k] = (s[k] < 0.0 ? -s[k] : 0.0);
						if (wantv)
						{
							for (int i = 0; i <= pp; i++)
							{
								Vx(i, k) = -Vx(i, k);
							}
						}
					}

					/* Order the singular values. */

					while (k < pp)
					{
						if (s[k] >= s[k + 1])
						{
							break;
						}
						double		t = s[k];

						s[k] = s[k + 1];
						s[k + 1] = t;
						if (wantv && (k < n - 1))
						{
							for (int i = 0; i < n; i++)
							{
								t = Vx(i, k + 1);
								Vx(i, k + 1) = Vx(i, k);
								Vx(i, k) = t;
							}
						}
						if (wantu && (k < m - 1))
						{
							for (int i = 0; i < m; i++)
							{
								t = Ux(i, k + 1);
								Ux(i, k + 1) = Ux(i, k);
								Ux(i, k) = t;
							}
						}
						k++;
					}
					iter = 0;
					p--;
				}
				break;
		}
	}

	*Sout = s;
}

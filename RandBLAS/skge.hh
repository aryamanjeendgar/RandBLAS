// Copyright, 2024. See LICENSE for copyright holder information.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// (1) Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// (2) Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// (3) Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
#pragma once

#include "RandBLAS/base.hh"
#include "RandBLAS/exceptions.hh"
#include "RandBLAS/random_gen.hh"
#include "RandBLAS/dense_skops.hh"
#include "RandBLAS/sparse_skops.hh"

#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <string>

#include <cmath>
#include <typeinfo>


namespace RandBLAS::dense {

using RandBLAS::DenseSkOp;
using RandBLAS::fill_dense;

// MARK: LSKGE3

// =============================================================================
/// LSKGE3: Perform a GEMM-like operation
/// @verbatim embed:rst:leading-slashes
/// .. math::
///     \mat(B) = \alpha \cdot \underbrace{\op(\submat(\mtxS))}_{d \times m} \cdot \underbrace{\op(\mat(A))}_{m \times n} + \beta \cdot \underbrace{\mat(B)}_{d \times n},    \tag{$\star$}
/// @endverbatim
/// where \math{\alpha} and \math{\beta} are real scalars, \math{\op(\mtxX)} either returns a matrix \math{X}
/// or its transpose, and \math{S} is a sketching operator that takes Level 3 BLAS effort to apply.
/// 
/// @verbatim embed:rst:leading-slashes
/// What are :math:`\mat(A)` and :math:`\mat(B)`?
///     Their shapes are defined implicitly by :math:`(d, m, n, \opA)`.
///     Their precise contents are determined by :math:`(A, \lda)`, :math:`(B, \ldb)`,
///     and "layout", following the same convention as BLAS.
///
/// What is :math:`\submat(\mtxS)`?
///     Its shape is defined implicitly by :math:`(\opS, d, m)`.
///     If :math:`{\submat(\mtxS)}` is of shape :math:`r \times c`,
///     then it is the :math:`r \times c` submatrix of :math:`{\mtxS}` whose upper-left corner
///     appears at index :math:`(\texttt{ro_s}, \texttt{co_s})` of :math:`{\mtxS}`.
/// @endverbatim
/// @param[in] layout
///     Layout::ColMajor or Layout::RowMajor
///      - Matrix storage for \math{\mat(A)} and \math{\mat(B)}.
///
/// @param[in] opS
///      - If \math{\opS} = NoTrans, then \math{ \op(\submat(\mtxS)) = \submat(\mtxS)}.
///      - If \math{\opS} = Trans, then \math{\op(\submat(\mtxS)) = \submat(\mtxS)^T }.
/// @param[in] opA
///      - If \math{\opA} == NoTrans, then \math{\op(\mat(A)) = \mat(A)}.
///      - If \math{\opA} == Trans, then \math{\op(\mat(A)) = \mat(A)^T}.
/// @param[in] d
///     A nonnegative integer.
///     - The number of rows in \math{\mat(B)}
///     - The number of rows in \math{\op(\mat(S))}.
///
/// @param[in] n
///     A nonnegative integer.
///     - The number of columns in \math{\mat(B)}
///     - The number of columns in \math{\op(\mat(A))}.
///
/// @param[in] m
///     A nonnegative integer.
///     - The number of columns in \math{\op(\submat(\mtxS))}
///     - The number of rows in \math{\op(\mat(A))}.
///
/// @param[in] alpha
///     A real scalar.
///     - If zero, then \math{A} is not accessed.
///
/// @param[in] S
///    A DenseSkOp object.
///    - Defines \math{\submat(\mtxS)}.
///
/// @param[in] ro_s
///     A nonnegative integer.
///     - The rows of \math{\submat(\mtxS)} are a contiguous subset of rows of \math{S}.
///     - The rows of \math{\submat(\mtxS)} start at \math{S[\texttt{ro_s}, :]}.
///
/// @param[in] co_s
///     A nonnnegative integer.
///     - The columns of \math{\submat(\mtxS)} are a contiguous subset of columns of \math{S}.
///     - The columns \math{\submat(\mtxS)} start at \math{S[:,\texttt{co_s}]}. 
///
/// @param[in] A
///     Pointer to a 1D array of real scalars.
///     - Defines \math{\mat(A)}.
///
/// @param[in] lda
///     A nonnegative integer.
///     * Leading dimension of \math{\mat(A)} when reading from \math{A}.
///     * If layout == ColMajor, then
///         @verbatim embed:rst:leading-slashes
///             .. math::
///                 \mat(A)_{ij} = A[i + j \cdot \lda].
///         @endverbatim
///       In this case, \math{\lda} must be \math{\geq} the length of a column in \math{\mat(A)}.
///     * If layout == RowMajor, then
///         @verbatim embed:rst:leading-slashes
///             .. math::
///                 \mat(A)_{ij} = A[i \cdot \lda + j].
///         @endverbatim
///       In this case, \math{\lda} must be \math{\geq} the length of a row in \math{\mat(A)}.
///
/// @param[in] beta
///     A real scalar.
///     - If zero, then \math{B} need not be set on input.
///
/// @param[in, out] B
///    Pointer to 1D array of real scalars.
///    - On entry, defines \math{\mat(B)}
///      on the RIGHT-hand side of \math{(\star)}.
///    - On exit, defines \math{\mat(B)}
///      on the LEFT-hand side of \math{(\star)}.
///
/// @param[in] ldb
///    - Leading dimension of \math{\mat(B)} when reading from \math{B}.
///    - Refer to documentation for \math{\lda} for details. 
///
template <typename T, typename DenseSkOp>
void lskge3(
    blas::Layout layout,
    blas::Op opS,
    blas::Op opA,
    int64_t d, // B is d-by-n
    int64_t n, // op(A) is m-by-n
    int64_t m, // op(S) is d-by-m
    T alpha,
    DenseSkOp &S,
    int64_t ro_s,
    int64_t co_s,
    const T *A,
    int64_t lda,
    T beta,
    T *B,
    int64_t ldb
){
    auto [rows_submat_S, cols_submat_S] = dims_before_op(d, m, opS);
    constexpr bool maybe_denseskop = !std::is_same_v<std::remove_cv_t<DenseSkOp>, BLASFriendlyOperator<T>>;
    if constexpr (maybe_denseskop) {
        if (!S.buff) {
            // DenseSkOp doesn't permit defining a "black box" distribution, so we have to pack the submatrix
            // into an equivalent datastructure ourselves.
            auto submat_S = submatrix_as_blackbox<BLASFriendlyOperator<T>>(S, rows_submat_S, cols_submat_S, ro_s, co_s);
            lskge3(layout, opS, opA, d, n, m, alpha, submat_S, 0, 0, A, lda, beta, B, ldb);
            return;
        } // else, continue with the function as usual.
    }
    randblas_require( S.buff != nullptr );
    randblas_require( S.n_rows >= rows_submat_S + ro_s );
    randblas_require( S.n_cols >= cols_submat_S + co_s );
    auto [rows_A, cols_A] = dims_before_op(m, n, opA);
    if (layout == blas::Layout::ColMajor) {
        randblas_require(lda >= rows_A);
        randblas_require(ldb >= d);
    } else {
        randblas_require(lda >= cols_A);
        randblas_require(ldb >= n);
    }

    auto [pos, lds] = offset_and_ldim(S.layout, S.n_rows, S.n_cols, ro_s, co_s);
    T* S_ptr = &S.buff[pos];
    if (S.layout != layout)
        opS = (opS == blas::Op::NoTrans) ? blas::Op::Trans : blas::Op::NoTrans;

    blas::gemm(layout, opS, opA, d, n, m, alpha, S_ptr, lds, A, lda, beta, B, ldb);
    return;
}

// MARK: RSKGE3

// =============================================================================
/// RSKGE3: Perform a GEMM-like operation
/// @verbatim embed:rst:leading-slashes
/// .. math::
///     \mat(B) = \alpha \cdot \underbrace{\op(\mat(A))}_{m \times n} \cdot \underbrace{\op(\submat(\mtxS))}_{n \times d} + \beta \cdot \underbrace{\mat(B)}_{m \times d},    \tag{$\star$}
/// @endverbatim
/// where \math{\alpha} and \math{\beta} are real scalars, \math{\op(\mtxX)} either returns a matrix \math{X}
/// or its transpose, and \math{S} is a sketching operator that takes Level 3 BLAS effort to apply.
/// 
/// @verbatim embed:rst:leading-slashes
/// What are :math:`\mat(A)` and :math:`\mat(B)`?
///     Their shapes are defined implicitly by :math:`(m, d, n, \opA)`.
///     Their precise contents are determined by :math:`(A, \lda)`, :math:`(B, \ldb)`,
///     and "layout", following the same convention as BLAS.
///
/// What is :math:`\submat(\mtxS)`?
///     Its shape is defined implicitly by :math:`(\opS, n, d)`.
///     If :math:`{\submat(\mtxS)}` is of shape :math:`r \times c`,
///     then it is the :math:`r \times c` submatrix of :math:`{\mtxS}` whose upper-left corner
///     appears at index :math:`(\texttt{ro_s}, \texttt{co_s})` of :math:`{\mtxS}`.
/// @endverbatim
/// @param[in] layout
///     Layout::ColMajor or Layout::RowMajor
///      - Matrix storage for \math{\mat(A)} and \math{\mat(B)}.
///
/// @param[in] opA
///      - If \math{\opA} == NoTrans, then \math{\op(\mat(A)) = \mat(A)}.
///      - If \math{\opA} == Trans, then \math{\op(\mat(A)) = \mat(A)^T}.
///
/// @param[in] opS
///      - If \math{\opS} = NoTrans, then \math{ \op(\submat(\mtxS)) = \submat(\mtxS)}.
///      - If \math{\opS} = Trans, then \math{\op(\submat(\mtxS)) = \submat(\mtxS)^T }.
///
/// @param[in] m
///     A nonnegative integer.
///     - The number of rows in \math{\mat(B)}.
///     - The number of rows in \math{\op(\mat(A))}.
///
/// @param[in] d
///     A nonnegative integer.
///     - The number of columns in \math{\mat(B)}
///     - The number of columns in \math{\op(\mat(S))}.
///
/// @param[in] n
///     A nonnegative integer.
///     - The number of columns in \math{\op(\mat(A))}
///     - The number of rows in \math{\op(\submat(\mtxS))}.
///
/// @param[in] alpha
///     A real scalar.
///     - If zero, then \math{A} is not accessed.
///
/// @param[in] A
///     Pointer to a 1D array of real scalars.
///     - Defines \math{\mat(A)}.
///
/// @param[in] lda
///     A nonnegative integer.
///     * Leading dimension of \math{\mat(A)} when reading from \math{A}.
///     * If layout == ColMajor, then
///         @verbatim embed:rst:leading-slashes
///             .. math::
///                 \mat(A)_{ij} = A[i + j \cdot \lda].
///         @endverbatim
///       In this case, \math{\lda} must be \math{\geq} the length of a column in \math{\mat(A)}.
///     * If layout == RowMajor, then
///         @verbatim embed:rst:leading-slashes
///             .. math::
///                 \mat(A)_{ij} = A[i \cdot \lda + j].
///         @endverbatim
///       In this case, \math{\lda} must be \math{\geq} the length of a row in \math{\mat(A)}.
///
/// @param[in] S
///    A DenseSkOp object.
///    - Defines \math{\submat(\mtxS)}.
///
/// @param[in] ro_s
///     A nonnegative integer.
///     - The rows of \math{\submat(\mtxS)} are a contiguous subset of rows of \math{S}.
///     - The rows of \math{\submat(\mtxS)} start at \math{S[\texttt{ro_s}, :]}.
///
/// @param[in] co_s
///     A nonnnegative integer.
///     - The columns of \math{\submat(\mtxS)} are a contiguous subset of columns of \math{S}.
///     - The columns \math{\submat(\mtxS)} start at \math{S[:,\texttt{co_s}]}. 
///
/// @param[in] beta
///     A real scalar.
///     - If zero, then \math{B} need not be set on input.
///
/// @param[in, out] B
///    Pointer to 1D array of real scalars.
///    - On entry, defines \math{\mat(B)}
///      on the RIGHT-hand side of \math{(\star)}.
///    - On exit, defines \math{\mat(B)}
///      on the LEFT-hand side of \math{(\star)}.
///
/// @param[in] ldb
///    - Leading dimension of \math{\mat(B)} when reading from \math{B}.
///    - Refer to documentation for \math{\lda} for details. 
///
template <typename T, typename DenseSkOp>
void rskge3(
    blas::Layout layout,
    blas::Op opA,
    blas::Op opS,
    int64_t m, // B is m-by-d
    int64_t d, // op(S) is n-by-d
    int64_t n, // op(A) is m-by-n
    T alpha,
    const T *A,
    int64_t lda,
    DenseSkOp &S,
    int64_t ro_s,
    int64_t co_s,
    T beta,
    T *B,
    int64_t ldb
){
    auto [rows_submat_S, cols_submat_S] = dims_before_op(n, d, opS);
    constexpr bool maybe_denseskop = !std::is_same_v<std::remove_cv_t<DenseSkOp>, BLASFriendlyOperator<T>>;
    if constexpr (maybe_denseskop) {
        if (!S.buff) {
            // DenseSkOp doesn't permit defining a "black box" distribution, so we have to pack the submatrix
            // into an equivalent datastructure ourselves.
            auto submat_S = submatrix_as_blackbox<BLASFriendlyOperator<T>>(S, rows_submat_S, cols_submat_S, ro_s, co_s);
            rskge3(layout, opA, opS, m, d, n, alpha, A, lda, submat_S, 0, 0, beta, B, ldb);
            return;
        }
    }
    randblas_require( S.buff != nullptr );
    randblas_require( S.n_rows >= rows_submat_S + ro_s );
    randblas_require( S.n_cols >= cols_submat_S + co_s );
    auto [rows_A, cols_A] = dims_before_op(m, n, opA);
    if (layout == blas::Layout::ColMajor) {
        randblas_require(lda >= rows_A);
        randblas_require(ldb >= m);
    } else {
        randblas_require(lda >= cols_A);
        randblas_require(ldb >= d);
    }

    auto [pos, lds] = offset_and_ldim(S.layout, S.n_rows, S.n_cols, ro_s, co_s);
    T* S_ptr = &S.buff[pos];
    if (S.layout != layout)
        opS = (opS == blas::Op::NoTrans) ? blas::Op::Trans : blas::Op::NoTrans;

    blas::gemm(layout, opA, opS, m, d, n, alpha, A, lda, S_ptr, lds, beta, B, ldb);
    return;
}

} // end namespace RandBLAS::dense


namespace RandBLAS::sparse {

// MARK: LSKGES

// =============================================================================
/// LSKGES: Perform a GEMM-like operation
/// @verbatim embed:rst:leading-slashes
/// .. math::
///     \mat(B) = \alpha \cdot \underbrace{\op(\submat(\mtxS))}_{d \times m} \cdot \underbrace{\op(\mat(A))}_{m \times n} + \beta \cdot \underbrace{\mat(B)}_{d \times n},    \tag{$\star$}
/// @endverbatim
/// where \math{\alpha} and \math{\beta} are real scalars, \math{\op(\mtxX)} either returns a matrix \math{X}
/// or its transpose, and \math{S} is a sparse sketching operator.
/// 
/// @verbatim embed:rst:leading-slashes
/// What are :math:`\mat(A)` and :math:`\mat(B)`?
///     Their shapes are defined implicitly by :math:`(d, m, n, \opA)`.
///     Their precise contents are determined by :math:`(A, \lda)`, :math:`(B, \ldb)`,
///     and "layout", following the same convention as BLAS.
///
/// What is :math:`\submat(\mtxS)`?
///     Its shape is defined implicitly by :math:`(\opS, d, m)`.
///     If :math:`{\submat(\mtxS)}` is of shape :math:`r \times c`,
///     then it is the :math:`r \times c` submatrix of :math:`{\mtxS}` whose upper-left corner
///     appears at index :math:`(\texttt{ro_s}, \texttt{co_s})` of :math:`{\mtxS}`.
/// @endverbatim
/// @param[in] layout
///     Layout::ColMajor or Layout::RowMajor
///      - Matrix storage for \math{\mat(A)} and \math{\mat(B)}.
///
/// @param[in] opS
///      - If \math{\opS} = NoTrans, then \math{ \op(\submat(\mtxS)) = \submat(\mtxS)}.
///      - If \math{\opS} = Trans, then \math{\op(\submat(\mtxS)) = \submat(\mtxS)^T }.
///
/// @param[in] opA
///      - If \math{\opA} == NoTrans, then \math{\op(\mat(A)) = \mat(A)}.
///      - If \math{\opA} == Trans, then \math{\op(\mat(A)) = \mat(A)^T}.
///
/// @param[in] d
///     A nonnegative integer.
///     - The number of rows in \math{\mat(B)}
///     - The number of rows in \math{\op(\mat(S))}.
///
/// @param[in] n
///     A nonnegative integer.
///     - The number of columns in \math{\mat(B)}
///     - The number of columns in \math{\op(\mat(A))}.
///
/// @param[in] m
///     A nonnegative integer.
///     - The number of columns in \math{\op(\submat(\mtxS))}
///     - The number of rows in \math{\op(\mat(A))}.
///
/// @param[in] alpha
///     A real scalar.
///     - If zero, then \math{A} is not accessed.
///
/// @param[in] S
///    A SparseSkOp object.
///    - Defines \math{\submat(\mtxS)}.
///
/// @param[in] ro_s
///     A nonnegative integer.
///     - The rows of \math{\submat(\mtxS)} are a contiguous subset of rows of \math{S}.
///     - The rows of \math{\submat(\mtxS)} start at \math{S[\texttt{ro_s}, :]}.
///
/// @param[in] co_s
///     A nonnnegative integer.
///     - The columns of \math{\submat(\mtxS)} are a contiguous subset of columns of \math{S}.
///     - The columns \math{\submat(\mtxS)} start at \math{S[:,\texttt{co_s}]}. 
///
/// @param[in] A
///     Pointer to a 1D array of real scalars.
///     - Defines \math{\mat(A)}.
///
/// @param[in] lda
///     A nonnegative integer.
///     * Leading dimension of \math{\mat(A)} when reading from \math{A}.
///     * If layout == ColMajor, then
///         @verbatim embed:rst:leading-slashes
///             .. math::
///                 \mat(A)_{ij} = A[i + j \cdot \lda].
///         @endverbatim
///       In this case, \math{\lda} must be \math{\geq} the length of a column in \math{\mat(A)}.
///     * If layout == RowMajor, then
///         @verbatim embed:rst:leading-slashes
///             .. math::
///                 \mat(A)_{ij} = A[i \cdot \lda + j].
///         @endverbatim
///       In this case, \math{\lda} must be \math{\geq} the length of a row in \math{\mat(A)}.
///
/// @param[in] beta
///     A real scalar.
///     - If zero, then \math{B} need not be set on input.
///
/// @param[in, out] B
///    Pointer to 1D array of real scalars.
///    - On entry, defines \math{\mat(B)}
///      on the RIGHT-hand side of \math{(\star)}.
///    - On exit, defines \math{\mat(B)}
///      on the LEFT-hand side of \math{(\star)}.
///
/// @param[in] ldb
///    - Leading dimension of \math{\mat(B)} when reading from \math{B}.
///    - Refer to documentation for \math{\lda} for details. 
///
template <typename T, typename RNG, SignedInteger sint_t>
void lskges(
    blas::Layout layout,
    blas::Op opS,
    blas::Op opA,
    int64_t d, // B is d-by-n
    int64_t n, // \op(A) is m-by-n
    int64_t m, // \op(\mtxS) is d-by-m
    T alpha,
    SparseSkOp<T,RNG,sint_t> &S,
    int64_t ro_s,
    int64_t co_s,
    const T *A,
    int64_t lda,
    T beta,
    T *B,
    int64_t ldb
) {
    if (S.nnz < 0) {
        SparseSkOp<T,RNG,sint_t> shallowcopy(S.dist, S.seed_state); // shallowcopy.own_memory = true.
        fill_sparse(shallowcopy);
        lskges(layout, opS, opA, d, n, m, alpha, shallowcopy, ro_s, co_s, A, lda, beta, B, ldb);
        return;
    }
    auto Scoo = coo_view_of_skop(S);
    left_spmm(layout, opS, opA, d, n, m, alpha, Scoo, ro_s, co_s, A, lda, beta, B, ldb);
    return;
}


// MARK: RSKGES

// =============================================================================
/// RSKGES: Perform a GEMM-like operation
/// @verbatim embed:rst:leading-slashes
/// .. math::
///     \mat(B) = \alpha \cdot \underbrace{\op(\mat(A))}_{m \times n} \cdot \underbrace{\op(\submat(\mtxS))}_{n \times d} + \beta \cdot \underbrace{\mat(B)}_{m \times d},    \tag{$\star$}
/// @endverbatim
/// where \math{\alpha} and \math{\beta} are real scalars, \math{\op(\mtxX)} either returns a matrix \math{X}
/// or its transpose, and \math{S} is a sparse sketching operator.
/// 
/// @verbatim embed:rst:leading-slashes
/// What are :math:`\mat(A)` and :math:`\mat(B)`?
///     Their shapes are defined implicitly by :math:`(m, d, n, \opA)`.
///     Their precise contents are determined by :math:`(A, \lda)`, :math:`(B, \ldb)`,
///     and "layout", following the same convention as BLAS.
///
/// What is :math:`\submat(\mtxS)`?
///     Its shape is defined implicitly by :math:`(\opS, n, d)`.
///     If :math:`{\submat(\mtxS)}` is of shape :math:`r \times c`,
///     then it is the :math:`r \times c` submatrix of :math:`{\mtxS}` whose upper-left corner
///     appears at index :math:`(\texttt{ro_s}, \texttt{co_s})` of :math:`{\mtxS}`.
/// @endverbatim
/// @param[in] layout
///     Layout::ColMajor or Layout::RowMajor
///      - Matrix storage for \math{\mat(A)} and \math{\mat(B)}.
///
/// @param[in] opA
///      - If \math{\opA} == NoTrans, then \math{\op(\mat(A)) = \mat(A)}.
///      - If \math{\opA} == Trans, then \math{\op(\mat(A)) = \mat(A)^T}.
///
/// @param[in] opS
///      - If \math{\opS} = NoTrans, then \math{ \op(\submat(\mtxS)) = \submat(\mtxS)}.
///      - If \math{\opS} = Trans, then \math{\op(\submat(\mtxS)) = \submat(\mtxS)^T }.
///
/// @param[in] m
///     A nonnegative integer.
///     - The number of rows in \math{\mat(B)}.
///     - The number of rows in \math{\op(\mat(A))}.
///
/// @param[in] d
///     A nonnegative integer.
///     - The number of columns in \math{\mat(B)}
///     - The number of columns in \math{\op(\mat(S))}.
///
/// @param[in] n
///     A nonnegative integer.
///     - The number of columns in \math{\op(\mat(A))}
///     - The number of rows in \math{\op(\submat(\mtxS))}.
///
/// @param[in] alpha
///     A real scalar.
///     - If zero, then \math{A} is not accessed.
///
/// @param[in] A
///     Pointer to a 1D array of real scalars.
///     - Defines \math{\mat(A)}.
///
/// @param[in] lda
///     A nonnegative integer.
///     * Leading dimension of \math{\mat(A)} when reading from \math{A}.
///     * If layout == ColMajor, then
///         @verbatim embed:rst:leading-slashes
///             .. math::
///                 \mat(A)_{ij} = A[i + j \cdot \lda].
///         @endverbatim
///       In this case, \math{\lda} must be \math{\geq} the length of a column in \math{\mat(A)}.
///     * If layout == RowMajor, then
///         @verbatim embed:rst:leading-slashes
///             .. math::
///                 \mat(A)_{ij} = A[i \cdot \lda + j].
///         @endverbatim
///       In this case, \math{\lda} must be \math{\geq} the length of a row in \math{\mat(A)}.
///
/// @param[in] S
///    A SparseSkOp object.
///    - Defines \math{\submat(\mtxS)}.
///
/// @param[in] ro_s
///     A nonnegative integer.
///     - The rows of \math{\submat(\mtxS)} are a contiguous subset of rows of \math{S}.
///     - The rows of \math{\submat(\mtxS)} start at \math{S[\texttt{ro_s}, :]}.
///
/// @param[in] co_s
///     A nonnnegative integer.
///     - The columns of \math{\submat(\mtxS)} are a contiguous subset of columns of \math{S}.
///     - The columns \math{\submat(\mtxS)} start at \math{S[:,\texttt{co_s}]}. 
///
/// @param[in] beta
///     A real scalar.
///     - If zero, then \math{B} need not be set on input.
///
/// @param[in, out] B
///    Pointer to 1D array of real scalars.
///    - On entry, defines \math{\mat(B)}
///      on the RIGHT-hand side of \math{(\star)}.
///    - On exit, defines \math{\mat(B)}
///      on the LEFT-hand side of \math{(\star)}.
///
/// @param[in] ldb
///    - Leading dimension of \math{\mat(B)} when reading from \math{B}.
///    - Refer to documentation for \math{\lda} for details. 
///
template <typename T, typename RNG, SignedInteger sint_t>
inline void rskges(
    blas::Layout layout,
    blas::Op opA,
    blas::Op opS,
    int64_t m, // B is m-by-d
    int64_t d, // op(S) is n-by-d
    int64_t n, // op(A) is m-by-n
    T alpha,
    const T *A,
    int64_t lda,
    SparseSkOp<T,RNG,sint_t> &S,
    int64_t ro_s,
    int64_t co_s,
    T beta,
    T *B,
    int64_t ldb
) { 
    if (S.nnz < 0) {
        SparseSkOp<T,RNG,sint_t> shallowcopy(S.dist, S.seed_state); // shallowcopy.own_memory = true.
        fill_sparse(shallowcopy);
        rskges(layout, opA, opS, m, d, n, alpha, A, lda, shallowcopy, ro_s, co_s, beta, B, ldb);
    }
    auto Scoo = coo_view_of_skop(S);
    right_spmm(
        layout, opA, opS, m, d, n, alpha, A, lda, Scoo, ro_s, co_s, beta, B, ldb
    );
    return;
}

} // end namespace RandBLAS::sparse


namespace RandBLAS {

using namespace RandBLAS::dense;
using namespace RandBLAS::sparse;

// MARK: SKGE overloads, sub

// =============================================================================
/// \fn sketch_general(blas::Layout layout, blas::Op opS, blas::Op opA, int64_t d,
///     int64_t n, int64_t m, T alpha, SKOP &S, int64_t ro_s, int64_t co_s,
///     const T *A, int64_t lda, T beta, T *B, int64_t ldb
/// ) 
/// @verbatim embed:rst:leading-slashes
/// Sketch from the left in a GEMM-like operation
/// 
/// .. math::
///     \mat(B) = \alpha \cdot \underbrace{\op(\submat(\mtxS))}_{d \times m} \cdot \underbrace{\op(\mat(A))}_{m \times n} + \beta \cdot \underbrace{\mat(B)}_{d \times n},    \tag{$\star$}
/// 
/// where :math:`\alpha` and :math:`\beta` are real scalars, :math:`\op(\mtxX)` either returns a matrix :math:`\mtxX`
/// or its transpose, and :math:`\mtxS` is a sketching operator.
///
/// .. dropdown:: FAQ
///   :animate: fade-in-slide-down
///
///     **What are** :math:`\mat(A)` **and** :math:`\mat(B)` **?**
///
///       Their shapes are defined implicitly by :math:`(d, m, n, \opA).`
///       Their precise contents are determined by :math:`(A, \lda),` :math:`(B, \ldb),`
///       and "layout", following the same convention as GEMM from BLAS.
///
///       If layout == ColMajor, then
///
///             .. math::
///                 \mat(A)_{ij} = A[i + j \cdot \lda].
///
///       In this case, :math:`\lda` must be :math:`\geq` the length of a column in :math:`\mat(A).`
///
///       If layout == RowMajor, then
///
///             .. math::
///                 \mat(A)_{ij} = A[i \cdot \lda + j].
///
///       In this case, :math:`\lda` must be :math:`\geq` the length of a row in :math:`\mat(A).`
///
///     **What is** :math:`\submat(\mtxS)` **?**
///
///       Its shape is defined implicitly by :math:`(\opS, d, m).`
///
///       If :math:`{\submat(\mtxS)}` is of shape :math:`r \times c,`
///       then it is the :math:`r \times c` submatrix of :math:`{\mtxS}` whose upper-left corner
///       appears at index :math:`(\texttt{ro_s}, \texttt{co_s})` of :math:`{\mtxS}.`
///
/// .. dropdown:: Full parameter descriptions
///     :animate: fade-in-slide-down
///
///      layout - [in]
///       * Either Layout::ColMajor or Layout::RowMajor
///       * Matrix storage for :math:`\mat(A)` and :math:`\mat(B).`
///
///      opS - [in]
///       * Either Op::Trans or Op::NoTrans.
///       * If :math:`\opS` = NoTrans, then :math:`\op(\submat(\mtxS)) = \submat(\mtxS).`
///       * If :math:`\opS` = Trans, then :math:`\op(\submat(\mtxS)) = \submat(\mtxS)^T.`
///
///      opA - [in]
///       * If :math:`\opA` == NoTrans, then :math:`\op(\mat(A)) = \mat(A).`
///       * If :math:`\opA` == Trans, then :math:`\op(\mat(A)) = \mat(A)^T.`
///
///      d - [in]
///       * A nonnegative integer.
///       * The number of rows in :math:`\mat(B)`
///       * The number of rows in :math:`\op(\submat(\mtxS)).`
///
///      n - [in]
///       * A nonnegative integer.
///       * The number of columns in :math:`\mat(B)`
///       * The number of columns in :math:`\op(\mat(A)).`
///
///      m - [in]
///       * A nonnegative integer.
///       * The number of columns in :math:`\op(\submat(\mtxS))`
///       * The number of rows in :math:`\op(\mat(A)).`
///
///      alpha - [in]
///       * A real scalar.
///       * If zero, then :math:`A` is not accessed.
///
///      S - [in]  
///       * A DenseSkOp or SparseSkOp object.
///       * Defines :math:`\submat(\mtxS).`
///
///      ro_s - [in]
///       * A nonnegative integer.
///       * The rows of :math:`\submat(\mtxS)` are a contiguous subset of rows of :math:`S.`
///       * The rows of :math:`\submat(\mtxS)` start at :math:`S[\texttt{ro_s}, :].`
///
///      co_s - [in]
///       * A nonnnegative integer.
///       * The columns of :math:`\submat(\mtxS)` are a contiguous subset of columns of :math:`S.`
///       * The columns of :math:`\submat(\mtxS)` start at :math:`S[:,\texttt{co_s}].` 
///
///      A - [in]
///       * Pointer to a 1D array of real scalars.
///       * Defines :math:`\mat(A).`
///
///      lda - [in]
///       * A nonnegative integer.
///       * Leading dimension of :math:`\mat(A)` when reading from :math:`A.`
///
///      beta - [in]
///       * A real scalar.
///       * If zero, then :math:`B` need not be set on input.
///
///      B - [in,out]
///       * Pointer to 1D array of real scalars.
///       * On entry, defines :math:`\mat(B)`
///         on the RIGHT-hand side of :math:`(\star).`
///       * On exit, defines :math:`\mat(B)`
///         on the LEFT-hand side of :math:`(\star).`
///
///      ldb - [in]
///       * A nonnegative integer.
///       * Leading dimension of :math:`\mat(B)` when reading from :math:`B.`
///
/// @endverbatim
template <typename T, SketchingOperator SKOP>
inline void sketch_general(
    blas::Layout layout,
    blas::Op opS,
    blas::Op opA,
    int64_t d, // B is d-by-n
    int64_t n, // op(A) is m-by-n
    int64_t m, // op(submat(\mtxS)) is d-by-m
    T alpha,
    SKOP &S,
    int64_t ro_s,
    int64_t co_s,
    const T *A,
    int64_t lda,
    T beta,
    T *B,
    int64_t ldb
);

template <typename T, typename RNG>
inline void sketch_general(
    blas::Layout layout,
    blas::Op opS,
    blas::Op opA,
    int64_t d, // B is d-by-n
    int64_t n, // op(A) is m-by-n
    int64_t m, // op(submat(\mtxS)) is d-by-m
    T alpha,
    SparseSkOp<T, RNG> &S,
    int64_t ro_s,
    int64_t co_s,
    const T *A,
    int64_t lda,
    T beta,
    T *B,
    int64_t ldb
) {
    return sparse::lskges(
        layout, opS, opA, d, n, m, alpha, S,
        ro_s, co_s, A, lda, beta, B, ldb
    );
}

template <typename T, typename RNG>
inline void sketch_general(
    blas::Layout layout,
    blas::Op opS,
    blas::Op opA,
    int64_t d, // B is d-by-n
    int64_t n, // op(A) is m-by-n
    int64_t m, // op(submat(\mtxS)) is d-by-m
    T alpha,
    DenseSkOp<T, RNG> &S,
    int64_t ro_s,
    int64_t co_s,
    const T *A,
    int64_t lda,
    T beta,
    T *B,
    int64_t ldb
) {
    return dense::lskge3(
        layout, opS, opA, d, n, m, alpha, S,
        ro_s, co_s, A, lda, beta, B, ldb
    );
}


// =============================================================================
/// \fn sketch_general(blas::Layout layout, blas::Op opA, blas::Op opS, int64_t m, int64_t d, int64_t n,
///    T alpha, const T *A, int64_t lda, SKOP &S,
///    int64_t ro_s, int64_t co_s, T beta, T *B, int64_t ldb
/// )
/// @verbatim embed:rst:leading-slashes
/// Sketch from the right in a GEMM-like operation
///
/// .. math::
///     \mat(B) = \alpha \cdot \underbrace{\op(\mat(A))}_{m \times n} \cdot \underbrace{\op(\submat(\mtxS))}_{n \times d} + \beta \cdot \underbrace{\mat(B)}_{m \times d},    \tag{$\star$}
/// 
/// where :math:`\alpha` and :math:`\beta` are real scalars, :math:`\op(\mtxX)` either returns a matrix :math:`\mtxX`
/// or its transpose, and :math:`\mtxS` is a sketching operator.
/// 
/// .. dropdown:: FAQ
///   :animate: fade-in-slide-down
///
///     **What are** :math:`\mat(A)` **and** :math:`\mat(B)` **?**
///
///       Their shapes are defined implicitly by :math:`(m, d, n, \opA).`
///       Their precise contents are determined by :math:`(A, \lda),` :math:`(B, \ldb),`
///       and "layout", following the same convention as the Level 3 BLAS function "GEMM."
///
///     **What is** :math:`\submat(\mtxS)` **?**
///
///       Its shape is defined implicitly by :math:`(\opS, n, d).`
///       If :math:`{\submat(\mtxS)}` is of shape :math:`r \times c,`
///       then it is the :math:`r \times c` submatrix of :math:`{\mtxS}` whose upper-left corner
///       appears at index :math:`(\texttt{ro_s}, \texttt{co_s})` of :math:`{\mtxS}.`
///
/// .. dropdown:: Full parameter descriptions
///     :animate: fade-in-slide-down
///
///      layout - [in]
///       * Either Layout::ColMajor or Layout::RowMajor
///       * Matrix storage for :math:`\mat(A)` and :math:`\mat(B).`
///
///      opA - [in]
///       * If :math:`\opA` == NoTrans, then :math:`\op(\mat(A)) = \mat(A).`
///       * If :math:`\opA` == Trans, then :math:`\op(\mat(A)) = \mat(A)^T.`
///
///      opS - [in]
///       * Either Op::Trans or Op::NoTrans.
///       * If :math:`\opS` = NoTrans, then :math:`\op(\submat(\mtxS)) = \submat(\mtxS).`
///       * If :math:`\opS` = Trans, then :math:`\op(\submat(\mtxS)) = \submat(\mtxS)^T.`
///
///      m - [in]
///       * A nonnegative integer.
///       * The number of rows in :math:`\mat(B).`
///       * The number of rows in :math:`\op(\mat(A)).`
///
///      d - [in]
///       * A nonnegative integer.
///       * The number of columns in :math:`\mat(B)`
///       * The number of columns in :math:`\op(\submat(\mtxS)).`
///
///      n - [in]
///       * A nonnegative integer.
///       * The number of columns in :math:`\op(\mat(A)).`
///       * The number of rows in :math:`\op(\submat(\mtxS)).`
///
///      alpha - [in]
///       * A real scalar.
///       * If zero, then :math:`A` is not accessed.
///
///      A - [in]
///       * Pointer to a 1D array of real scalars.
///       * Defines :math:`\mat(A).`
///
///      lda - [in]
///       * A nonnegative integer.
///       * Leading dimension of :math:`\mat(A)` when reading from :math:`A.`
///
///      S - [in]  
///       * A DenseSkOp or SparseSkOp object.
///       * Defines :math:`\submat(\mtxS).`
///       * Defines :math:`\submat(\mtxS).`
///
///      ro_s - [in]
///       * A nonnegative integer.
///       * The rows of :math:`\submat(\mtxS)` are a contiguous subset of rows of :math:`S.`
///       * The rows of :math:`\submat(\mtxS)` start at :math:`S[\texttt{ro_s}, :].`
///
///      co_s - [in]
///       * A nonnegative integer.
///       * The columns of :math:`\submat(\mtxS)` are a contiguous subset of columns of :math:`S.`
///       * The columns :math:`\submat(\mtxS)` start at :math:`S[:,\texttt{co_s}].` 
///
///      beta - [in]
///       * A real scalar.
///       * If zero, then :math:`B` need not be set on input.
///
///      B - [in,out]
///       * Pointer to 1D array of real scalars.
///       * On entry, defines :math:`\mat(B)`
///         on the RIGHT-hand side of :math:`(\star).`
///       * On exit, defines :math:`\mat(B)`
///         on the LEFT-hand side of :math:`(\star).`
///
///      ldb - [in]
///       * A nonnegative integer.
///       * Leading dimension of :math:`\mat(B)` when reading from :math:`B.`
///
/// @endverbatim
template <typename T, SketchingOperator SKOP>
inline void sketch_general(
    blas::Layout layout,
    blas::Op opA,
    blas::Op opS,
    int64_t m, // B is m-by-d
    int64_t d, // op(submat(\mtxS)) is n-by-d
    int64_t n, // op(A) is m-by-n
    T alpha,
    const T *A,
    int64_t lda,
    SKOP &S,
    int64_t ro_s,
    int64_t co_s,
    T beta,
    T *B,
    int64_t ldb
);

template <typename T, typename RNG>
inline void sketch_general(
    blas::Layout layout,
    blas::Op opA,
    blas::Op opS,
    int64_t m, // B is m-by-d
    int64_t d, // op(submat(\mtxS)) is n-by-d
    int64_t n, // op(A) is m-by-n
    T alpha,
    const T *A,
    int64_t lda,
    DenseSkOp<T, RNG> &S,
    int64_t ro_s,
    int64_t co_s,
    T beta,
    T *B,
    int64_t ldb
) {
    return dense::rskge3(layout, opA, opS, m, d, n, alpha, A, lda,
        S, ro_s, co_s, beta, B, ldb
    );
}


template <typename T, typename RNG>
inline void sketch_general(
    blas::Layout layout,
    blas::Op opA,
    blas::Op opS,
    int64_t m, // B is m-by-d
    int64_t d, // op(submat(\mtxS)) is n-by-d
    int64_t n, // op(A) is m-by-n
    T alpha,
    const T *A,
    int64_t lda,
    SparseSkOp<T, RNG> &S,
    int64_t ro_s,
    int64_t co_s,
    T beta,
    T *B,
    int64_t ldb
) {
    return sparse::rskges(layout, opA, opS, m, d, n, alpha, A, lda,
        S, ro_s, co_s, beta, B, ldb
    );
}


// MARK: SKGE overloads, full

// =============================================================================
/// \fn sketch_general(blas::Layout layout, blas::Op opS, blas::Op opA, int64_t d,
///     int64_t n, int64_t m, T alpha, SKOP &S, const T *A, int64_t lda, T beta, T *B, int64_t ldb
/// ) 
/// @verbatim embed:rst:leading-slashes
/// Sketch from the left in a GEMM-like operation
///
/// .. math::
///     \mat(B) = \alpha \cdot \underbrace{\op(\mtxS)}_{d \times m} \cdot \underbrace{\op(\mat(A))}_{m \times n} + \beta \cdot \underbrace{\mat(B)}_{d \times n},    \tag{$\star$}
///
/// where :math:`\alpha` and :math:`\beta` are real scalars, :math:`\op(\mtxX)` either returns a matrix :math:`\mtxX`
/// or its transpose, and :math:`\mtxS` is a sketching operator.
///
/// .. dropdown:: Full parameter descriptions
///     :animate: fade-in-slide-down
///
///      layout - [in]
///       * Either Layout::ColMajor or Layout::RowMajor
///       * Matrix storage for :math:`\mat(A)` and :math:`\mat(B).`
///
///      opS - [in]
///       * Either Op::Trans or Op::NoTrans.
///       * If :math:`\opS` = NoTrans, then :math:`\op(\mtxS) = \mtxS.`
///       * If :math:`\opS` = Trans, then :math:`\op(\mtxS) = \mtxS^T.`
///
///      opA - [in]
///       * If :math:`\opA` == NoTrans, then :math:`\op(\mat(A)) = \mat(A).`
///       * If :math:`\opA` == Trans, then :math:`\op(\mat(A)) = \mat(A)^T.`
///
///      d - [in]
///       * A nonnegative integer.
///       * The number of rows in :math:`\mat(B)`
///       * The number of rows in :math:`\op(\mat(S)).`
///
///      n - [in]
///       * A nonnegative integer.
///       * The number of columns in :math:`\mat(B)`
///       * The number of columns in :math:`\op(\mat(A)).`
///
///      m - [in]
///       * A nonnegative integer.
///       * The number of columns in :math:`\op(\mtxS).`
///       * The number of rows in :math:`\op(\mat(A)).`
///
///      alpha - [in]
///       * A real scalar.
///       * If zero, then :math:`A` is not accessed.
///
///      S - [in]  
///       * A DenseSkOp or SparseSkOp object.
///       * Defines :math:`\submat(\mtxS).`
///
///      A - [in]
///       * Pointer to a 1D array of real scalars.
///       * Defines :math:`\mat(A).`
///
///      lda - [in]
///       * A nonnegative integer.
///       * Leading dimension of :math:`\mat(A)` when reading from :math:`A.`
///
///      beta - [in]
///       * A real scalar.
///       * If zero, then :math:`B` need not be set on input.
///
///      B - [in,out]
///       * Pointer to 1D array of real scalars.
///       * On entry, defines :math:`\mat(B)`
///         on the RIGHT-hand side of :math:`(\star).`
///       * On exit, defines :math:`\mat(B)`
///         on the LEFT-hand side of :math:`(\star).`
///
///      ldb - [in]
///       * A nonnegative integer.
///       * Leading dimension of :math:`\mat(B)` when reading from :math:`B.`
///
/// @endverbatim
template <typename T, SketchingOperator SKOP>
inline void sketch_general(
    blas::Layout layout,
    blas::Op opS,
    blas::Op opA,
    int64_t d, // B is d-by-n
    int64_t n, // op(A) is m-by-n
    int64_t m, // op(S) is d-by-m
    T alpha,
    SKOP &S,
    const T *A,
    int64_t lda,
    T beta,
    T *B,
    int64_t ldb
) {
    if (opS == blas::Op::NoTrans) {
        randblas_require(S.n_rows == d);
        randblas_require(S.n_cols == m);
    } else {
        randblas_require(S.n_rows == m);
        randblas_require(S.n_cols == d);
    }
    return sketch_general(layout, opS, opA, d, n, m, alpha, S, 0, 0, A, lda, beta, B, ldb);
};


// =============================================================================
/// \fn sketch_general(blas::Layout layout, blas::Op opA, blas::Op opS, int64_t m, int64_t d, int64_t n,
///    T alpha, const T *A, int64_t lda, SKOP &S, T beta, T *B, int64_t ldb
/// )
/// @verbatim embed:rst:leading-slashes
/// Sketch from the right in a GEMM-like operation
///
/// .. math::
///     \mat(B) = \alpha \cdot \underbrace{\op(\mat(A))}_{m \times n} \cdot \underbrace{\op(\mtxS)}_{n \times d} + \beta \cdot \underbrace{\mat(B)}_{m \times d},    \tag{$\star$}
///
/// where :math:`\alpha` and :math:`\beta` are real scalars, :math:`\op(\mtxX)` either returns a matrix :math:`\mtxX`
/// or its transpose, and :math:`\mtxS` is a sketching operator.
///
/// .. dropdown:: Full parameter descriptions
///     :animate: fade-in-slide-down
///
///      layout - [in]
///       * Either Layout::ColMajor or Layout::RowMajor
///       * Matrix storage for :math:`\mat(A)` and :math:`\mat(B).`
///
///      opA - [in]
///       * If :math:`\opA` == NoTrans, then :math:`\op(\mat(A)) = \mat(A).`
///       * If :math:`\opA` == Trans, then :math:`\op(\mat(A)) = \mat(A)^T.`
///
///      opS - [in]
///       * Either Op::Trans or Op::NoTrans.
///       * If :math:`\opS` = NoTrans, then :math:`\op(\mtxS) = \mtxS.`
///       * If :math:`\opS` = Trans, then :math:`\op(\mtxS) = \mtxS^T.`
///
///      m - [in]
///       * A nonnegative integer.
///       * The number of rows in :math:`\mat(B).`
///       * The number of rows in :math:`\op(\mat(A)).`
///
///      d - [in]
///       * A nonnegative integer.
///       * The number of columns in :math:`\mat(B).`
///       * The number of columns in :math:`\op(\mat(S)).`
///
///      n - [in]
///       * A nonnegative integer.
///       * The number of columns in :math:`\op(\mat(A)).`
///       * The number of rows in :math:`\op(\mtxS).`
///
///      alpha - [in]
///       * A real scalar.
///       * If zero, then :math:`A` is not accessed.
///
///      A - [in]
///       * Pointer to a 1D array of real scalars.
///       * Defines :math:`\mat(A).`
///
///      lda - [in]
///       * A nonnegative integer.
///       * Leading dimension of :math:`\mat(A)` when reading from :math:`A.`
///
///      S - [in]  
///       * A DenseSkOp or SparseSkOp object.
///
///      beta - [in]
///       * A real scalar.
///       * If zero, then :math:`B` need not be set on input.
///
///      B - [in,out]
///       * Pointer to 1D array of real scalars.
///       * On entry, defines :math:`\mat(B)`
///         on the RIGHT-hand side of :math:`(\star).`
///       * On exit, defines :math:`\mat(B)`
///         on the LEFT-hand side of :math:`(\star).`
///
///      ldb - [in]
///       * A nonnegative integer.
///       * Leading dimension of :math:`\mat(B)` when reading from :math:`B.`
///
/// @endverbatim
template <typename T, SketchingOperator SKOP>
inline void sketch_general(
    blas::Layout layout,
    blas::Op opA,
    blas::Op opS,
    int64_t m, // B is m-by-d
    int64_t d, // op(S) is n-by-d
    int64_t n, // op(A) is m-by-n
    T alpha,
    const T *A,
    int64_t lda,
    SKOP &S,
    T beta,
    T *B,
    int64_t ldb
) {
    if (opS == blas::Op::NoTrans) {
        randblas_require(S.n_rows == n);
        randblas_require(S.n_cols == d);
    } else {
        randblas_require(S.n_rows == d);
        randblas_require(S.n_cols == n);
    }
    return sketch_general(layout, opA, opS, m, d, n, alpha, A, lda, S, 0, 0, beta, B, ldb);
};

}  // end namespace RandBLAS

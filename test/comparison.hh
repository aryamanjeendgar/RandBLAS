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

#include "RandBLAS.hh"
#include <gtest/gtest.h>
#include <cmath>
#include <cmath>
#include <numeric>
#include <iostream>


namespace test::comparison {

using blas::Layout;
using blas::Op;

/** Tests two floating point numbers for approximate equality.
 * See https://randomascii.wordpress.com/2012/02/25/comparing-floating-point-numbers-2012-edition/
 *
 * @param[in] A    one number to compare
 * @param[in] B    the second number to compare
 * @param[in] atol is an absolute tolerance that comes into play when
 *                 the values are close to zero
 * @param[in] rtol is a relative tolerance, which should be close to
 *                 epsilon for the given type.
 * @param[inout] str a stream to send a decritpive error message to
 *
 * @returns true if the numbers are atol absolute difference or rtol relative
 *          difference from each other.
 */
template <typename T>
bool approx_equal(T A, T B, std::ostream &str,
    T atol = T(10)*std::numeric_limits<T>::epsilon(),
    T rtol = std::numeric_limits<T>::epsilon())
{
    // Check if the numbers are really close -- needed
    // when comparing numbers near zero.
    T diff_ab = abs(A - B);
    if (diff_ab <= atol)
        return true;

    T max_ab = std::max(abs(B), abs(A));

    if (diff_ab <= max_ab * rtol)
        return true;

    str.precision(std::numeric_limits<T>::max_digits10);

    str << A << " != " << B << " with absDiff=" << diff_ab
        << ", relDiff=" << max_ab*rtol << ", atol=" << atol
        << ", rtol=" << rtol;

    return false;
}

template <typename T>
void approx_equal(T A, T B, const char* testName, const char* fileName, int lineNo, T atol, T rtol) {
    std::ostringstream oss;
    if (!approx_equal(A, B, oss, atol, rtol)) {
        FAIL() << std::endl << fileName << ":" << lineNo << std::endl
            << testName << std::endl << "Test failed. " << oss.str() << std::endl;
        oss.str("");
    }
    return;
}



/** Test two arrays are approximately equal elementwise.
 *
 * @param[in] actual_ptr the array to check
 * @param[in] expect_ptr the array to check against
 * @param[in] size the number of elements to compare
 * @param[in] testName the name of the test, used in decriptive message
 * @param[in] fileName the name of the file, used in descriptive message
 * @param[in] lineNo the line tested, used in descriptive message
 * 
 * aborts if any of the elemnts are not approximately equal.
 */
template <typename T>
void buffs_approx_equal(
    const T *actual_ptr,
    const T *expect_ptr,
    int64_t size,
    const char *testName,
    const char *fileName,
    int lineNo,
    T atol = T(10)*std::numeric_limits<T>::epsilon(),
    T rtol = std::numeric_limits<T>::epsilon()
) {
    std::ostringstream oss;
    for (int64_t i = 0; i < size; ++i) {
        if (!approx_equal(actual_ptr[i], expect_ptr[i], oss, atol, rtol)) {
            FAIL() << std::endl << fileName << ":" << lineNo << std::endl
                << testName << std::endl << "Test failed at index " << i
                << " " << oss.str() << std::endl;
            oss.str("");
        }
    }
}

// TODO: Make macros that can automatically inject __PRETTY_FUNCTION__, __FILE__, and __LINE__ into the calls below.
//       This is slightly complicated by the presence of optional arguments in the function definition.

template <typename T>
void buffs_approx_equal(
    int64_t size,
    const T *actual_ptr,
    int64_t inc_actual,
    const T *expect_ptr,
    int64_t inc_expect,
    const char *testName,
    const char *fileName,
    int lineNo,
    T atol = T(10)*std::numeric_limits<T>::epsilon(),
    T rtol = std::numeric_limits<T>::epsilon()
) {
    std::ostringstream oss;
    for (int64_t i = 0; i < size; ++i) {
        if (!approx_equal(actual_ptr[i*inc_actual], expect_ptr[i*inc_expect], oss, atol, rtol)) {
            FAIL() << std::endl << fileName << ":" << lineNo << std::endl
                << testName << std::endl << "Test failed at index " << i
                << " " << oss.str() << std::endl;
            oss.str("");
        }
    }
}

template <typename T>
void buffs_approx_equal(
    const T *actual_ptr,
    const T *expect_ptr,
    const T *bounds_ptr,
    int64_t size,
    const char *test_name,
    const char *file_name,
    int line_no
) {
    std::ostringstream oss;
    for (int64_t i = 0; i < size; ++i) {
        T actual_err = abs(actual_ptr[i] - expect_ptr[i]);
        T allowed_err = bounds_ptr[i];
        if (actual_err > allowed_err) {
            FAIL() << std::endl << "\t" <<  file_name << ":" << line_no << std::endl
                    << "\t" << test_name << std::endl << "\tTest failed at index "
                    << i << ".\n\t| (" << actual_ptr[i] << ") - (" << expect_ptr[i] << ") | "
                    << " > " << allowed_err << oss.str() << std::endl;
            oss.str("");
        }
    }
}

template <typename T>
void matrices_approx_equal(
    blas::Layout layoutA,
    blas::Layout layoutB,
    blas::Op transB,
    int64_t m,
    int64_t n,
    const T *A,
    int64_t lda,
    const T *B,
    int64_t ldb,
    const char *testName,
    const char *fileName,
    int lineNo,
    T atol = T(10)*std::numeric_limits<T>::epsilon(),
    T rtol = std::numeric_limits<T>::epsilon()
) {
    std::ostringstream oss;
    // check that A == op(B), where A is m-by-n.
    auto idxa = [lda, layoutA](int64_t i, int64_t j) {
        return  (layoutA == blas::Layout::ColMajor) ? (i + j*lda) : (j + i*lda);
    };
    auto idxb = [ldb, layoutB](int64_t i, int64_t j) {
        return  (layoutB == blas::Layout::ColMajor) ? (i + j*ldb) : (j + i*ldb);
    };
    if (transB == blas::Op::NoTrans) {
        for (int64_t i = 0; i < m; ++i) {
            for (int64_t j = 0; j < n; ++j) {
                if (!approx_equal(A[idxa(i, j)], B[idxb(i, j)], oss, atol, rtol)) {
                    FAIL() << std::endl << fileName << ":" << lineNo << std::endl
                        << testName << std::endl << "\tTest failed at index ("
                        << i << ", " << j << ")\n\t" << oss.str() << std::endl;
                    oss.str("");
                }
            }
        }
    } else {
        for (int64_t i = 0; i < m; ++i) {
            for (int64_t j = 0; j < n; ++j) {
                if (!approx_equal(A[idxa(i, j)], B[idxb(j, i)], oss, atol, rtol)) {
                    FAIL() << std::endl << fileName << ":" << lineNo << std::endl
                        << testName << std::endl << "\tTest failed at index ("
                        << j << ", " << i << ")\n\t"  << oss.str() << std::endl;
                    oss.str("");
                }
            }
        }
    }
}


template <typename T>
void matrices_approx_equal(
    blas::Layout layout,
    blas::Op transB,
    int64_t m,
    int64_t n,
    const T *A,
    int64_t lda,
    const T *B,
    int64_t ldb,
    const char *testName,
    const char *fileName,
    int lineNo,
    T atol = T(10)*std::numeric_limits<T>::epsilon(),
    T rtol = std::numeric_limits<T>::epsilon()
) {
    matrices_approx_equal(layout, layout, transB, m, n, A, lda, B, ldb, testName, fileName, lineNo, atol, rtol);
}

} // end namespace test::comparison

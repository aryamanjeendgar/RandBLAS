#include "../common.hh"
#include "RandBLAS/skge.hh"
#include <gtest/gtest.h>
#include <math.h>


class TestRSKGE3 : public ::testing::Test
{
    protected:
    
    virtual void SetUp(){};

    virtual void TearDown(){};

    template <typename T>
    static void sketch_eye(
        uint32_t seed,
        int64_t m,
        int64_t d,
        bool preallocate,
        blas::Layout layout
    ) {
        RandBLAS::DenseDist D(m, d);
        RandBLAS::DenseSkOp<T> S0(D, seed, nullptr);
        if (preallocate)
            RandBLAS::fill_dense(S0);
        test::common::test_right_apply_submatrix_to_eye<T>(1.0, S0, m, d, 0, 0, layout, 0.0, 0);
    }

    template <typename T>
    static void transpose_S(
        uint32_t seed,
        int64_t m,
        int64_t d,
        blas::Layout layout
    ) {
        RandBLAS::DenseDist Dt(d, m);
        RandBLAS::DenseSkOp<T> S0(Dt, seed, nullptr);
        test::common::test_right_apply_tranpose_to_eye<T>(S0, layout);
    }

    template <typename T>
    static void submatrix_S(
        uint32_t seed,
        int64_t d, // columns in sketch
        int64_t m, // size of identity matrix
        int64_t d0, // cols in S0
        int64_t m0, // rows in S0
        int64_t S_ro, // row offset for S in S0
        int64_t S_co, // column offset for S in S0
        blas::Layout layout
    ) {
        RandBLAS::DenseDist D(m0, d0);
        RandBLAS::DenseSkOp<T> S0(D, seed);
        test::common::test_right_apply_submatrix_to_eye<T>(1.0, S0, m, d, S_ro, S_co, layout, 0.0, 0);
    }

    template <typename T>
    static void submatrix_A(
        uint32_t seed_S0, // seed for S
        int64_t d, // cols in S
        int64_t m, // rows in A.
        int64_t n, // cols in A, rows in S.
        int64_t m0, // rows in A0
        int64_t n0, // cols in A0
        int64_t A_ro, // row offset for A in A0
        int64_t A_co, // column offset for A in A0
        blas::Layout layout
    ) {
        RandBLAS::DenseDist D(n, d);
        RandBLAS::DenseSkOp<T> S0(D, seed_S0, nullptr);
        test::common::test_right_apply_to_submatrix<T>(S0, m, m0, n0, A_ro, A_co, layout);
    }

};


////////////////////////////////////////////////////////////////////////
//
//
//      RSKGE3: Basic sketching (vary preallocation, row vs col major)
//
//
////////////////////////////////////////////////////////////////////////

TEST_F(TestRSKGE3, right_sketch_eye_double_preallocate_colmajor)
{
    for (uint32_t seed : {0})
        sketch_eye<double>(seed, 200, 30, true, blas::Layout::ColMajor);
}

TEST_F(TestRSKGE3, right_sketch_eye_double_preallocate_rowmajor)
{
    for (uint32_t seed : {0})
        sketch_eye<double>(seed, 200, 30, true, blas::Layout::RowMajor);
}

TEST_F(TestRSKGE3, right_sketch_eye_double_null_colmajor)
{
    for (uint32_t seed : {0})
        sketch_eye<double>(seed, 200, 30, false, blas::Layout::ColMajor);
}

TEST_F(TestRSKGE3, right_sketch_eye_double_null_rowmajor)
{
    for (uint32_t seed : {0})
        sketch_eye<double>(seed, 200, 30, false, blas::Layout::RowMajor);
}

TEST_F(TestRSKGE3, right_sketch_eye_single_preallocate)
{
    for (uint32_t seed : {0})
        sketch_eye<float>(seed, 200, 30, true, blas::Layout::ColMajor);
}

TEST_F(TestRSKGE3, right_sketch_eye_single_null)
{
    for (uint32_t seed : {0})
        sketch_eye<float>(seed, 200, 30, false, blas::Layout::ColMajor);
}


////////////////////////////////////////////////////////////////////////
//
//
//      RSKGE3: Lifting
//
//
////////////////////////////////////////////////////////////////////////

TEST_F(TestRSKGE3, right_lift_eye_double_preallocate_colmajor)
{
    for (uint32_t seed : {0})
        sketch_eye<double>(seed, 10, 51, true, blas::Layout::ColMajor);
}

TEST_F(TestRSKGE3, right_lift_eye_double_preallocate_rowmajor)
{
    for (uint32_t seed : {0})
        sketch_eye<double>(seed, 10, 51, true, blas::Layout::RowMajor);
}

TEST_F(TestRSKGE3, right_lift_eye_double_null_colmajor)
{
    for (uint32_t seed : {0})
        sketch_eye<double>(seed, 10, 51, false, blas::Layout::ColMajor);
}

TEST_F(TestRSKGE3, right_lift_eye_double_null_rowmajor)
{
    for (uint32_t seed : {0})
        sketch_eye<double>(seed, 10, 51, false, blas::Layout::RowMajor);
}


////////////////////////////////////////////////////////////////////////
//
//
//      RSKGE3: transpose of S
//
//
////////////////////////////////////////////////////////////////////////

TEST_F(TestRSKGE3, transpose_double_colmajor)
{
    for (uint32_t seed : {0})
        transpose_S<double>(seed, 200, 30, blas::Layout::ColMajor);
}

TEST_F(TestRSKGE3, transpose_double_rowmajor)
{
    for (uint32_t seed : {0})
        transpose_S<double>(seed, 200, 30, blas::Layout::RowMajor);
}

TEST_F(TestRSKGE3, transpose_single)
{
    for (uint32_t seed : {0})
        transpose_S<float>(seed, 200, 30, blas::Layout::ColMajor);
}

////////////////////////////////////////////////////////////////////////
//
//
//      RSKGE3: Submatrices of S
//
//
////////////////////////////////////////////////////////////////////////

TEST_F(TestRSKGE3, submatrix_s_double_colmajor) 
{
    for (uint32_t seed : {0})
        submatrix_S<double>(seed,
            3, 10, // (cols, rows) in S.
            8, 12, // (cols, rows) in S0.
            2, // The first row of S is in the third row of S0
            1, // The first col of S is in the second col of S0
            blas::Layout::ColMajor
        );
}

TEST_F(TestRSKGE3, submatrix_s_double_rowmajor) 
{
    for (uint32_t seed : {0})
        submatrix_S<double>(seed,
            3, 10, // (cols, rows) in S.
            8, 12, // (cols, rows) in S0.
            2, // The first row of S is in the third row of S0
            1, // The first col of S is in the second col of S0
            blas::Layout::RowMajor
        );
}

TEST_F(TestRSKGE3, submatrix_s_single) 
{
    for (uint32_t seed : {0})
        submatrix_S<float>(seed,
            3, 10, // (cols, rows) in S.
            8, 12, // (cols, rows) in S0.
            2, // The first row of S is in the third row of S0
            1, // The first col of S is in the second col of S0
            blas::Layout::ColMajor
        );
}

////////////////////////////////////////////////////////////////////////
//
//
//     RSKGE3: submatrix of A
//
//
////////////////////////////////////////////////////////////////////////

TEST_F(TestRSKGE3, submatrix_a_double_colmajor) 
{
    for (uint32_t seed : {0})
        submatrix_A<double>(seed,
            3, // number of columns in sketch
            10, 5, // (rows, cols) in A.
            12, 8, // (rows, cols) in A0.
            2, // The first row of A is in the third row of A0.
            1, // The first col of A is in the second col of A0.
            blas::Layout::ColMajor
        );
}

TEST_F(TestRSKGE3, submatrix_a_double_rowmajor) 
{
    for (uint32_t seed : {0})
        submatrix_A<double>(seed,
            3, // number of columns in sketch
            10, 5, // (rows, cols) in A.
            12, 8, // (rows, cols) in A0.
            2, // The first row of A is in the third row of A0.
            1, // The first col of A is in the second col of A0.
            blas::Layout::RowMajor
        );
}

TEST_F(TestRSKGE3, submatrix_a_single) 
{
    for (uint32_t seed : {0})
        submatrix_A<float>(seed,
            3, // number of columns in sketch.
            10, 5, // (rows, cols) in A.
            12, 8, // (rows, cols) in A0.
            2, // The first row of A is in the third row of A0.
            1, // The first col of A is in the second col of A0.
            blas::Layout::ColMajor
        );
}

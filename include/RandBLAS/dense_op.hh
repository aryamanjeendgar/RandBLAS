#ifndef BLAS_HH
#include <blas.hh>
#define BLAS_HH
#endif

#ifndef RandBLAS_DO_HH
#define RandBLAS_DO_HH

// Below comes from .cc

#include <iostream>
#include <stdio.h>
#include <omp.h>

#include <math.h>
#include <typeinfo>

namespace RandBLAS::dense_op {

enum class DenseDist : char {Gaussian = 'G', Normal = 'G', Uniform = 'U', Rademacher = 'R', Haar = 'H'};

template <typename T>
struct SketchingBuffer {
    DenseDist dist = DenseDist::Gaussian;
    int64_t ctr_offset = 0;
    int64_t key = 0;
    int64_t n_rows;
    int64_t n_cols;
    T *op_data = NULL;
    bool populated = false;
    bool persistent = false;
}

template <typename T>
void gen_rmat_unif(
    int64_t n_rows,
    int64_t n_cols,
    T* mat,
    uint32_t seed
);

template <typename T>
void gen_rmat_norm(
    int64_t n_rows,
    int64_t n_cols,
    T* mat,
    uint32_t seed
);

} // end namespace RandBLAS::dense_op

#endif  // define RandBLAS_UTIL_HH

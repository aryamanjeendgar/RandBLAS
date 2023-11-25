#include "RandBLAS/base.hh"
#include "RandBLAS/exceptions.hh"
#include "RandBLAS/sparse_data/base.hh"
#include "RandBLAS/sparse_data/coo.hh"

namespace RandBLAS::sparse_data {

template <typename T>
struct CSRMatrix {
    const int64_t n_rows;
    const int64_t n_cols;
    const IndexBase index_base;
    const bool own_memory;
    int64_t nnz;
    T *vals;
    int64_t *rowptr;
    int64_t *colidxs;
    bool _can_reserve = true;

    CSRMatrix(
        int64_t n_rows,
        int64_t n_cols,
        IndexBase index_base = IndexBase::Zero
    ) : n_rows(n_rows), n_cols(n_cols), index_base(index_base), own_memory(true) {
        this->nnz = 0;
        this->vals = nullptr;
        this->rowptr = nullptr;
        this->colidxs = nullptr;
    };

    CSRMatrix(
        int64_t n_rows,
        int64_t n_cols,
        int64_t nnz,
        T *vals,
        int64_t *rowptr,
        int64_t *colidxs,
        IndexBase index_base = IndexBase::Zero
    ) : n_rows(n_rows), n_cols(n_cols), index_base(index_base), own_memory(false) {
        this->nnz = nnz;
        this->vals = vals;
        this->rowptr = rowptr;
        this->colidxs = colidxs;
    };

    ~CSRMatrix() {
        if (this->own_memory) {
            delete [] this->rowptr;
            delete [] this->colidxs;
            delete [] this->vals;
        }
    };

    void reserve(int64_t nnz) {
        randblas_require(this->_can_reserve);
        randblas_require(this->own_memory);
        this->nnz = nnz;
        this->rowptr = new int64_t[this->n_rows + 1];
        this->colidxs = new int64_t[nnz];
        this->vals = new T[nnz];
        this->_can_reserve = false;
    };

};

} // end namespace RandBLAS::sparse_data

namespace RandBLAS::sparse_data::csr {

using namespace RandBLAS::sparse_data;
using blas::Layout;

// =============================================================================
///
/// @param[in] stride_row
/// Logic offset for moving between two consecutive rows (equivalently,
/// to take one step down a given column). Equal to 1 in column-major format.
///
/// @param[in] stride_col
/// Logic offset for moving between two consecutive columns (equivalently,
/// to take one step along a given row). Equal to 1 in row-major format.
///
template <typename T>
void csr_to_dense(
    const CSRMatrix<T> &spmat,
    int64_t stride_row,
    int64_t stride_col,
    T *mat
) {
    auto *rowptr = spmat.rowptr;
    auto *colidxs = spmat.colidxs;
    auto *vals = spmat.vals;
    #define MAT(_i, _j) mat[(_i) * stride_row + (_j) * stride_col]
    for (int64_t i = 0; i < spmat.n_rows; ++i) {
        for (int64_t j = 0; j < spmat.n_cols; ++j) {
            MAT(i, j) = 0.0;
        }
    }
    for (int64_t i = 0; i < spmat.n_rows; ++i) {
        for (int64_t ell = rowptr[i]; ell < rowptr[i+1]; ++ell) {
            int j = colidxs[ell];
            if (spmat.index_base == IndexBase::One)
                j -= 1;
            MAT(i, j) = vals[ell];
        }
    }
    return;
}

template <typename T>
void csr_to_dense(
    const CSRMatrix<T> &spmat,
    Layout layout,
    T *mat
) {
    if (layout == Layout::ColMajor) {
        csr_to_dense(spmat, 1, spmat.n_cols, mat);
    } else {
        csr_to_dense(spmat, spmat.n_rows, 1, mat);
    }
    return;
}

template <typename T>
void dense_to_csr(
    int64_t stride_row,
    int64_t stride_col,
    T *mat,
    T abs_tol,
    CSRMatrix<T> &spmat
) {
    int64_t n_rows = spmat.n_rows;
    int64_t n_cols = spmat.n_cols;
    #define MAT(_i, _j) mat[(_i) * stride_row + (_j) * stride_col]
    // Step 1: count the number of entries with absolute value at least abstol
    int64_t nnz = nnz_in_dense(n_rows, n_cols, stride_row, stride_col, mat, abs_tol);
    // Step 2: allocate memory needed by the sparse matrix
    spmat.reserve(nnz);
    // Step 3: traverse the dense matrix again, populating the sparse matrix as we go
    nnz = 0;
    for (int64_t i = 0; i < n_rows; ++i) {
        for (int64_t j = 0; j < n_cols; ++j) {
            T val = MAT(i, j);
            if (abs(val) > abs_tol) {
                spmat.vals[nnz] = val;
                spmat.colidxs[nnz] = j;
                nnz += 1;
            }
        }
        spmat.rowptr[i+1] = nnz;
    }
    return;
}

template <typename T>
void dense_to_csr(Layout layout, T* mat, T abs_tol, CSRMatrix<T> &spmat) {
    if (layout == Layout::ColMajor) {
        dense_to_csr(1, spmat.n_cols, mat, abs_tol, spmat);
    } else {
        dense_to_csr(spmat.n_rows, 1, mat, abs_tol, spmat);
    }
    return;
}

template <typename T>
void coo_to_csr(COOMatrix<T> &coo, CSRMatrix<T> &csr) {
    sort_coo_data(NonzeroSort::CSR, coo);
    csr.reserve(coo.nnz);
    csr.rowptr[0] = 0;
    int64_t ell = 0;
    for (int64_t i = 0; i < coo.n_rows; ++i) {
        while (coo.rows[ell] == i) {
            csr.colidxs[ell] = coo.cols[ell];
            csr.vals[ell] = coo.vals[ell];
            ++ell;
        }
        csr.rowptr[ell] = ell;
    }
}

template <typename T>
void csr_to_coo(CSRMatrix<T> &csr, COOMatrix<T> &coo) {
    randblas_require(csr.n_rows == coo.n_rows);
    randblas_require(csr.n_cols == coo.n_cols);
    coo.reserve(csr.nnz);
    int64_t ell = 0;
    for (int64_t i = 0; i < csr.n_rows; ++i) {
        for (int64_t j = csr.rowptr[i]; j < csr.rowptr[i+1]; ++j) {
            coo.vals[ell] = csr.vals[ell];
            coo.rows[ell] = i;
            coo.cols[ell] = j;
        }
    }
    coo.sort = NonzeroSort::CSR;
}

template <typename T>
void csr_from_diag(
    T* vals,
    int64_t nnz,
    int64_t offset,
    CSRMatrix<T> &spmat
) {
    spmat.reserve(nnz);
    int64_t ell = 0;
    if (offset >= 0) {
        randblas_require(nnz <= spmat.n_rows);
        while (ell < nnz) {
            spmat.rowptr[ell] = ell;
            spmat.colidxs[ell] = ell + offset;
            spmat.vals[ell] = vals[ell];
            ++ell;
        }
    } else { // offset < 0
        randblas_require(nnz <= spmat.n_cols);
        for (int64_t i = 0; i < -offset; ++i)
            spmat.rowptr[i] = 0;
        while(ell < nnz) {
            spmat.rowptr[ell - offset] = ell;
            spmat.colidxs[ell] = ell;
            spmat.vals[ell] = vals[ell];
            ++ell;
        }
    }
    while (ell <= spmat.n_rows) {
        spmat.rowptr[ell] = nnz;
        ++ell;
    }
    return;
}


} // end namespace RandBLAS::sparse_data::csr
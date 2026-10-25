#include "addr.h"
#include "special.h"
#include "sparse.h"
#include "test.h"

static AddrBound bd(int lo, int hi) {
    AddrBound bound;
    bound.lo = lo;
    bound.hi = hi;
    return bound;
}

static void test_2021(void) {
    int cols = 0;
    Addr2 part;
    CHECK(addr_infer_cols(0, 0, 100, 3, 3, 220, 1, &cols));
    CHECK_EQ(cols, 39);
    CHECK(addr_calc2(bd(0, 20), bd(0, 38), 5, 5, ADDR_ROW, &part));
    CHECK_EQ(part.passed * part.each + part.inner, 200);
    CHECK_EQ(addr_loc(100, part.offset, 1), 300);
    cols = 0;
    CHECK(addr_infer_cols(3, 3, 220, 0, 0, 100, 1, &cols));
    CHECK_EQ(cols, 39);
    CHECK(!addr_infer_cols(0, 0, 100, 3, 3, 221, 1, &cols));
    CHECK(!addr_infer_cols(1, 0, 10, 1, 5, 20, 1, &cols));
}

static void test_row_col(void) {
    Addr2 row;
    Addr2 col;
    CHECK(addr_calc2(bd(0, 2), bd(0, 3), 1, 2, ADDR_ROW, &row));
    CHECK(addr_calc2(bd(1, 3), bd(1, 4), 2, 3, ADDR_ROW, &col));
    CHECK_EQ(row.offset, col.offset);
    CHECK_EQ(addr_loc(1000, row.offset, 2), 1012);
    CHECK(addr_calc2(bd(0, 2), bd(0, 3), 2, 3, ADDR_ROW, &row));
    CHECK_EQ(addr_loc(1000, row.offset, 2), 1022);
    CHECK(addr_calc2(bd(0, 2), bd(0, 3), 1, 2, ADDR_COL, &col));
    CHECK_EQ(addr_loc(1000, col.offset, 2), 1014);
    CHECK(addr_calc2(bd(0, 3), bd(0, 4), 3, 4, ADDR_ROW, &row));
    CHECK(addr_calc2(bd(0, 3), bd(0, 4), 3, 4, ADDR_COL, &col));
    CHECK_EQ(row.offset, 19);
    CHECK_EQ(col.offset, 19);
    CHECK_EQ(addr_loc(1000, row.offset, 4), 1076);
    CHECK_EQ(addr_scan2(bd(0, 2), bd(0, 3), ADDR_ROW), 0);
    CHECK_EQ(addr_scan2(bd(-1, 1), bd(2, 4), ADDR_COL), 0);
    CHECK(!addr_calc2(bd(0, 2), bd(0, 3), 3, 0, ADDR_ROW, &row));
    CHECK_EQ(addr_len(bd(3, 1)), -1);
}

static void test_3d(void) {
    Addr3 part;
    CHECK(addr_calc3(bd(0, 2), bd(0, 3), bd(0, 4), 2, 1, 3, ADDR_ROW, &part));
    CHECK_EQ(part.offset, 48);
    CHECK_EQ(addr_loc(200, part.offset, 2), 296);
    CHECK(addr_calc3(bd(0, 2), bd(0, 3), bd(0, 4), 2, 1, 3, ADDR_COL, &part));
    CHECK_EQ(part.offset, 41);
    CHECK_EQ(addr_loc(200, part.offset, 2), 282);
    CHECK(addr_calc3(bd(1, 3), bd(1, 4), bd(1, 5), 3, 2, 4, ADDR_ROW, &part));
    CHECK_EQ(part.offset, 48);
    CHECK_EQ(addr_scan3(bd(0, 1), bd(0, 1), bd(0, 1), ADDR_ROW), 0);
    CHECK_EQ(addr_scan3(bd(0, 1), bd(0, 1), bd(0, 1), ADDR_COL), 0);
}

static void test_exam_index(void) {
    MapExplain explain;
    int row = 0;
    int col = 0;
    CHECK(map_explain(MAP_BAND, 100, 30, 30, &explain));
    CHECK_EQ(explain.prefix + explain.inner, 87);
    CHECK_EQ(explain.k, 87);
    CHECK(map_band_inv(100, 87, &row, &col));
    CHECK_EQ(row, 30);
    CHECK_EQ(col, 30);
    CHECK(map_explain(MAP_SYM_UPPER_ROW, 12, 6, 6, &explain));
    CHECK_EQ(explain.k, 50);
    CHECK(map_explain(MAP_SYM_UPPER_COL, 10, 7, 2, &explain));
    CHECK_EQ(explain.swapped, 1);
    CHECK_EQ(explain.si, 2);
    CHECK_EQ(explain.sj, 7);
    CHECK_EQ(explain.k, 22);
    /* 数组从 1 计数时下标再加 1。教材练习：A[66][65] 在 B[1..298] 中是 195。 */
    CHECK(map_explain(MAP_BAND, 100, 66, 65, &explain));
    CHECK_EQ(explain.k + 1, 195);
    CHECK(map_explain(MAP_BAND, 5, 1, 1, &explain));
    CHECK_EQ(explain.prefix, -1);
    CHECK_EQ(explain.k, 0);
    CHECK(!map_explain(MAP_BAND, 100, 1, 3, &explain));
}

static void test_triangle_vs_sym(void) {
    MapExplain tri;
    MapExplain sym;
    CHECK(map_explain(MAP_TRI_LOWER, 4, 1, 3, &tri));
    CHECK(map_explain(MAP_SYM_LOWER, 4, 1, 3, &sym));
    CHECK_EQ(tri.constant, 1);
    CHECK_EQ(tri.k, 10);
    CHECK_EQ(sym.constant, 0);
    CHECK_EQ(sym.k, 3);
    CHECK_EQ(map_stored(MAP_BAND, 100), 298);
    CHECK_EQ(map_stored(MAP_SYM_UPPER_ROW, 12), 78);
    CHECK_EQ(map_stored(MAP_TRI_LOWER, 4), 11);
}

static void test_verify_all(void) {
    MapKind kinds[6] = {MAP_SYM_LOWER,     MAP_SYM_UPPER_ROW, MAP_SYM_UPPER_COL,
                        MAP_TRI_LOWER,     MAP_TRI_UPPER,     MAP_BAND};
    int n;
    int which;
    for (n = 1; n <= 12; n++) {
        for (which = 0; which < 6; which++) {
            CHECK(map_verify(kinds[which], n));
        }
    }
}

static void test_band_roundtrip(void) {
    int n;
    int k;
    int row;
    int col;
    MapExplain explain;
    for (n = 1; n <= 20; n++) {
        for (k = 0; k <= 3 * n - 3; k++) {
            CHECK(map_band_inv(n, k, &row, &col));
            CHECK(map_explain(MAP_BAND, n, row, col, &explain));
            CHECK_EQ(explain.k, k);
        }
    }
    CHECK(!map_band_inv(100, 298, &row, &col));
}

static void fill_classic(Sparse *matrix) {
    CHECK(sparse_init(matrix, 6, 7));
    CHECK(sparse_set(matrix, 1, 2, 12));
    CHECK(sparse_set(matrix, 1, 3, 9));
    CHECK(sparse_set(matrix, 3, 1, -3));
    CHECK(sparse_set(matrix, 3, 6, 14));
    CHECK(sparse_set(matrix, 4, 3, 24));
    CHECK(sparse_set(matrix, 5, 2, 18));
    CHECK(sparse_set(matrix, 6, 1, 15));
    CHECK(sparse_set(matrix, 6, 4, -7));
}

static void expect_same(const Sparse *left, const Sparse *right) {
    int p;
    CHECK_EQ(left->rows, right->rows);
    CHECK_EQ(left->cols, right->cols);
    CHECK_EQ(left->nnz, right->nnz);
    for (p = 1; p <= left->nnz; p++) {
        CHECK_EQ(left->data[p].row, right->data[p].row);
        CHECK_EQ(left->data[p].col, right->data[p].col);
        CHECK_EQ(left->data[p].val, right->data[p].val);
    }
}

static void test_classic_sparse(void) {
    Sparse matrix;
    Sparse slow;
    Sparse fast;
    int num[SP_MAX + 1];
    int cpot[SP_MAX + 1];
    int visits = 0;
    int val = 0;
    int scans = 0;
    fill_classic(&matrix);
    CHECK(!sparse_set(&matrix, 1, 1, 1));
    CHECK(!sparse_set(&matrix, 2, 2, 0));
    CHECK(sparse_get(&matrix, 3, 1, &val, &scans));
    CHECK_EQ(val, -3);
    CHECK_EQ(scans, 3);
    CHECK(sparse_get(&matrix, 1, 1, &val, &scans));
    CHECK_EQ(val, 0);
    CHECK_EQ(scans, 1);
    CHECK(!sparse_get(&matrix, 7, 1, &val, &scans));
    CHECK(sparse_transpose_slow(&matrix, &slow, &visits));
    CHECK_EQ(visits, 56);
    CHECK_EQ(slow.data[1].val, -3);
    CHECK_EQ(slow.data[8].row, 6);
    CHECK_EQ(slow.data[8].col, 3);
    CHECK_EQ(slow.data[8].val, 14);
    CHECK(sparse_count_cols(&matrix, num));
    CHECK_EQ(num[1], 2);
    CHECK_EQ(num[5], 0);
    CHECK_EQ(num[7], 0);
    CHECK(sparse_cpot(matrix.cols, num, cpot));
    CHECK_EQ(cpot[1], 1);
    CHECK_EQ(cpot[2], 3);
    CHECK_EQ(cpot[7], 9);
    CHECK(sparse_transpose_fast(&matrix, &fast, &visits));
    CHECK_EQ(visits, 22);
    expect_same(&slow, &fast);
    CHECK(sparse_get(&fast, 2, 1, &val, &scans));
    CHECK_EQ(val, 12);
}

static void test_pattern_transpose(void) {
    Sparse matrix;
    Sparse fast;
    int dense[6][7] = {{0}};
    int row;
    int col;
    int val = 0;
    CHECK(sparse_init(&matrix, 5, 6));
    for (row = 1; row <= 5; row++) {
        for (col = 1; col <= 6; col++) {
            if ((row * 3 + col) % 4 == 0) {
                CHECK(sparse_set(&matrix, row, col, row * 10 + col));
                dense[row][col] = row * 10 + col;
            }
        }
    }
    CHECK(sparse_transpose_fast(&matrix, &fast, NULL));
    for (row = 1; row <= 5; row++) {
        for (col = 1; col <= 6; col++) {
            CHECK(sparse_get(&fast, col, row, &val, NULL));
            CHECK_EQ(val, dense[row][col]);
        }
    }
}

static void test_2023_fields(void) {
    Sparse matrix;
    int val = 1;
    int scans = -1;
    CHECK(sparse_init(&matrix, 3, 3));
    CHECK(sparse_set(&matrix, 1, 1, 5));
    CHECK(sparse_get(&matrix, 2, 2, &val, &scans));
    CHECK_EQ(val, 0);
    CHECK(scans >= 1);
    CHECK(!sparse_get(&matrix, 4, 1, &val, &scans));
    CHECK(!sparse_init(NULL, 3, 3));
    CHECK(!sparse_init(&matrix, 0, 3));
}

int main(void) {
    test_2021();
    test_row_col();
    test_3d();
    test_exam_index();
    test_triangle_vs_sym();
    test_verify_all();
    test_band_roundtrip();
    test_classic_sparse();
    test_pattern_transpose();
    test_2023_fields();
    return TEST_SUMMARY();
}

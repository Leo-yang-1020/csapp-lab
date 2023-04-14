/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */ 
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

void block_transpose(int M, int N, int A[N][M], int B[M][N], int bsize)
{
    int i, j, ii , jj, temp;
    for (ii = 0; ii < N; ii += bsize) {
        for (jj = 0; jj < M; jj += bsize) {
            for (i = ii; i < ii + bsize && i < N; i++) {
                for (j = jj; j < jj + bsize && j < M; j++) {
                    temp = A[i][j];
                    B[j][i] = temp;
                }
            }
        }
    }
}

void solve_32(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;
  
    for (i = 0; i < 32; i += 8) {      // 枚举每八行
        for (j = 0; j < 32; j += 8) {  // 枚举每八列
            // 这里用这些临时(寄存器)变量，如果你查看过A和B的地址的话，你会发现A和B的地址差距是64的整数倍（0x40000），
            // 那么 直接赋值的话，在对角线的时候 每一个Load A[i][i]紧跟Store B[i][i],将造成比较多的
            // eviction
            for (int cnt = 0; cnt < 8; ++cnt, ++i) {  // 枚举0~8中的每一行，一行八列
                int temp1 = A[i][j];                  // 这八个只会发生一次miss
                int temp2 = A[i][j + 1];
                int temp3 = A[i][j + 2];
                int temp4 = A[i][j + 3];
                int temp5 = A[i][j + 4];
                int temp6 = A[i][j + 5];
                int temp7 = A[i][j + 6];
                int temp8 = A[i][j + 7];

                B[j][i] = temp1;  // 第一次 这八个都会 miss,后面就会命中，当然对角线有些例外
                B[j + 1][i] = temp2;
                B[j + 2][i] = temp3;
                B[j + 3][i] = temp4;
                B[j + 4][i] = temp5;
                B[j + 5][i] = temp6;
                B[j + 6][i] = temp7;
                B[j + 7][i] = temp8;
            }
            i -= 8;
        }
    }
}

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32)
        solve_32(M, N, A, B);
    else if (M == 64)
        block_transpose(M, N, A, B, 8);
    else block_transpose(M, N, A, B, 17); /* 不断测试得到的结果 */
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */ 

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }    

}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc); 

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc); 

}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}


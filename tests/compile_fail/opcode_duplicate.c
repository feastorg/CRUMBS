/* Must fail to compile with: duplicate case value */
#include "crumbs_ops.h"
#define OPS(X) X(OP_A, 0x03) X(OP_B, 0x80) X(OP_C, 0x03)
CRUMBS_DEFINE_FAMILY(FAM, 0x07, OPS)
int main(void) { return 0; }

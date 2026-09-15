/* Must fail to compile with: every opcode must be 0x00-0xFF and not 0xFE */
#include "crumbs_ops.h"
#define OPS(X) X(OP_A, 0x01) X(OP_B, 0xFE)
CRUMBS_DEFINE_FAMILY(FAM, 0x07, OPS)
int main(void) { return 0; }

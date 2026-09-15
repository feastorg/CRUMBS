/* Must fail to compile with: type_id must be 0x01-0xFF */
#include "crumbs_ops.h"
#define OPS(X) X(OP_A, 0x01)
CRUMBS_DEFINE_FAMILY(FAM, 0x00, OPS)
int main(void) { return 0; }

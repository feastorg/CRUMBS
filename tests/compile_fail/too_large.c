/* Must fail to compile with: payload exceeds CRUMBS_MAX_PAYLOAD */
#include "crumbs_ops.h"
#define F(X) X(u32, a) X(u32, b) X(u32, c) X(u32, d) X(u32, e) X(u32, f) X(u32, g)
CRUMBS_DEFINE_PAYLOAD(p, 28, F)
int main(void) { return 0; }

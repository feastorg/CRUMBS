/* Must fail to compile with: the field list does not sum to the declared wire size */
#include "crumbs_ops.h"
#define F(X) X(u8, a) X(i16, b)
CRUMBS_DEFINE_PAYLOAD(p, 4, F)
int main(void) { return 0; }

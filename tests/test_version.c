/**
 * Unit tests for CRUMBS version header.
 */

#include <stdio.h>
#include <string.h>
#include "crumbs.h"

int main(void)
{
/* Test 1: Version macros are defined */
#ifndef CRUMBS_VERSION_MAJOR
    fprintf(stderr, "FAIL: CRUMBS_VERSION_MAJOR not defined\n");
    return 1;
#endif

#ifndef CRUMBS_VERSION_MINOR
    fprintf(stderr, "FAIL: CRUMBS_VERSION_MINOR not defined\n");
    return 1;
#endif

#ifndef CRUMBS_VERSION_PATCH
    fprintf(stderr, "FAIL: CRUMBS_VERSION_PATCH not defined\n");
    return 1;
#endif

#ifndef CRUMBS_VERSION_STRING
    fprintf(stderr, "FAIL: CRUMBS_VERSION_STRING not defined\n");
    return 1;
#endif

#ifndef CRUMBS_VERSION
    fprintf(stderr, "FAIL: CRUMBS_VERSION not defined\n");
    return 1;
#endif

    /* Test 2: Numeric version matches formula (cross-consistency, not hardcoded values) */
    int expected_version = CRUMBS_VERSION_MAJOR * 10000 +
                           CRUMBS_VERSION_MINOR * 100 +
                           CRUMBS_VERSION_PATCH;
    if (CRUMBS_VERSION != expected_version)
    {
        fprintf(stderr, "FAIL: CRUMBS_VERSION macro (%d) does not match "
                "MAJOR*10000+MINOR*100+PATCH (%d)\n",
                CRUMBS_VERSION, expected_version);
        return 1;
    }

    /* Test 3: Version string matches MAJOR.MINOR.PATCH (cross-consistency) */
    char expected_string[32];
    snprintf(expected_string, sizeof(expected_string), "%d.%d.%d",
             CRUMBS_VERSION_MAJOR, CRUMBS_VERSION_MINOR, CRUMBS_VERSION_PATCH);
    if (strcmp(CRUMBS_VERSION_STRING, expected_string) != 0)
    {
        fprintf(stderr, "FAIL: CRUMBS_VERSION_STRING ('%s') does not match "
                "computed '%s'\n", CRUMBS_VERSION_STRING, expected_string);
        return 1;
    }

    printf("PASS: Version header validated (v%s, %d)\n",
           CRUMBS_VERSION_STRING, CRUMBS_VERSION);
    return 0;
}

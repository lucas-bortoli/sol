// Generates doctest's own main() — every other test .cpp just includes
// doctest.h and defines TEST_CASE(...). Intentionally the only place
// DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN is defined; doing it in more than one
// translation unit is a doctest misuse (duplicate main()).
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

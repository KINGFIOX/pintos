/** Exec a child with an exec string that spans a page boundary. */

#include <syscall.h>

#include "tests/lib.h"
#include "tests/main.h"
#include "tests/userprog/boundary.h"

void test_main(void) { wait(exec(copy_string_across_boundary("child-args arg1 arg2"))); }

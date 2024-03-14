#include <gmock/gmock.h>
#include <stdlib.h>

#include "include/filefuncs.h"
#include "include/myMemory.h"

#include "tests/gtests/sw_testhelpers.h"

using ::testing::HasSubstr;


namespace {

  TEST(MemoryTest, MemoryRealloc) {
    LOG_INFO LogInfo;
    sw_init_logs(NULL, &LogInfo); // Initialize logs and silence warn/error reporting

    int *ptr0, *ptr1;
    int k, n_old = 5, n_new = 7;
    size_t
      size_old = sizeof ptr0 * n_old,
      size_new = sizeof ptr0 * n_new;


    //--- Expect to reallocate previously allocated memory ------
    ptr0 = (int *)Mem_Malloc(size_old, "MemoryRealloc", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (k = 0; k < n_old; k++) {
      ptr0[k] = k;
    }

    ptr1 = (int *)Mem_ReAlloc(ptr0, size_new, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (k = 0; k < n_old; k++) {
      EXPECT_EQ(ptr1[k], k);
    }

    free(ptr1);
    ptr1 = NULL;


    //--- Expect to return NULL and report error if size_new is 0
    sw_init_logs(NULL, &LogInfo);

    ptr0 = (int *)Mem_Malloc(size_old, "MemoryRealloc", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    ptr1 = (int *)Mem_ReAlloc(ptr0, 0, &LogInfo);

    EXPECT_TRUE(isnull(ptr1));
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("failed due to new_size = 0"));



    //--- Expect to return NULL and report error if realloc failed
//    sw_init_logs(NULL, &LogInfo);
//
//    ptr0 = (int *)Mem_Malloc(size_old, "MemoryRealloc", &LogInfo);
//    sw_fail_on_error(&LogInfo); // exit test program if unexpected error
//
//    ptr1 = (int *)Mem_ReAlloc(ptr0, SIZE_MAX, &LogInfo);
//
//    EXPECT_TRUE(isnull(ptr1));
//    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("failed to allocate"));

  }

} // namespace

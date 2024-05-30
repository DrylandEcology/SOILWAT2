#include "include/generic.h"        // for isnull
#include "include/myMemory.h"       // for Mem_Malloc, Mem_ReAlloc
#include "include/SW_datastructs.h" // for LOG_INFO
#include "include/SW_Main_lib.h"    // for sw_fail_on_error, sw_init_logs
#include "gmock/gmock.h"            // for HasSubstr, MakePredicateFormatte...
#include "gtest/gtest.h"            // for Test, AssertionResult, Message, T...
#include <stdlib.h>                 // for NULL, free, size_t

using ::testing::HasSubstr;

namespace {
TEST(MemoryTest, MemoryRealloc) {
    LOG_INFO LogInfo;
    // Initialize logs and silence warn/error reporting
    sw_init_logs(NULL, &LogInfo);

    int *ptr0, *ptr1;
    int k, n_old = 5, n_new = 7;
    size_t size_old = sizeof ptr0 * n_old, size_new = sizeof ptr0 * n_new;


    //--- Expect to reallocate previously allocated memory ------
    ptr0 = (int *) Mem_Malloc(size_old, "MemoryRealloc", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (k = 0; k < n_old; k++) {
        ptr0[k] = k;
    }

    ptr1 = (int *) Mem_ReAlloc(ptr0, size_new, &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    for (k = 0; k < n_old; k++) {
        EXPECT_EQ(ptr1[k], k);
    }

    free(ptr1);
    ptr1 = NULL;


    //--- Expect to return NULL and report error if size_new is 0
    sw_init_logs(NULL, &LogInfo);

    ptr0 = (int *) Mem_Malloc(size_old, "MemoryRealloc", &LogInfo);
    sw_fail_on_error(&LogInfo); // exit test program if unexpected error

    ptr1 = (int *) Mem_ReAlloc(ptr0, 0, &LogInfo);

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

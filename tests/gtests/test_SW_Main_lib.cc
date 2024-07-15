#include "include/filefuncs.h"      // for LogError
#include "include/generic.h"        // for LOGERROR, LOGWARN
#include "include/SW_datastructs.h" // for LOG_INFO
#include "include/SW_Main_lib.h"    // for sw_init_logs, sw_fail_on_error
#include "gmock/gmock.h"            // for HasSubstr, MakePredicateFormatte...
#include "gtest/gtest.h"            // for Test, Message, AssertionResult
#include <stddef.h>                 // for NULL


using ::testing::HasSubstr;

namespace {
TEST(Messages, WarningsAndErrors) {
    LOG_INFO LogInfo;

    sw_init_logs(NULL, &LogInfo);

    LogError(&LogInfo, LOGWARN, "This is a %s.", "warning");
    LogError(&LogInfo, LOGWARN, "This is a second %s.", "warning");
    LogError(&LogInfo, LOGERROR, "This is an %s.", "error");

    EXPECT_EQ(LogInfo.numWarnings, 2);
    EXPECT_TRUE(LogInfo.stopRun);

    EXPECT_THAT(LogInfo.warningMsgs[0], HasSubstr("This is a warning."));
    EXPECT_THAT(LogInfo.warningMsgs[1], HasSubstr("This is a second warning."));
    EXPECT_THAT(LogInfo.errorMsg, HasSubstr("This is an error."));
}

TEST(MessagesDeath, FailOnErrorDeath) {
    LOG_INFO LogInfo;

    sw_init_logs(NULL, &LogInfo);

    LogError(&LogInfo, LOGERROR, "This is an %s.", "error");

    EXPECT_EQ(LogInfo.numWarnings, 0);
    EXPECT_TRUE(LogInfo.stopRun);

    EXPECT_DEATH_IF_SUPPORTED(sw_fail_on_error(&LogInfo), "This is an error.");
}
} // namespace

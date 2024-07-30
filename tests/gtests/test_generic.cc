#include "include/generic.h"    // for sw_strtok, standardDeviation, mean
#include "include/SW_Defines.h" // for SW_MISSING
#include "gtest/gtest.h"        // for Message, AssertionResult, Test, Test...
#include <cmath>                // for isnan
#include <stdio.h>              // for NULL

namespace {
const unsigned int N = 9;
unsigned int k;
double x[N] = {-4., -3., -2., -1., 0., 1., 2., 3., 4.},
       // m calculated in R with `for (k in seq_along(x)) print(mean(x[1:k]))`
    m[N] = {-4, -3.5, -3, -2.5, -2, -1.5, -1, -0.5, 0},
       // sd calculated in R with `for (k in seq_along(x)) print(sd(x[1:k]))`
    sd[N] = {
        SW_MISSING,
        0.7071068,
        1.,
        1.290994,
        1.581139,
        1.870829,
        2.160247,
        2.44949,
        2.738613
};
double tol = 1e-6;

TEST(GenericTest, GenericRunningMean) {
    double m_at_k = 0.;

    for (k = 0; k < N; k++) {
        m_at_k = get_running_mean(k + 1, m_at_k, x[k]);
        EXPECT_DOUBLE_EQ(m_at_k, m[k]);
    }
}

TEST(GenericTest, GenericRunningSD) {
    double ss;
    double sd_at_k;

    for (k = 0; k < N; k++) {
        if (k == 0) {
            ss = get_running_sqr(0., m[k], x[k]);

        } else {
            ss += get_running_sqr(m[k - 1], m[k], x[k]);

            sd_at_k = final_running_sd(k + 1, ss); // k is base0
            EXPECT_NEAR(sd_at_k, sd[k], tol);
        }
    }
}

TEST(GenericTest, GenericUnexpectedAndExpectedCasesSD) {
    double value[1] = {5.};
    double values[5] = {5.4, 3.4, 7.6, 5.6, 1.8};
    double oneValMissing[5] = {5.4, SW_MISSING, 7.6, 5.6, 1.8};

    double standardDev = standardDeviation(value, 1);

    // Testing that one value for a standard deviation is `NAN`
    EXPECT_TRUE(isnan(standardDev));

    standardDev = standardDeviation(value, 0);

    // Testing that no value put into standard deviation is `NAN`
    EXPECT_DOUBLE_EQ(standardDev, 0.);

    standardDev = standardDeviation(values, 5);

    // Testing the standard deviation function on a normal set of data
    EXPECT_NEAR(standardDev, 2.22441, tol);

    standardDev = standardDeviation(oneValMissing, 5);

    // Testing the standard deviation function on a normal set of data with
    // one value set to SW_MISSING
    EXPECT_NEAR(standardDev, 2.413848, tol);
}

TEST(GenericTest, GenericUnexpectedAndExpectedCasesMean) {

    double result;
    double values[5] = {1.8, 2.2, 10., 13.5, 3.2};
    double oneValMissing[5] = {4.3, 2.6, SW_MISSING, 17.1, 32.4};

    result = mean(values, 0);

    // Testing that a set of size zero returns `NAN` for a mean
    EXPECT_TRUE(isnan(result));

    result = mean(values, 5);

    // Testing the mean function on a normal set of data
    EXPECT_FLOAT_EQ(result, 6.14);

    result = mean(oneValMissing, 5);

    // Testing the mean function on a normal set of data
    EXPECT_FLOAT_EQ(result, 14.1);
}

TEST(GenericTest, GenericStrtok) {
    /*
        This section covers a custom version of C's `strtok()` function
        which is not thread-safe -- while the custom version, `sw_strtok()` is.
    */

    char *currString;
    size_t startIndex = 0;
    size_t strLen = 0;

    char emptyDelim[] = "";
    char oneDelim[] = "\\";
    char multipleDelim[] = "*/^%\\";
    char pathDelim[] = "/";
    char extDelim[] = ".";

    char emptyString[] = "";
    char oneDelimStr[] = "dir\\testFile.in";
    char multipleDelimStr[] = "%root\\dir^folder/testFile.in";
    char filepathStr1[] = "path/to/my_file1.txt";
    char filepathStr2[] = "path/to/my_file2.txt";


    // Test separation between file name and file extension
    currString = sw_strtok(filepathStr1, &startIndex, &strLen, extDelim);
    ASSERT_STREQ(currString, "path/to/my_file1");

    currString = sw_strtok(filepathStr1, &startIndex, &strLen, extDelim);
    ASSERT_STREQ(currString, "txt");

    currString = sw_strtok(filepathStr1, &startIndex, &strLen, extDelim);
    ASSERT_TRUE(currString == NULL);
    startIndex = 0,
    strLen = 0; // Reset start index and string length for next test


    // Test separation among file path elements
    currString = sw_strtok(filepathStr2, &startIndex, &strLen, pathDelim);
    ASSERT_STREQ(currString, "path");

    currString = sw_strtok(filepathStr2, &startIndex, &strLen, pathDelim);
    ASSERT_STREQ(currString, "to");

    currString = sw_strtok(filepathStr2, &startIndex, &strLen, pathDelim);
    ASSERT_STREQ(currString, "my_file2.txt");

    currString = sw_strtok(filepathStr2, &startIndex, &strLen, pathDelim);
    ASSERT_TRUE(currString == NULL);
    startIndex = 0,
    strLen = 0; // Reset start index and string length for next test


    // Test that empty strings return NULL
    currString = sw_strtok(emptyString, &startIndex, &strLen, emptyDelim);
    ASSERT_TRUE(currString == NULL);
    startIndex = 0,
    strLen = 0; // Reset start index and string length for next test

    currString = sw_strtok(emptyString, &startIndex, &strLen, pathDelim);
    ASSERT_TRUE(currString == NULL);
    startIndex = 0,
    strLen = 0; // Reset start index and string length for next test


    // Test strings when there is an empty delimiter
    // Strings should remain the same
    currString = sw_strtok(multipleDelimStr, &startIndex, &strLen, emptyDelim);
    ASSERT_STREQ(currString, "%root\\dir^folder/testFile.in");
    startIndex = 0,
    strLen = 0; // Reset start index and string length for next test

    // Test strings when there is one delimiter
    // The strings should be split into multiple parts depending on the number
    // of delimiters used (two here)
    currString = sw_strtok(oneDelimStr, &startIndex, &strLen, oneDelim);
    ASSERT_STREQ(currString, "dir");

    currString = sw_strtok(oneDelimStr, &startIndex, &strLen, oneDelim);
    ASSERT_STREQ(currString, "testFile.in");
    startIndex = 0,
    strLen = 0; // Reset start index and string length for next test

    // Test things when there are more than one delimiter, both possible
    // characters and more than one occurrance This should split the string up
    // in into multiple parts depending on the number of delimiters used (four
    // here)
    currString =
        sw_strtok(multipleDelimStr, &startIndex, &strLen, multipleDelim);
    ASSERT_STREQ(currString, "root");

    currString =
        sw_strtok(multipleDelimStr, &startIndex, &strLen, multipleDelim);
    ASSERT_STREQ(currString, "dir");

    currString =
        sw_strtok(multipleDelimStr, &startIndex, &strLen, multipleDelim);
    ASSERT_STREQ(currString, "folder");

    currString =
        sw_strtok(multipleDelimStr, &startIndex, &strLen, multipleDelim);
    ASSERT_STREQ(currString, "testFile.in");
}
} // namespace

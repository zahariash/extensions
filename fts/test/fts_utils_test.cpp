#include "common/exception/runtime.h"
#include "function/fts_config.h"
#include "gtest/gtest.h"
#include "re2.h"
#include "utils/fts_utils.h"

using namespace lbug::common;
using namespace lbug::fts_extension;

namespace {

std::string norm(std::string query, const char* pattern, bool preserveWildcards) {
    RE2 re{pattern};
    FTSUtils::normalizeQuery(query, re, preserveWildcards);
    return query;
}

constexpr const char* kCustomClass = "[^[:alnum:]-]+";

} // namespace

TEST(FTSUtilsTest, NormalizeDocumentPathStripsDefaultSeparators) {
    EXPECT_EQ("foo bar", norm("foo*bar", IgnorePattern::DEFAULT_VALUE, false));
    EXPECT_EQ("foo", norm("Foo", IgnorePattern::DEFAULT_VALUE, false));
}

TEST(FTSUtilsTest, NormalizeDocumentPathCustomClass) {
    EXPECT_EQ("a4 ", norm("A4!", kCustomClass, false));
    EXPECT_EQ("t-roc", norm("t-roc", kCustomClass, false));
}

TEST(FTSUtilsTest, NormalizeQueryKeepsWildcardsUnderCustomClass) {
    EXPECT_EQ("a?", norm("a?", kCustomClass, true));
    EXPECT_EQ("t?roc", norm("t?roc", kCustomClass, true));
    EXPECT_EQ("202*", norm("202*", kCustomClass, true));
}

TEST(FTSUtilsTest, NormalizeQueryAppliesCustomPattern) {
    EXPECT_EQ("a4", norm("A4", kCustomClass, true));
    EXPECT_EQ("a4 ", norm("A4!", kCustomClass, true));
    EXPECT_EQ(" 6 ", norm("(6)", kCustomClass, true));
}

TEST(FTSUtilsTest, NormalizeQueryDefaultPatternKeepsWildcards) {
    EXPECT_EQ("m?c*n*", norm("m?c*n*", IgnorePattern::DEFAULT_VALUE, true));
}

TEST(FTSUtilsTest, NormalizeQueryLeadingWildcardSkip) {
    EXPECT_EQ("*202", norm("*202", kCustomClass, true));
    EXPECT_EQ("*n*", norm("*n*", kCustomClass, true));
}

TEST(FTSUtilsTest, NormalizeQueryNoMatchCopiesRest) {
    EXPECT_EQ("t-roc", norm("t-roc", kCustomClass, true));
    EXPECT_EQ("6", norm("6", kCustomClass, true));
}

TEST(FTSUtilsTest, NormalizeQueryEmptyMatchAdvances) {
    EXPECT_EQ("xyz", norm("xyz", "a*", true));
}

TEST(FTSUtilsTest, NormalizeQueryWildcardInsideMatchSpan) {
    EXPECT_EQ("  *  ", norm("ab*cd", ".+", true));
}

TEST(FTSUtilsTest, NormalizeQueryInvalidPatternThrows) {
    RE2 re{"[a-z"};
    ASSERT_FALSE(re.ok());
    std::string query{"abc"};
    EXPECT_THROW(FTSUtils::normalizeQuery(query, re, true), RuntimeException);
    EXPECT_THROW(FTSUtils::normalizeQuery(query, re, false), RuntimeException);
}

TEST(FTSUtilsTest, NormalizeQueryEmptyInput) {
    EXPECT_EQ("", norm("", kCustomClass, true));
    EXPECT_EQ("", norm("", IgnorePattern::DEFAULT_VALUE, false));
}

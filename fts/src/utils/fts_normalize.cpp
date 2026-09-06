#include "utils/fts_utils.h"

#include "common/exception/runtime.h"
#include "common/string_utils.h"
#include "re2.h"

namespace lbug {
namespace fts_extension {

using namespace lbug::common;

void FTSUtils::normalizeQuery(std::string& query, const RE2& ignorePattern,
    bool preserveWildcards) {
    if (!ignorePattern.ok()) {
        throw RuntimeException{std::format(
            "An error occurred while compiling the regex: \"{}\".\nError: \"{}\".",
            ignorePattern.pattern(), ignorePattern.error())};
    }
    if (!preserveWildcards) {
        std::string replacePattern = " ";
        RE2::GlobalReplace(&query, ignorePattern, replacePattern);
        StringUtils::toLower(query);
        return;
    }
    std::string out;
    out.reserve(query.size());
    const auto n = query.size();
    size_t pos = 0;
    while (pos < n) {
        if (query[pos] == '*' || query[pos] == '?') {
            out.push_back(query[pos++]);
            continue;
        }
        regex::StringPiece input(query);
        regex::StringPiece match;
        if (!ignorePattern.Match(input, pos, n, RE2::UNANCHORED, &match, 1)) {
            out.append(query, pos, n - pos);
            break;
        }
        if (match.empty()) {
            out.push_back(query[pos++]);
            continue;
        }
        const auto matchPos = static_cast<size_t>(match.data() - input.data());
        if (matchPos > pos) {
            out.append(query, pos, matchPos - pos);
        }
        for (size_t i = 0; i < match.size(); i++) {
            const char c = match[i];
            out.push_back((c == '*' || c == '?') ? c : ' ');
        }
        pos = matchPos + match.size();
    }
    query = std::move(out);
    StringUtils::toLower(query);
}

std::string FTSUtils::escapeForCypher(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (c == '\\' || c == '\'') {
            result += '\\';
        }
        result += c;
    }
    return result;
}

} // namespace fts_extension
} // namespace lbug

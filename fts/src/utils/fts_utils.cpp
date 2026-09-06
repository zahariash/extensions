#include "utils/fts_utils.h"

#include <optional>

#include "common/exception/runtime.h"
#include "common/string_utils.h"
#include "cppjieba/Jieba.hpp"
#include "function/stem.h"
#include "libstemmer.h"
#include "re2.h"
#include "storage/storage_manager.h"
#include "storage/table/node_table.h"

namespace lbug {
namespace fts_extension {

using namespace lbug::common;
using namespace lbug::storage;
using namespace lbug::transaction;
using namespace lbug::catalog;

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

struct StopWordsChecker {
    ValueVector termsVector;
    storage::NodeTable* stopWordsTable;
    transaction::Transaction* tx;
    common::offset_t offset = common::INVALID_OFFSET;
    std::function<bool(const std::string& term)> isStopWord;

    StopWordsChecker(MemoryManager* mm, NodeTable* stopWordsTable, Transaction* tx,
        bool defaultStopWords);
};

StopWordsChecker::StopWordsChecker(MemoryManager* mm, NodeTable* stopwordsTable, Transaction* tx,
    bool defaultStopWords)
    : termsVector{LogicalType::STRING(), mm}, stopWordsTable{stopwordsTable}, tx{tx} {
    termsVector.state = common::DataChunkState::getSingleValueDataChunkState();
    if (defaultStopWords) {
        isStopWord = [](const std::string& term) {
            return StopWords::getDefaultStopWords().contains(term);
        };
    } else {
        isStopWord = [this](const std::string& term) {
            termsVector.setValue(0, term);
            return stopWordsTable->lookupPK(this->tx, &termsVector, 0 /* vectorPos */, offset);
        };
    }
}

bool FTSUtils::hasWildcardPattern(const std::string& term) {
    for (auto& c : term) {
        if (c == '*' || c == '?') {
            return true;
        }
    }
    return false;
}

std::vector<std::string> FTSUtils::stemTerms(std::vector<std::string> terms,
    const FTSConfig& config, MemoryManager* mm, NodeTable* stopwordsTable, Transaction* tx,
    bool isConjunctive, bool isQuery) {
    if (config.stemmer == "none" && !isConjunctive) {
        return terms;
    }
    std::optional<StopWordsChecker> checker;
    if (isConjunctive) {
        checker.emplace(mm, stopwordsTable, tx, config.stopWordsSource == StopWords::DEFAULT_VALUE);
    }
    if (config.stemmer == "none") {
        std::vector<std::string> result;
        for (auto& term : terms) {
            if (checker->isStopWord(term)) {
                continue;
            }
            result.push_back(term);
        }
        return result;
    }
    StemFunction::validateStemmer(config.stemmer);
    auto sbStemmer = sb_stemmer_new(reinterpret_cast<const char*>(config.stemmer.c_str()), "UTF_8");
    if (!sbStemmer) {
        // If stemmer creation fails, fall back to returning original terms to avoid crashes.
        return terms;
    }
    std::vector<std::string> result;
    for (auto& term : terms) {
        if (checker && checker->isStopWord(term)) {
            continue;
        }
        if (isQuery && hasWildcardPattern(term)) {
            result.push_back(term);
            continue;
        }
        auto stemData = sb_stemmer_stem(sbStemmer, reinterpret_cast<const sb_symbol*>(term.c_str()),
            term.length());
        if (stemData) {
            result.push_back(std::string(reinterpret_cast<const char*>(stemData)));
        } else {
            // If stemming fails for a term, keep the original term.
            result.push_back(term);
        }
    }
    sb_stemmer_delete(sbStemmer);
    return result;
}

std::vector<std::string> FTSUtils::tokenizeString(std::string& str, const FTSConfig& config) {
    std::vector<std::string> terms;
    if (config.tokenizer == "jieba") {
        cppjieba::Jieba jieba(config.jiebaDictDir + "/jieba.dict.utf8",
            config.jiebaDictDir + "/hmm_model.utf8", config.jiebaDictDir + "/user.dict.utf8",
            config.jiebaDictDir + "/idf.utf8", config.jiebaDictDir + "/stop_words.utf8");
        jieba.CutForSearch(str, terms);
    } else {
        terms = StringUtils::split(str, " ", true /* ignoreEmptyStringParts */);
    }
    return terms;
}

} // namespace fts_extension
} // namespace lbug

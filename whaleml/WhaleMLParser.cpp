/*
 * File: WhaleMLParser.cpp
 *
 * Orka WhaleML parser - Implementation
 *
 * This file is part of Orka Engine - the game engine that stays out of your way
 *
 * Copyright (c) 2024-2026 Lucas G. (aka TheAllocator32)
 *
 * Licensed under the Apache License, Version 2.0.
 * See the LICENSE file in the project root for details.
 */

#include "WhaleMLParser.hpp"
#include "../essentials/Logger.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>
#include <regex>
#include <stdexcept>
#include <cctype>
#include <string_view>

/*
 * WhaleML example:
 *
 * // C++ style comments allowed
 *
 * number : 1 // Key = value (can be key:value without whitespaces as well) with integral Nummer
 * float_number : 2.5 // Key = value with floating point number
 * some_string : "bla" // Key = value with string (can be 'bla' as well)
 * list : 1, 2, 3, 4, 5, 6 // List with a single type
 * multi_list : 1, 2.5, 3.666, "blah", 'chars', ["list inside another list", 2.5, 567]
 *
 * // multi key=values in one line
 * number : 1; test : "bla"; whatever : 2.5
 *
 * // Predefined value-type key = value (Special feature) (Like C++ ENUMS)
 * physics_body_type : rigid  // rigid = predefined value!
 *
 * // You may use = instead of :
 *
 * // Trees:
 * // I know they aren’t that powerful but at least they exist!
 * parent_object.child1 : 10
 * parent_object.child2 : 100
 * parent_object.child2.bla : 42
 *
 */

std::string shortenForLog(std::string_view text, std::size_t maxLen = 160) {
    if (text.size() <= maxLen)
        return std::string(text);
    return std::string(text.substr(0, maxLen)) + "...";
}

void logWhaleMLError(int lineNumber, std::string_view context, std::string_view wrong, std::string_view expected) {
    std::ostringstream oss;
    oss << "Line " << lineNumber;
    if (!context.empty())
        oss << " near '" << shortenForLog(context) << "'";
    oss << " | Wrong: " << wrong;
    oss << " | Expected: " << expected;
    Logger::log(LET_ERROR, "Orka, WhaleML Parser", oss.str());
}

void logWhaleMLWarning(int lineNumber, std::string_view context, std::string_view warning) {
    std::ostringstream oss;
    oss << "Line " << lineNumber;
    if (!context.empty())
        oss << " near '" << shortenForLog(context) << "'";
    oss << " | " << warning;
    Logger::log(LET_WARN, "Orka, WhaleML Parser", oss.str());
}

struct WhaleMLPreprocessedLine {
    int lineNumber = 0;
    int indentLevel = 0;
    std::string content;
    std::string original;
    bool objectHeader = false;
};

struct WhaleMLIndentInfo {
    int level = 0;
};

struct WhaleMLFormatStats {
    int meaningfulLines = 0;
    int semicolons = 0;
    int colonSeparators = 0;
    int equalsSeparators = 0;
    int compactSeparators = 0;
    int spacedSeparators = 0;
    int indentedLines = 0;
    int blankGroups = 0;
    int quotedTrimEligibleScalars = 0;
    int unquotedTrimEligibleScalars = 0;
};

std::string normalizeSmartQuotes(std::string_view value);
WhaleMLIndentInfo parseAntiRedundantIndent(std::string_view line, int lineNumber, std::string_view original);
void countTrimQuoteCandidates(std::string_view rawValue, WhaleMLFormatStats& stats);
bool canTrimQuotedScalar(std::string_view value);

bool isEscaped(std::string_view s, size_t pos) {
    if (pos == 0 || pos > s.size())
        return false;
    size_t backslashes = 0;
    size_t i = pos;
    while (i > 0 && s[i - 1] == '\\') {
        ++backslashes;
        --i;
    }
    return (backslashes % 2) != 0;
}

std::string trimString(std::string_view s) {
    const size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
        return {};
    const size_t end = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(start, end - start + 1));
}

enum class WhaleMLIncludeParseResult {
    NotInclude,
    Include,
    Error
};

std::filesystem::path normalizeWhaleMLIncludePath(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
    if (ec)
        absolutePath = path;
    return absolutePath.lexically_normal();
}

void skipWhaleMLIncludeSpaces(std::string_view line, std::size_t& pos) {
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\r'))
        ++pos;
}

WhaleMLIncludeParseResult parseWhaleMLIncludeDirective(std::string_view line, std::string& includePath, std::string& error) {
    std::size_t pos = line.find_first_not_of(" \t\r");
    if (pos == std::string_view::npos)
        return WhaleMLIncludeParseResult::NotInclude;

    constexpr std::string_view keyword = "include";
    if (line.substr(pos, keyword.size()) != keyword)
        return WhaleMLIncludeParseResult::NotInclude;

    pos += keyword.size();
    if (pos < line.size() && line[pos] != ' ' && line[pos] != '\t')
        return WhaleMLIncludeParseResult::NotInclude;

    skipWhaleMLIncludeSpaces(line, pos);
    if (pos >= line.size() || (line[pos] != '"' && line[pos] != '\'')) {
        error = "expected include \"file.whaleml\".";
        return WhaleMLIncludeParseResult::Error;
    }

    const char quote = line[pos++];
    const std::size_t start = pos;
    while (pos < line.size() && line[pos] != quote)
        ++pos;
    if (pos >= line.size()) {
        error = "unterminated include path.";
        return WhaleMLIncludeParseResult::Error;
    }

    includePath = std::string(line.substr(start, pos - start));
    ++pos;
    skipWhaleMLIncludeSpaces(line, pos);
    if (pos < line.size() && line.substr(pos, 2) != "//") {
        error = "unexpected text after include path.";
        return WhaleMLIncludeParseResult::Error;
    }
    if (includePath.empty()) {
        error = "empty include path.";
        return WhaleMLIncludeParseResult::Error;
    }

    return WhaleMLIncludeParseResult::Include;
}

std::string whaleMLIncludeStackString(const std::vector<std::filesystem::path>& stack, const std::filesystem::path& next) {
    std::ostringstream oss;
    for (const auto& path : stack) {
        if (oss.tellp() > 0)
            oss << " -> ";
        oss << path.string();
    }
    if (!stack.empty())
        oss << " -> ";
    oss << next.string();
    return oss.str();
}

bool expandWhaleMLIncludes(const std::filesystem::path& filePath, std::vector<std::filesystem::path>& includeStack,
                           std::string& output, bool& foundInclude) {
    const std::filesystem::path path = normalizeWhaleMLIncludePath(filePath);
    if (std::find(includeStack.begin(), includeStack.end(), path) != includeStack.end()) {
        Logger::log(LET_ERROR, "Orka, WhaleML Parser", "Recursive WhaleML include detected: " + whaleMLIncludeStackString(includeStack, path));
        return false;
    }

    std::ifstream file(path);
    if (!file) {
        const char* fileKind = includeStack.empty() ? "WhaleML file" : "WhaleML include file";
        Logger::log(LET_ERROR, "Orka, WhaleML Parser", "Could not load " + std::string(fileKind) + ": '" + path.string() + "'.");
        return false;
    }

    includeStack.push_back(path);

    bool success = true;
    int lineNumber = 0;
    std::string line;
    while (std::getline(file, line)) {
        ++lineNumber;

        std::string includePath;
        std::string error;
        const WhaleMLIncludeParseResult includeResult = parseWhaleMLIncludeDirective(line, includePath, error);
        if (includeResult == WhaleMLIncludeParseResult::Error) {
            std::ostringstream oss;
            oss << "Invalid WhaleML include in '" << path.string() << "' on line " << lineNumber << ": " << error;
            Logger::log(LET_ERROR, "Orka, WhaleML Parser", oss.str());
            success = false;
            continue;
        }
        if (includeResult == WhaleMLIncludeParseResult::Include) {
            foundInclude = true;
            std::filesystem::path childPath(includePath);
            if (childPath.is_relative())
                childPath = path.parent_path() / childPath;
            if (!expandWhaleMLIncludes(childPath, includeStack, output, foundInclude))
                success = false;
            continue;
        }

        output.append(line);
        output.push_back('\n');
    }

    includeStack.pop_back();
    return success;
}

std::filesystem::path getWhaleMLBufferIncludeBasePath() {
    std::error_code ec;
    std::filesystem::path basePath = std::filesystem::current_path(ec);
    if (ec)
        return ".";
    return basePath;
}

bool expandWhaleMLBufferIncludes(std::string_view data, const std::filesystem::path& basePath,
                                 std::vector<std::filesystem::path>& includeStack, std::string& output,
                                 bool& foundInclude) {
    std::istringstream iss{std::string(data)};
    std::string line;
    int lineNumber = 0;
    bool success = true;

    while (std::getline(iss, line)) {
        ++lineNumber;

        std::string includePath;
        std::string error;
        const WhaleMLIncludeParseResult includeResult = parseWhaleMLIncludeDirective(line, includePath, error);
        if (includeResult == WhaleMLIncludeParseResult::Error) {
            std::ostringstream oss;
            oss << "Invalid WhaleML include in buffer on line " << lineNumber << ": " << error;
            Logger::log(LET_ERROR, "Orka, WhaleML Parser", oss.str());
            success = false;
            continue;
        }
        if (includeResult == WhaleMLIncludeParseResult::Include) {
            foundInclude = true;
            std::filesystem::path childPath(includePath);
            if (childPath.is_relative())
                childPath = basePath / childPath;
            if (!expandWhaleMLIncludes(childPath, includeStack, output, foundInclude))
                success = false;
            continue;
        }

        output.append(line);
        output.push_back('\n');
    }

    return success;
}

bool isValidWhaleMLKeyPath(std::string_view keyPath) {
    if (keyPath.empty())
        return false;

    bool previousWasDot = false;
    for (std::size_t i = 0; i < keyPath.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(keyPath[i]);
        if (i == 0 && !(std::isalpha(c) || keyPath[i] == '_'))
            return false;
        if (!(std::isalnum(c) || keyPath[i] == '_' || keyPath[i] == '-' || keyPath[i] == '.'))
            return false;
        if (keyPath[i] == '.') {
            if (previousWasDot)
                return false;
            previousWasDot = true;
        } else {
            previousWasDot = false;
        }
    }

    return keyPath.back() != '.';
}

bool findFirstTopLevelSeparator(std::string_view line, std::size_t& pos, char& separator) {
    bool inQuotes = false;
    char quoteChar = 0;
    int depth = 0;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if ((c == '"' || c == '\'') && !isEscaped(line, i) && (quoteChar == 0 || quoteChar == c)) {
            inQuotes = !inQuotes;
            quoteChar = inQuotes ? c : 0;
            continue;
        }
        if (inQuotes)
            continue;
        if (c == '[') {
            ++depth;
            continue;
        }
        if (c == ']') {
            --depth;
            continue;
        }
        if (depth == 0 && (c == ':' || c == '=')) {
            pos = i;
            separator = c;
            return true;
        }
    }

    return false;
}

WhaleMLPrettyPrintOptions detectPrettyPrintOptions(std::string_view data) {
    std::istringstream iss{std::string(data)};
    std::string rawLine;
    WhaleMLFormatStats stats;
    bool previousMeaningfulLine = false;
    bool sawBlankAfterMeaningfulLine = false;

    while (std::getline(iss, rawLine)) {
        rawLine = normalizeSmartQuotes(rawLine);
        std::string line;
        bool inQuotes = false;
        char quoteChar = 0;
        for (std::size_t i = 0; i < rawLine.size(); ++i) {
            const char c = rawLine[i];
            if ((c == '"' || c == '\'') && !isEscaped(rawLine, i) && (quoteChar == 0 || quoteChar == c)) {
                inQuotes = !inQuotes;
                quoteChar = inQuotes ? c : 0;
            }
            if (!inQuotes && c == '/' && i + 1 < rawLine.size() && rawLine[i + 1] == '/')
                break;
            line.push_back(c);
        }

        if (trimString(line).empty()) {
            if (previousMeaningfulLine)
                sawBlankAfterMeaningfulLine = true;
            continue;
        }

        ++stats.meaningfulLines;
        if (sawBlankAfterMeaningfulLine)
            ++stats.blankGroups;
        sawBlankAfterMeaningfulLine = false;
        previousMeaningfulLine = true;

        int leadingSpaces = 0;
        bool hasLeadingIndent = false;
        for (char c : line) {
            if (c == '\t') {
                hasLeadingIndent = true;
                break;
            }
            if (c == ' ') {
                ++leadingSpaces;
                continue;
            }
            break;
        }
        if (hasLeadingIndent || leadingSpaces >= 4)
            ++stats.indentedLines;

        std::string segment;
        bool segmentQuotes = false;
        char segmentQuoteChar = 0;
        for (std::size_t i = 0; i <= line.size(); ++i) {
            const char c = i < line.size() ? line[i] : ';';
            if (i < line.size() && (c == '"' || c == '\'') && !isEscaped(line, i) &&
                (segmentQuoteChar == 0 || segmentQuoteChar == c)) {
                segmentQuotes = !segmentQuotes;
                segmentQuoteChar = segmentQuotes ? c : 0;
            }
            if (!segmentQuotes && c == ';') {
                if (i < line.size())
                    ++stats.semicolons;
                const std::string trimmedSegment = trimString(segment);
                std::size_t separatorPos = 0;
                char separator = 0;
                if (findFirstTopLevelSeparator(trimmedSegment, separatorPos, separator)) {
                    if (separator == '=')
                        ++stats.equalsSeparators;
                    else
                        ++stats.colonSeparators;

                    const bool leftSpace = separatorPos > 0 &&
                                           std::isspace(static_cast<unsigned char>(trimmedSegment[separatorPos - 1]));
                    const bool rightSpace = separatorPos + 1 < trimmedSegment.size() &&
                                            std::isspace(static_cast<unsigned char>(trimmedSegment[separatorPos + 1]));
                    if (leftSpace || rightSpace)
                        ++stats.spacedSeparators;
                    else
                        ++stats.compactSeparators;

                    const std::string trimmedValue = trimString(trimmedSegment.substr(separatorPos + 1));
                    countTrimQuoteCandidates(trimmedValue, stats);
                }
                segment.clear();
                continue;
            }
            if (i < line.size())
                segment.push_back(c);
        }
    }

    WhaleMLPrettyPrintOptions options = 0;
    if (stats.semicolons > 0 && stats.meaningfulLines <= 1)
        options |= WPO_SINGLELINE;
    if (stats.indentedLines > 0)
        options |= WPO_ANTI_REDUNDANT;
    if (stats.equalsSeparators > stats.colonSeparators)
        options |= WPO_USE_EQUALS_SEPARATOR;
    if (stats.blankGroups > 0)
        options |= WPO_TIDY;
    if (stats.compactSeparators > 0 && stats.compactSeparators >= stats.spacedSeparators)
        options |= WPO_NO_WHITESPACES;
    if (stats.unquotedTrimEligibleScalars > 0 &&
        stats.unquotedTrimEligibleScalars >= stats.quotedTrimEligibleScalars)
        options |= WPO_TRIM_QUOTES;
    return options;
}

WhaleMLIndentInfo parseAntiRedundantIndent(std::string_view line, int lineNumber, std::string_view original) {
    WhaleMLIndentInfo info;
    int pendingSpaces = 0;
    bool sawSpaces = false;
    bool sawTabs = false;

    for (char c : line) {
        if (c == ' ') {
            ++pendingSpaces;
            sawSpaces = true;
            continue;
        }
        if (c == '\t') {
            if (pendingSpaces != 0) {
                logWhaleMLWarning(lineNumber, original,
                                  "Anti-redundant indentation mixed spaces before a tab; the parser put on a helmet and rounded down.");
                info.level += pendingSpaces / 4;
                pendingSpaces = 0;
            }
            ++info.level;
            sawTabs = true;
            continue;
        }
        break;
    }

    if (pendingSpaces != 0) {
        info.level += pendingSpaces / 4;
        if ((pendingSpaces % 4) != 0) {
            std::ostringstream warning;
            warning << "Anti-redundant indentation has " << pendingSpaces
                    << " leading spaces; use tabs or groups of 4 spaces. The leftover spaces were politely ignored.";
            logWhaleMLWarning(lineNumber, original, warning.str());
        }
    }

    if (sawSpaces && sawTabs) {
        logWhaleMLWarning(lineNumber, original,
                          "Anti-redundant indentation mixes tabs and spaces. It parsed, but somewhere a style guide coughed.");
    }

    return info;
}

void replaceAll(std::string& s, std::string_view from, std::string_view to) {
    if (from.empty())
        return;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}

std::string normalizeSmartQuotes(std::string_view value) {
    std::string normalized(value);
    replaceAll(normalized, "\xE2\x80\x9C", "\""); // U+201C
    replaceAll(normalized, "\xE2\x80\x9D", "\""); // U+201D
    replaceAll(normalized, "\xE2\x80\x98", "'");  // U+2018
    replaceAll(normalized, "\xE2\x80\x99", "'");  // U+2019
    return normalized;
}

struct ParsedScalarToken {
    std::string value;
    bool wasQuoted = false;
};

ParsedScalarToken parseScalarToken(std::string_view rawValue) {
    const std::string normalized = normalizeSmartQuotes(trimString(rawValue));
    const std::string_view value(normalized);

    auto unescapeQuoted = [](std::string_view inner, char quoteChar) {
        std::string out;
        out.reserve(inner.size());
        bool escape = false;
        for (char c : inner) {
            if (escape) {
                if (c == quoteChar || c == '\\') {
                    out.push_back(c);
                } else {
                    out.push_back('\\');
                    out.push_back(c);
                }
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            out.push_back(c);
        }
        if (escape)
            out.push_back('\\');
        return out;
    };

    if (value.size() >= 2) {
        const bool doubleQuoted = value.front() == '"' && value.back() == '"';
        const bool singleQuoted = value.front() == '\'' && value.back() == '\'';
        if (doubleQuoted || singleQuoted) {
            const char quoteChar = value.front();
            return ParsedScalarToken{ unescapeQuoted(value.substr(1, value.size() - 2), quoteChar), true };
        }
    }

    static const std::string leftDouble = "\xE2\x80\x9C";  // U+201C
    static const std::string rightDouble = "\xE2\x80\x9D"; // U+201D
    static const std::string leftSingle = "\xE2\x80\x98";  // U+2018
    static const std::string rightSingle = "\xE2\x80\x99"; // U+2019

    auto hasUtf8WrappedQuotes = [](std::string_view s, std::string_view lq, std::string_view rq) -> bool {
        return s.size() >= lq.size() + rq.size() &&
               s.rfind(lq, 0) == 0 &&
               s.compare(s.size() - rq.size(), rq.size(), rq) == 0;
    };

    if (hasUtf8WrappedQuotes(value, leftDouble, rightDouble))
        return ParsedScalarToken{
            std::string(value.substr(leftDouble.size(), value.size() - leftDouble.size() - rightDouble.size())),
            true
        };
    if (hasUtf8WrappedQuotes(value, leftSingle, rightSingle))
        return ParsedScalarToken{
            std::string(value.substr(leftSingle.size(), value.size() - leftSingle.size() - rightSingle.size())),
            true
        };

    return ParsedScalarToken{ std::string(value), false };
}

void countTrimQuoteCandidates(std::string_view rawValue, WhaleMLFormatStats& stats) {
    std::string token;
    bool inQuotes = false;
    char quoteChar = 0;
    int depth = 0;

    auto recordTokenStyle = [&](std::string_view candidate) {
        const ParsedScalarToken parsed = parseScalarToken(candidate);
        if (!canTrimQuotedScalar(parsed.value))
            return;
        if (parsed.wasQuoted)
            ++stats.quotedTrimEligibleScalars;
        else
            ++stats.unquotedTrimEligibleScalars;
    };

    for (std::size_t i = 0; i < rawValue.size(); ++i) {
        const char c = rawValue[i];
        if ((c == '"' || c == '\'') && !isEscaped(rawValue, i) && (quoteChar == 0 || quoteChar == c)) {
            inQuotes = !inQuotes;
            quoteChar = inQuotes ? c : 0;
            token.push_back(c);
            continue;
        }

        if (!inQuotes) {
            if (c == '[') {
                ++depth;
            } else if (c == ']') {
                --depth;
            } else if (c == ',' && depth == 0) {
                recordTokenStyle(trimString(token));
                token.clear();
                continue;
            }
        }

        token.push_back(c);
    }

    recordTokenStyle(trimString(token));
}

bool hasTopLevelComma(std::string_view value) {
    bool inQuotes = false;
    char quoteChar = 0;
    int depth = 0;

    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if ((c == '"' || c == '\'') && !isEscaped(value, i) && (quoteChar == 0 || quoteChar == c)) {
            inQuotes = !inQuotes;
            quoteChar = inQuotes ? c : 0;
            continue;
        }

        if (inQuotes)
            continue;

        if (c == '[') {
            ++depth;
        } else if (c == ']') {
            --depth;
        } else if (c == ',' && depth == 0) {
            return true;
        }
    }

    return false;
}

bool isFullyBracketWrappedList(std::string_view value) {
    if (value.size() < 2 || value.front() != '[' || value.back() != ']')
        return false;

    bool inQuotes = false;
    char quoteChar = 0;
    int depth = 0;

    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if ((c == '"' || c == '\'') && !isEscaped(value, i) && (quoteChar == 0 || quoteChar == c)) {
            inQuotes = !inQuotes;
            quoteChar = inQuotes ? c : 0;
            continue;
        }
        if (inQuotes)
            continue;

        if (c == '[') {
            ++depth;
        } else if (c == ']') {
            --depth;
            if (depth < 0)
                return false;
            if (depth == 0 && i != value.size() - 1)
                return false;
        }
    }

    return depth == 0;
}

bool equalsIgnoreCaseAscii(std::string_view value, std::string_view lowerAsciiWord) {
    if (value.size() != lowerAsciiWord.size())
        return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));
        if (c != lowerAsciiWord[i])
            return false;
    }
    return true;
}

bool isBooleanLiteral(std::string_view value) {
    return equalsIgnoreCaseAscii(value, "true") || equalsIgnoreCaseAscii(value, "false");
}

bool isNumericLiteral(std::string_view value) {
    if (value.empty())
        return false;

    std::size_t i = 0;
    if (value[i] == '+' || value[i] == '-')
        ++i;
    if (i >= value.size())
        return false;

    bool hasDigitsBeforeDot = false;
    while (i < value.size() && std::isdigit(static_cast<unsigned char>(value[i]))) {
        hasDigitsBeforeDot = true;
        ++i;
    }

    bool hasDigitsAfterDot = false;
    if (i < value.size() && value[i] == '.') {
        ++i;
        while (i < value.size() && std::isdigit(static_cast<unsigned char>(value[i]))) {
            hasDigitsAfterDot = true;
            ++i;
        }
    }

    if (!hasDigitsBeforeDot && !hasDigitsAfterDot)
        return false;

    if (i < value.size() && (value[i] == 'e' || value[i] == 'E')) {
        ++i;
        if (i < value.size() && (value[i] == '+' || value[i] == '-'))
            ++i;

        bool hasExponentDigits = false;
        while (i < value.size() && std::isdigit(static_cast<unsigned char>(value[i]))) {
            hasExponentDigits = true;
            ++i;
        }
        if (!hasExponentDigits)
            return false;
    }

    return i == value.size();
}

bool isEnumLikeLiteral(std::string_view value) {
    if (value.empty())
        return false;

    const auto isAlphaOrUnderscore = [](char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    };
    const auto isAlnumOrUnderscore = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    };

    if (!isAlphaOrUnderscore(value[0]))
        return false;
    for (std::size_t i = 1; i < value.size(); ++i) {
        if (!isAlnumOrUnderscore(value[i]))
            return false;
    }
    return true;
}

bool shouldSerializeQuoted(std::string_view value) {
    if (value.empty())
        return true;
    if (isBooleanLiteral(value) || isNumericLiteral(value) || isEnumLikeLiteral(value))
        return false;
    return true;
}

bool containsWhitespace(std::string_view value) {
    for (char c : value) {
        if (std::isspace(static_cast<unsigned char>(c)))
            return true;
    }
    return false;
}

bool canTrimQuotedScalar(std::string_view value) {
    if (value.empty() || containsWhitespace(value))
        return false;

    for (char c : value) {
        if (c == ',' || c == '[' || c == ']' || c == ';')
            return false;
    }

    for (std::size_t i = 0; i + 1 < value.size(); ++i) {
        if (value[i] == '/' && value[i + 1] == '/')
            return false;
    }

    return true;
}

void appendEscapedForDoubleQuotes(std::string_view value, std::string& out) {
    for (char c : value) {
        if (c == '\\' || c == '"')
            out.push_back('\\');
        out.push_back(c);
    }
}

void appendSerializedScalar(std::string_view value, bool forceQuote, std::string& out, WhaleMLPrettyPrintOptions options) {
    if (forceQuote &&
        hasWhaleMLPrettyPrintOption(options, WPO_TRIM_QUOTES) &&
        canTrimQuotedScalar(value)) {
        forceQuote = false;
    }

    if (!forceQuote && !shouldSerializeQuoted(value)) {
        out.append(value);
        return;
    }
    out.push_back('"');
    appendEscapedForDoubleQuotes(value, out);
    out.push_back('"');
}

void appendSerializedList(const WhaleMLList& list, std::string& out, bool wrapInBrackets, WhaleMLPrettyPrintOptions options) {
    if (wrapInBrackets)
        out.push_back('[');

    bool first = true;
    auto appendSeparator = [&]() {
        if (!first)
            out.append(hasWhaleMLPrettyPrintOption(options, WPO_NO_WHITESPACES) ? "," : ", ");
        first = false;
    };

    for (const auto& valPtr : list.getItems()) {
        appendSeparator();
        const std::string value = valPtr ? valPtr->asString() : "";
        const bool forceQuote = valPtr && valPtr->wasQuoted();
        appendSerializedScalar(value, forceQuote, out, options);
    }

    for (const auto& [index, sublist] : list.getSublists()) {
        (void)index;
        appendSeparator();
        appendSerializedList(sublist, out, true, options);
    }

    if (wrapInBrackets)
        out.push_back(']');
}

struct WhaleMLSerializedEntry {
    std::string keyPath;
    const WhaleMLNode* node = nullptr;
    bool hasSerializationOrder = false;
    std::size_t serializationOrder = 0;
};

bool serializedEntryLess(const WhaleMLSerializedEntry& a, const WhaleMLSerializedEntry& b) {
    if (a.hasSerializationOrder != b.hasSerializationOrder)
        return a.hasSerializationOrder > b.hasSerializationOrder;
    if (a.hasSerializationOrder && a.serializationOrder != b.serializationOrder)
        return a.serializationOrder < b.serializationOrder;
    return a.keyPath < b.keyPath;
}

void collectSerializedEntries(const WhaleMLNode* node, std::vector<WhaleMLSerializedEntry>& entries, std::string& keyPath) {
    if (!node)
        return;

    node->forEachChildOrdered([&](std::string_view key, const WhaleMLNode* childNode) {
        if (!childNode)
            return;

        const std::size_t previousKeyPathSize = keyPath.size();
        if (!keyPath.empty())
            keyPath.push_back('.');
        keyPath += key;

        if (childNode->isList()) {
            const WhaleMLList& list = static_cast<const WhaleMLList&>(*childNode);
            const bool hasListPayload = list.isExplicitList() ||
                                        !list.getItems().empty() ||
                                        !list.getSublists().empty();
            if (hasListPayload) {
                entries.push_back({
                    keyPath,
                    childNode,
                    childNode->hasSerializationOrder(),
                    childNode->getSerializationOrder()
                });
            }
            if (!list.getChildren().empty())
                collectSerializedEntries(&list, entries, keyPath);
        } else {
            const WhaleMLValue& value = static_cast<const WhaleMLValue&>(*childNode);
            if (value.getValue().isValid()) {
                entries.push_back({
                    keyPath,
                    childNode,
                    childNode->hasSerializationOrder(),
                    childNode->getSerializationOrder()
                });
            }
            if (!value.getChildren().empty())
                collectSerializedEntries(&value, entries, keyPath);
        }

        keyPath.resize(previousKeyPathSize);
    });
}

bool hasSerializablePayload(const WhaleMLNode* node) {
    if (!node)
        return false;
    if (node->isList()) {
        const WhaleMLList& list = static_cast<const WhaleMLList&>(*node);
        return list.isExplicitList() || !list.getItems().empty() || !list.getSublists().empty();
    }

    const WhaleMLValue& value = static_cast<const WhaleMLValue&>(*node);
    return value.getValue().isValid();
}

void appendSerializedEntryLine(std::string_view key, const WhaleMLNode& node, std::string& output,
                               WhaleMLPrettyPrintOptions options) {
    output.append(key);
    if (hasWhaleMLPrettyPrintOption(options, WPO_NO_WHITESPACES)) {
        output.push_back(hasWhaleMLPrettyPrintOption(options, WPO_USE_EQUALS_SEPARATOR) ? '=' : ':');
    } else {
        output.push_back(' ');
        output.push_back(hasWhaleMLPrettyPrintOption(options, WPO_USE_EQUALS_SEPARATOR) ? '=' : ':');
        output.push_back(' ');
    }

    if (node.isList()) {
        appendSerializedList(static_cast<const WhaleMLList&>(node), output, false, options);
    } else {
        const WhaleMLValue& valueNode = static_cast<const WhaleMLValue&>(node);
        appendSerializedScalar(valueNode.getValue().asString(), valueNode.getValue().wasQuoted(), output, options);
    }
}

std::string firstPathSegment(std::string_view keyPath) {
    const std::size_t dot = keyPath.find('.');
    return std::string(dot == std::string_view::npos ? keyPath : keyPath.substr(0, dot));
}

void appendFlatSerializedEntries(const std::vector<WhaleMLSerializedEntry>& entries, std::string& output,
                                 WhaleMLPrettyPrintOptions options) {
    std::string previousGroup;
    bool wroteEntry = false;
    for (const auto& entry : entries) {
        if (!entry.node)
            continue;

        if (wroteEntry && hasWhaleMLPrettyPrintOption(options, WPO_TIDY)) {
            const std::string group = firstPathSegment(entry.keyPath);
            if (group != previousGroup)
                output.push_back('\n');
            previousGroup = group;
        } else if (!wroteEntry) {
            previousGroup = firstPathSegment(entry.keyPath);
        }

        appendSerializedEntryLine(entry.keyPath, *entry.node, output, options);
        output.push_back('\n');
        wroteEntry = true;
    }
}

void appendAntiRedundantSerializedNode(const WhaleMLNode* node, std::string_view pathPrefix, int indentLevel,
                                       std::string& output, WhaleMLPrettyPrintOptions options) {
    if (!node)
        return;

    std::string previousGroup;
    bool wroteEntryAtLevel = false;
    node->forEachChildOrdered([&](std::string_view key, const WhaleMLNode* childNode) {
        if (!childNode)
            return;

        std::string effectiveKey(pathPrefix);
        if (!effectiveKey.empty())
            effectiveKey.push_back('.');
        effectiveKey += key;

        const bool payload = hasSerializablePayload(childNode);
        if (payload) {
            if (wroteEntryAtLevel && indentLevel == 0 && hasWhaleMLPrettyPrintOption(options, WPO_TIDY)) {
                const std::string group = firstPathSegment(effectiveKey);
                if (group != previousGroup)
                    output.push_back('\n');
                previousGroup = group;
            } else if (!wroteEntryAtLevel && indentLevel == 0) {
                previousGroup = firstPathSegment(effectiveKey);
            }

            output.append(static_cast<std::size_t>(indentLevel) * 4, ' ');
            appendSerializedEntryLine(effectiveKey, *childNode, output, options);
            output.push_back('\n');
            wroteEntryAtLevel = true;
        }

        if (!childNode->getChildren().empty()) {
            if (payload) {
                appendAntiRedundantSerializedNode(childNode, {}, indentLevel + 1, output, options);
            } else {
                appendAntiRedundantSerializedNode(childNode, effectiveKey, indentLevel, output, options);
            }
        }
    });
}

std::unique_ptr<WhaleMLNode> parseWhaleMLData(std::string_view data, bool warnOnDuplicateKeys) {
    const WhaleMLPrettyPrintOptions autodetectedOptions = detectPrettyPrintOptions(data);
    std::istringstream iss{std::string(data)};
    std::string rawLine;
    std::vector<WhaleMLPreprocessedLine> preprocessedLines;
    int lineNumber = 0;

    while (std::getline(iss, rawLine)) {
        ++lineNumber;
        rawLine = normalizeSmartQuotes(rawLine);
        std::string line;
        bool inQuotes = false;
        char quoteChar = 0;
        for (size_t i = 0; i < rawLine.size(); ++i) {
            char c = rawLine[i];
            if ((c == '"' || c == '\'') && !isEscaped(rawLine, i) && (quoteChar == 0 || quoteChar == c)) {
                inQuotes = !inQuotes;
                quoteChar = inQuotes ? c : 0;
            }
            if (!inQuotes && c == '/' && i + 1 < rawLine.size() && rawLine[i + 1] == '/') break;
            line += c;
        }
        if (inQuotes) {
            logWhaleMLError(lineNumber, rawLine,
                            "Unterminated quoted string while stripping comments (you opened a quote and ghosted it).",
                            "Close the quote before the line ends.");
        }
        if (trimString(line).empty()) continue;
        const WhaleMLIndentInfo indentInfo = parseAntiRedundantIndent(line, lineNumber, rawLine);

        std::vector<std::string> segments;
        {
            std::string cur;
            bool q = false;
            char qChar = 0;
            for (size_t i = 0; i < line.size(); ++i) {
                char c = line[i];
                if ((c == '"' || c == '\'') && !isEscaped(line, i) && (qChar == 0 || qChar == c)) {
                    q = !q;
                    qChar = q ? c : 0;
                }
                if (!q && c == ';') {
                    if (!cur.empty()) segments.push_back(cur);
                    cur.clear();
                } else cur += c;
            }
            if (q) {
                logWhaleMLError(lineNumber, line,
                                "Unterminated quoted string while splitting ';' statements (semicolon splitter is judging your life choices).",
                                "A closing quote before the ';' or line end.");
            }
            if (!cur.empty()) segments.push_back(cur);
        }

        for (auto& seg : segments) {
            const std::string originalSegment = trimString(seg);
            std::string cleaned;
            bool q = false;
            char qChar = 0;
            for (size_t i = 0; i < seg.size(); ++i) {
                char c = seg[i];
                if ((c == '"' || c == '\'') && !isEscaped(seg, i) && (qChar == 0 || qChar == c)) {
                    q = !q;
                    qChar = q ? c : 0;
                }
                if (!q && std::isspace(static_cast<unsigned char>(c))) continue;
                cleaned += c;
            }
            if (q) {
                logWhaleMLError(lineNumber, originalSegment,
                                "Unterminated quoted string while removing whitespace (the trimmer resigned mid-shift).",
                                "A closing quote in this statement.");
            }
            if (cleaned.empty()) continue;

            std::string normalized;
            q = false;
            qChar = 0;
            for (size_t i = 0; i < cleaned.size(); ++i) {
                char c = cleaned[i];
                if ((c == '"' || c == '\'') && !isEscaped(cleaned, i) && (qChar == 0 || qChar == c)) {
                    q = !q;
                    qChar = q ? c : 0;
                }
                if (!q && (c == ':' || c == '=')) {
                    normalized += ':';
                    continue;
                }
                normalized += c;
            }
            if (q) {
                logWhaleMLError(lineNumber, originalSegment,
                                "Unterminated quoted string while normalizing ':'/'=' (separator scan rage-quit).",
                                "A closing quote before the statement ends.");
            }
            if (!normalized.empty()) {
                if (!normalized.contains(':')) {
                    if (isValidWhaleMLKeyPath(normalized)) {
                        preprocessedLines.push_back({lineNumber, indentInfo.level, normalized + ":", originalSegment, true});
                    } else {
                        logWhaleMLError(lineNumber, originalSegment,
                                        "Missing key/value separator (apparently the parser should read your mind now).",
                                        "Use `key : value`, `key = value`, or an anti-redundant object header like `key`.");
                    }
                    continue;
                }
                preprocessedLines.push_back({lineNumber, indentInfo.level, normalized, originalSegment, false});
            }
        }
    }

    auto splitKeyPath = [](std::string_view keyPath) {
        std::vector<std::string> parts;
        std::string cur;
        for (char c : keyPath) {
            if (c == '.') {
                if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
            } else cur += c;
        }
        if (!cur.empty()) parts.push_back(cur);
        return parts;
    };

    std::function<std::unique_ptr<WhaleMLList>(std::string_view, int, std::string_view)> parseList;
    parseList = [&](std::string_view str, int sourceLine, std::string_view keyPath) -> std::unique_ptr<WhaleMLList> {
        auto list = std::make_unique<WhaleMLList>();
        list->setExplicitList(true);
        std::string token;
        bool inQuotes = false;
        char quoteChar = 0;
        int depth = 0;
        int subIndex = 0;

        auto pushToken = [&]() {
            std::string t = trimString(token);
            if (t.empty()) {
                token.clear();
                return;
            }
            ParsedScalarToken parsed = parseScalarToken(t);
            list->getItems().push_back(std::make_unique<WhaleMLScalarValue>(parsed.value, true, parsed.wasQuoted));
            token.clear();
        };

        for (size_t i = 0; i < str.size(); ++i) {
            char c = str[i];
            if ((c == '"' || c == '\'') && !isEscaped(str, i) && (quoteChar == 0 || quoteChar == c)) {
                inQuotes = !inQuotes;
                quoteChar = inQuotes ? c : 0;
                token += c;
                continue;
            }

            if (!inQuotes) {
                if (c == '[') {
                    depth++;
                    if (depth == 1) { token.clear(); continue; }
                } else if (c == ']') {
                    if (depth == 0) {
                        logWhaleMLError(sourceLine, keyPath,
                                        "Unexpected closing bracket `]` in list value (extra bracket, bonus confusion).",
                                        "A scalar token, `[` starting a sublist, or a comma separator.");
                        continue;
                    }
                    depth--;
                    if (depth == 0) {
                        auto sublist = parseList(token, sourceLine, keyPath);
                        list->getSublists()[subIndex++] = std::move(*sublist);
                        token.clear();
                        continue;
                    }
                } else if (c == ',' && depth == 0) {
                    pushToken();
                    continue;
                }
            }
            token += c;
        }

        if (inQuotes) {
            logWhaleMLError(sourceLine, keyPath,
                            "Unterminated quoted string in list value (string left on read).",
                            "Close the quote before the list item ends.");
        }
        if (depth != 0) {
            logWhaleMLError(sourceLine, keyPath,
                            "Unbalanced list brackets (one bracket escaped containment).",
                            "Matching `[` and `]` pairs.");
        }

        pushToken();
        return list;
    };

    std::unique_ptr<WhaleMLNode> root = std::make_unique<WhaleMLList>(); // root can hold any
    root->setAutodetectedPrettyPrintOptions(autodetectedOptions);
    std::regex kvRe(R"(^\s*([A-Za-z_][A-Za-z0-9_\.\-]*)\s*:\s*(.*?)\s*$)");
    std::smatch m;
    std::unordered_map<std::string, int> firstSeenLineByPath;
    std::vector<std::string> antiRedundantPathStack;

    for (const auto& preprocessedLine : preprocessedLines) {
        const std::string& line = preprocessedLine.content;
        if (!std::regex_match(line, m, kvRe)) {
            logWhaleMLError(preprocessedLine.lineNumber, preprocessedLine.original,
                            "Invalid statement syntax (this line is freestyling, not parsing).",
                            "A statement like `key.path : value`.");
            continue;
        }
        std::string keyPath = m[1];
        std::string value = trimString(m[2].str());
        if (keyPath.empty()) {
            logWhaleMLError(preprocessedLine.lineNumber, preprocessedLine.original,
                            "Empty key path (bold strategy, no key at all).",
                            "A non-empty key like `physics.body.type`.");
            continue;
        }
        if (keyPath.back() == '.' || keyPath.contains("..")) {
            logWhaleMLError(preprocessedLine.lineNumber, preprocessedLine.original,
                            "Malformed key path (empty path segment detected; dots are separators, not confetti).",
                            "A dot path like `parent.child.value` with a real key between every dot.");
            continue;
        }

        int effectiveIndentLevel = preprocessedLine.indentLevel;
        if (effectiveIndentLevel > static_cast<int>(antiRedundantPathStack.size())) {
            std::ostringstream warning;
            warning << "Anti-redundant indentation jumped to level " << effectiveIndentLevel
                    << " while only " << antiRedundantPathStack.size()
                    << " parent level(s) exist. The parser installed floaties and attached it to the deepest known parent.";
            logWhaleMLWarning(preprocessedLine.lineNumber, preprocessedLine.original, warning.str());
            effectiveIndentLevel = static_cast<int>(antiRedundantPathStack.size());
        }
        if (antiRedundantPathStack.size() > static_cast<std::size_t>(effectiveIndentLevel))
            antiRedundantPathStack.resize(static_cast<std::size_t>(effectiveIndentLevel));

        if (effectiveIndentLevel > 0) {
            const std::string& parentPath = antiRedundantPathStack[static_cast<std::size_t>(effectiveIndentLevel - 1)];
            if (parentPath.empty()) {
                logWhaleMLWarning(preprocessedLine.lineNumber, preprocessedLine.original,
                                  "Anti-redundant indentation found an empty parent path. That is not a family tree, that is fog.");
            } else {
                keyPath = parentPath + "." + keyPath;
            }
        }

        if (antiRedundantPathStack.size() == static_cast<std::size_t>(effectiveIndentLevel))
            antiRedundantPathStack.push_back(keyPath);
        else
            antiRedundantPathStack[static_cast<std::size_t>(effectiveIndentLevel)] = keyPath;

        bool isExactDuplicatePath = false;
        if (!preprocessedLine.objectHeader) {
            auto seenIt = firstSeenLineByPath.find(keyPath);
            if (seenIt != firstSeenLineByPath.end()) {
                isExactDuplicatePath = true;
                if (warnOnDuplicateKeys) {
                    std::ostringstream warning;
                    warning << "Duplicate key '" << keyPath << "' overwrites previous value from line "
                            << seenIt->second << ".";
                    logWhaleMLWarning(preprocessedLine.lineNumber, preprocessedLine.original, warning.str());
                }
            } else {
                firstSeenLineByPath.emplace(keyPath, preprocessedLine.lineNumber);
            }
        }

        WhaleMLNode* node = root.get();
        auto parts = splitKeyPath(keyPath);
        if (parts.empty()) {
            logWhaleMLError(preprocessedLine.lineNumber, preprocessedLine.original,
                            "Could not resolve key path segments (the splitter stared into the void).",
                            "Dot-separated key parts such as `player.inventory`.");
            continue;
        }

        std::string currentPath;
        for (size_t i = 0; i + 1 < parts.size(); ++i) {
            const std::string& k = parts[i];
            if (!currentPath.empty())
                currentPath.push_back('.');
            currentPath += k;
            WhaleMLNode* child = node->getChild(k);
            if (!child) {
                auto listNode = std::make_unique<WhaleMLList>();
                child = listNode.get();
                node->setChild(k, std::move(listNode));
            }
            node = child;
        }

        std::string_view leaf = parts.back();
        if (preprocessedLine.objectHeader) {
            WhaleMLNode* existingLeaf = node->getChild(leaf);
            if (!existingLeaf) {
                auto listNode = std::make_unique<WhaleMLList>();
                node->setChild(leaf, std::move(listNode));
            } else if (!existingLeaf->isList()) {
                auto listNode = std::make_unique<WhaleMLList>();
                listNode->copyChildrenFrom(*existingLeaf);
                node->setChild(leaf, std::move(listNode));
            }
            continue;
        }

        if (warnOnDuplicateKeys && !isExactDuplicatePath) {
            WhaleMLNode* existingLeaf = node->getChild(leaf);
            if (existingLeaf) {
                std::ostringstream warning;
                warning << "Key '" << keyPath << "' overwrites an existing "
                        << (existingLeaf->isList() ? "list/object" : "scalar") << ".";
                logWhaleMLWarning(preprocessedLine.lineNumber, keyPath, warning.str());
            }
        }

        if (isFullyBracketWrappedList(value)) {
            std::string inner = value.substr(1, value.size() - 2);
            auto parsed = parseList(inner, preprocessedLine.lineNumber, keyPath);
            if (WhaleMLNode* existingLeaf = node->getChild(leaf)) {
                parsed->copyChildrenFrom(*existingLeaf);
            }
            node->setChild(leaf, std::move(parsed));
        } else if (hasTopLevelComma(value)) {
            auto parsed = parseList(value, preprocessedLine.lineNumber, keyPath);
            if (WhaleMLNode* existingLeaf = node->getChild(leaf)) {
                parsed->copyChildrenFrom(*existingLeaf);
            }
            node->setChild(leaf, std::move(parsed));
        } else {
            ParsedScalarToken parsed = parseScalarToken(value);
            WhaleMLNode* existingLeaf = node->getChild(leaf);
            if (existingLeaf && !existingLeaf->isList()) {
                WhaleMLValue& existingValue = static_cast<WhaleMLValue&>(*existingLeaf);
                existingValue = WhaleMLScalarValue(parsed.value, true, parsed.wasQuoted);
            } else {
                auto val = std::make_unique<WhaleMLValue>();
                *val = WhaleMLScalarValue(parsed.value, true, parsed.wasQuoted);
                if (existingLeaf) {
                    val->copyChildrenFrom(*existingLeaf);
                }
                node->setChild(leaf, std::move(val));
            }
        }
    }

    return root;
}

std::unique_ptr<WhaleMLNode> WhaleMLParser::parse(std::string_view data) {
    std::string content;
    bool foundInclude = false;
    std::vector<std::filesystem::path> includeStack;
    if (!expandWhaleMLBufferIncludes(data, getWhaleMLBufferIncludeBasePath(), includeStack, content, foundInclude))
        throw std::runtime_error("WhaleML include expansion failed.");
    return parseWhaleMLData(content, !foundInclude);
}

std::unique_ptr<WhaleMLNode> WhaleMLParser::parseFile(std::string_view filepath) {
    std::string content;
    bool foundInclude = false;
    std::vector<std::filesystem::path> includeStack;
    if (!expandWhaleMLIncludes(std::filesystem::path(std::string(filepath)), includeStack, content, foundInclude))
        throw std::runtime_error("WhaleML include expansion failed.");
    return parseWhaleMLData(content, !foundInclude);
}

std::string WhaleMLParser::serialize(const std::unique_ptr<WhaleMLNode>& root) {
    if (root)
        return serialize(root, root->getAutodetectedPrettyPrintOptions());
    return {};
}

std::string WhaleMLParser::serialize(const std::unique_ptr<WhaleMLNode>& root, WhaleMLPrettyPrintOptions options) {
    if (!root)
        return {};

    if (hasWhaleMLPrettyPrintOption(options, WPO_SINGLELINE)) {
        options &= ~static_cast<WhaleMLPrettyPrintOptions>(WPO_ANTI_REDUNDANT);
        options &= ~static_cast<WhaleMLPrettyPrintOptions>(WPO_TIDY);
    }

    std::vector<WhaleMLSerializedEntry> entries;
    entries.reserve(128);
    std::string keyPath;
    collectSerializedEntries(root.get(), entries, keyPath);
    if (entries.empty())
        return {};

    if (!std::ranges::is_sorted(entries, serializedEntryLess))
        std::ranges::sort(entries, serializedEntryLess);

    std::string output;
    output.reserve(entries.size() * 40);
    if (hasWhaleMLPrettyPrintOption(options, WPO_ANTI_REDUNDANT)) {
        appendAntiRedundantSerializedNode(root.get(), {}, 0, output, options);
    } else {
        appendFlatSerializedEntries(entries, output, options);
    }

    if (hasWhaleMLPrettyPrintOption(options, WPO_SINGLELINE)) {
        std::string singleLine;
        singleLine.reserve(output.size());
        bool pendingSeparator = false;
        const std::string separator = hasWhaleMLPrettyPrintOption(options, WPO_NO_WHITESPACES) ? ";" : "; ";
        for (char c : output) {
            if (c == '\r' || c == '\n') {
                pendingSeparator = !singleLine.empty();
                continue;
            }
            if (pendingSeparator) {
                singleLine.append(separator);
                pendingSeparator = false;
            }
            singleLine.push_back(c);
        }
        output = std::move(singleLine);
    }
    return output;
}


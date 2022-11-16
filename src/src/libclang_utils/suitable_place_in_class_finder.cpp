/**
 * @file	finder.cpp
 * @brief	Implements the core of the suitable place in class finder.
 */
#include <clang/Lex/Lexer.h>

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base_error.hpp"
#include "common_types.hpp"
#include "file_grepper.hpp"
#include "libclang_utils/suitable_place_in_class_finder.hpp"
#include "clang/Basic/TokenKinds.h"

using namespace clang;
using namespace Tsepepe;

// --------------------------------------------------------------------------------------------------------------------
// Private declaration
// --------------------------------------------------------------------------------------------------------------------
struct SuitablePlaceInClassFinder
{
    explicit SuitablePlaceInClassFinder(const std::string& cpp_file_content,
                                        const clang::CXXRecordDecl* node,
                                        const clang::SourceManager& source_manager) :
        cpp_file_content{cpp_file_content}, record{node}, source_manager{source_manager}
    {
    }

    SuitablePublicMethodPlaceInCppFile find() const
    {
        SourceLocation result;
        bool is_public_section_needed{false};
        if (auto maybe_after_public_method{find_location_after_last_public_method_in_the_first_chain()};
            maybe_after_public_method)
            result = *maybe_after_public_method;

        return {.offset = get_insert_offset_after_location(result),
                .is_public_section_needed = is_public_section_needed};
    }

  private:
    std::optional<SourceLocation> find_location_after_last_public_method_in_the_first_chain() const
    {
        auto is_public_explicit_method{[](const CXXMethodDecl* method) {
            return method->getAccess() == AccessSpecifier::AS_public and not method->isImplicit();
        }};

        auto is_not_public_or_is_public_implicit_method{[&](const CXXMethodDecl* method) {
            return not is_public_explicit_method(method);
        }};

        auto beg{record->method_begin()};
        auto end{record->method_end()};

        auto first_public_method_it{std::ranges::find_if(beg, end, is_public_explicit_method)};
        if (first_public_method_it == end)
            return {};

        // Ideally, we could use find_if() to find the first non-public method, and then use std::prev(), but the method
        // range is a forward range.
        auto last_public_method_in_first_public_method_chain_it{first_public_method_it};
        for (auto it{first_public_method_it}; it != end; ++it)
        {
            if (is_not_public_or_is_public_implicit_method(*it))
                break;
            last_public_method_in_first_public_method_chain_it = it;
        }

        return last_public_method_in_first_public_method_chain_it->getEndLoc();
    }

    unsigned get_insert_offset_after_location(SourceLocation location) const
    {
        auto first_token_after_begin_location{Lexer::findNextToken(location, source_manager, record->getLangOpts())};
        auto begin_location{unpack_source_location_from_token(first_token_after_begin_location)};
        auto current_location{begin_location};

        if (first_token_after_begin_location->is(tok::semi))
        {
            for (unsigned i{0}; i < 1000; ++i)
            {
                auto tok{Lexer::findNextToken(current_location, source_manager, record->getLangOpts())};
                if (not tok.hasValue())
                    break;
                current_location = tok->getLocation();
                if (tok->isNot(tok::semi))
                    break;
            }
        }
        auto end_location{current_location};

        auto begin_offset{source_manager.getFileOffset(begin_location)};
        auto end_offset{source_manager.getFileOffset(end_location)};

        auto begin_it{std::next(std::begin(cpp_file_content), begin_offset)};
        auto end_it{std::next(std::begin(cpp_file_content), end_offset)};

        auto newline_place_it{std::find(begin_it, end_it, '\n')};
        if (newline_place_it != end_it)
            return std::distance(std::begin(cpp_file_content), newline_place_it) + 1;
        else
            return end_offset;
    }

    SourceLocation unpack_source_location_from_token(const Optional<Token>& token) const
    {
        if (not token.hasValue())
            throw Tsepepe::BaseError{"Can't unpack token; token empty!"};

        auto loc{token->getLocation()};
        if (not loc.isValid())
            throw Tsepepe::BaseError{"Can't unpack token; token holds invalid location!"};
        return loc;
    }

    const std::string& cpp_file_content;
    const clang::CXXRecordDecl* record;
    const clang::SourceManager& source_manager;
};

static unsigned get_insert_offset_after_location(const std::string& file_content, SourceLocation);
static std::optional<unsigned> get_offset_past_last_public_method_in_first_public_method_chain(
    const std::string& cpp_file_content, const CXXRecordDecl* record, const SourceManager& source_manager);
static const CXXMethodDecl* find_last_public_method_in_first_method_chain(const CXXRecordDecl*);
static std::optional<unsigned>
try_find_line_with_public_section(const std::string& cpp_file_content, const CXXRecordDecl*, const SourceManager&);
static unsigned find_line_with_opening_bracket(const CXXRecordDecl*, const SourceManager&);

// --------------------------------------------------------------------------------------------------------------------
// Public stuff
// --------------------------------------------------------------------------------------------------------------------
#include <iostream>

Tsepepe::SuitablePublicMethodPlaceInCppFile Tsepepe::find_suitable_place_in_class_for_public_method(
    const std::string& cpp_file_content, const clang::CXXRecordDecl* node, const clang::SourceManager& source_manager)
{
    return SuitablePlaceInClassFinder{cpp_file_content, node, source_manager}.find();
    if (auto maybe_offset_past_last_method_in_first_public_method_chain{
            get_offset_past_last_public_method_in_first_public_method_chain(cpp_file_content, node, source_manager)};
        maybe_offset_past_last_method_in_first_public_method_chain)
        return {.offset = *maybe_offset_past_last_method_in_first_public_method_chain};
    else if (auto maybe_first_public_section{try_find_line_with_public_section(cpp_file_content, node, source_manager)};
             maybe_first_public_section)
    {
        return {.offset = *maybe_first_public_section};
    } else
    {
        SuitablePublicMethodPlaceInCppFile result;
        result.offset = find_line_with_opening_bracket(node, source_manager);
        result.is_public_section_needed = not node->isStruct();
        return result;
    }
}

#include "libclang_utils/misc_utils.hpp"

// --------------------------------------------------------------------------------------------------------------------
// Private implementations
// --------------------------------------------------------------------------------------------------------------------
static unsigned get_insert_offset_after_location(const std::string& file_content, SourceLocation location)
{
}

static std::optional<unsigned> get_offset_past_last_public_method_in_first_public_method_chain(
    const std::string& cpp_file_content, const CXXRecordDecl* record, const SourceManager& source_manager)
{
    auto method{find_last_public_method_in_first_method_chain(record)};
    if (method == nullptr)
        return {};

    auto end_source_loc{method->getEndLoc()};
    auto offset{source_manager.getFileOffset(end_source_loc)};
    std::cout << "YOLO: offset: " << offset << ' ' << cpp_file_content[offset] << std::endl;
    auto newline_place{cpp_file_content.find('\n', offset)};

    auto current_location{end_source_loc};
    for (unsigned i{0}; i < 10; ++i)
    {
        auto tok{Lexer::findNextToken(current_location, source_manager, record->getLangOpts())};
        if (not tok.hasValue())
            break;
        Tsepepe::dump_token(*tok);
        current_location = tok->getLocation();
    }
    // FIXME: this is wrong! Add more test cases to check for declaration vs definiton, and match regex against end
    // of line, if not end of line then return offset past the declaration/definition! Maybe use Lexer, to find a new
    // token? Skip newlines / do not skip them? What does it mean?
    if (newline_place == std::string::npos)
    {
        if (cpp_file_content[offset + 1] == ';')
            return offset + 2;
        else
            return offset + 1;
    } else
    {
        return newline_place + 1;
    }
}

static const CXXMethodDecl* find_last_public_method_in_first_method_chain(const CXXRecordDecl* record)
{
    auto is_public_explicit_method{[](const CXXMethodDecl* method) {
        return method->getAccess() == AccessSpecifier::AS_public and not method->isImplicit();
    }};

    auto is_not_public_or_is_public_implicit_method{[&](const CXXMethodDecl* method) {
        return not is_public_explicit_method(method);
    }};

    auto beg{record->method_begin()};
    auto end{record->method_end()};

    auto first_public_method_it{std::ranges::find_if(beg, end, is_public_explicit_method)};
    if (first_public_method_it == end)
        return nullptr;

    // Ideally, we could use find_if() to find the first non-public method, and then use std::prev(), but the method
    // range is a forward range.
    auto last_public_method_in_first_public_method_chain_it{first_public_method_it};
    for (auto it{first_public_method_it}; it != end; ++it)
    {
        if (is_not_public_or_is_public_implicit_method(*it))
            break;
        last_public_method_in_first_public_method_chain_it = it;
    }

    return *last_public_method_in_first_public_method_chain_it;
}

static std::optional<unsigned> try_find_line_with_public_section(const std::string& cpp_file_content,
                                                                 const CXXRecordDecl* record,
                                                                 const SourceManager& source_manager)
{
    auto get_class_body_begin_end_lines{[&]() {
        // FIXME: Use PresumedSourceRange instead of the clang::Lexer module.
        auto class_body_source_range{record->getSourceRange()};
        auto class_body_past_end_loc{class_body_source_range.getEnd()};
        auto class_body_end_loc{
            Lexer::getLocForEndOfToken(class_body_past_end_loc, 0, source_manager, record->getLangOpts())};
        auto class_body_begin_line{source_manager.getSpellingLineNumber(class_body_source_range.getBegin())};
        auto class_body_end_line{source_manager.getSpellingLineNumber(class_body_end_loc)};
        return std::make_pair(class_body_begin_line, class_body_end_line);
    }};

    auto get_line_nums_with_public_access_specifier_declarations{[&]() -> std::vector<unsigned> {
        return Tsepepe::grep_file(cpp_file_content, Tsepepe::RustRegexPattern{"public\\s*:"});
    }};

    auto class_body_line_range{get_class_body_begin_end_lines()};
    auto is_line_within_class_body{[&](unsigned line) {
        return line >= class_body_line_range.first and line <= class_body_line_range.second;
    }};

    auto lines_with_public_access_specifier_declarations{get_line_nums_with_public_access_specifier_declarations()};
    auto public_section_within_the_class_it{
        std::ranges::find_if(lines_with_public_access_specifier_declarations, is_line_within_class_body)};
    if (public_section_within_the_class_it != std::end(lines_with_public_access_specifier_declarations))
        return *public_section_within_the_class_it;
    else
        return {};
}

static unsigned find_line_with_opening_bracket(const CXXRecordDecl* record, const SourceManager& source_manager)
{
    auto location{record->getBeginLoc()};
    while (true)
    {
        auto maybe_token{Lexer::findNextToken(location, source_manager, record->getLangOpts())};
        if (not maybe_token)
            break;

        const auto& token{*maybe_token};
        location = token.getLocation();
        if (token.getKind() == tok::TokenKind::l_brace)
            break;
    }

    return source_manager.getSpellingLineNumber(location);
}

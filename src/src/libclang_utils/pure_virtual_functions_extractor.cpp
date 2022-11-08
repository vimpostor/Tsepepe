/**
 * @file	pure_virtual_functions_extractor.cpp
 * @brief	Implements the pure virtual functions extractor.
 */

#include <regex>

#include "libclang_utils/full_function_declaration_expander.hpp"
#include "libclang_utils/pure_virtual_functions_extractor.hpp"

using namespace clang;

// --------------------------------------------------------------------------------------------------------------------
// Private declarations
// --------------------------------------------------------------------------------------------------------------------

// --------------------------------------------------------------------------------------------------------------------
// Public stuff
// --------------------------------------------------------------------------------------------------------------------
Tsepepe::OverrideDeclarations
Tsepepe::pure_virtual_functions_to_override_declarations(const clang::CXXRecordDecl* node,
                                                         const clang::SourceManager& source_manager)
{
    OverrideDeclarations override_declarations;

    auto append_override_declaration{[&](const CXXMethodDecl* method) {
        auto declaration{Tsepepe::fully_expand_function_declaration(method, source_manager)};
        auto interface_nesting_prefix{method->getParent()->getQualifiedNameAsString() + "::"};
        declaration = std::regex_replace(declaration, std::regex{interface_nesting_prefix}, "");
        declaration.append(" override;");
        override_declarations.emplace_back(std::move(declaration));
        // TODO
        // 3. Shortify nesting from the namespace(s), the derived class is in.
    }};

    auto collect_override_declarations{[&](const clang::CXXRecordDecl* record) {
        for (auto method : record->methods())
            if (method->isPure())
                append_override_declaration(method);
    }};

    // The actual story begins here ...
    node->forallBases([&](const CXXRecordDecl* base) {
        collect_override_declarations(base);
        return true;
    });
    collect_override_declarations(node);
    return override_declarations;
}

// --------------------------------------------------------------------------------------------------------------------
// Private definitions
// --------------------------------------------------------------------------------------------------------------------

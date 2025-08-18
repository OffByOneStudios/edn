#pragma once
#include <string>
#include <string_view>

namespace rustlite {

struct ParseResult {
    bool success{false};
    std::string edn;           // Lowered EDN text (stub for now)
    std::string error_message; // If !success, human-readable message
    int line{0};
    int column{0};
};

class Parser {
public:
    // Parse Rustlite source text and produce EDN.
    // v0: accepts whitespace/comments-only input and yields an empty module.
    ParseResult parse_string(std::string_view src, std::string_view filename = "<memory>") const;
};

} // namespace rustlite

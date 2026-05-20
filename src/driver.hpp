#pragma once

#include "ast.hpp"
#include "parser.hpp"

#include <memory>
#include <string>
#include <vector>

namespace mini {

class Driver {
public:
    Driver() = default;

    bool parseFile(const std::string& inputPath);

    void setProgram(std::unique_ptr<Program> parsedProgram);
    std::unique_ptr<Program> takeProgram();
    const Program* program() const;

    mini::Parser::location_type consumeLocation(const char* text);
    mini::Parser::location_type currentParserLocation() const;
    SourceLocation toSourceLocation(const mini::Parser::location_type& location) const;
    SourceLocation currentSourceLocation() const;

    void parseError(const mini::Parser::location_type& location, const std::string& message);
    void lexError(const SourceLocation& location, const std::string& message);

    const std::vector<std::string>& errors() const;
    bool hasErrors() const;

private:
    void advance(const char* text);
    std::string formatLocation(const SourceLocation& location) const;

    std::unique_ptr<Program> program_;
    std::vector<std::string> errors_;
    std::string filename_ = "<input>";
    int line_ = 1;
    int column_ = 1;
};

} // namespace mini

#include "driver.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>

extern FILE* yyin;
extern void yyrestart(FILE* inputFile);

namespace mini {

bool Driver::parseFile(const std::string& inputPath) {
    errors_.clear();
    program_.reset();
    filename_ = inputPath;
    line_ = 1;
    column_ = 1;

    FILE* input = std::fopen(inputPath.c_str(), "rb");
    if (!input) {
        errors_.push_back(inputPath + ": error: cannot open input file");
        return false;
    }

    yyin = input;
    yyrestart(input);

    mini::Parser parser(*this);
    const int result = parser.parse();

    std::fclose(input);
    yyin = nullptr;

    return result == 0 && !hasErrors() && program_ != nullptr;
}

void Driver::setProgram(std::unique_ptr<Program> parsedProgram) {
    program_ = std::move(parsedProgram);
}

std::unique_ptr<Program> Driver::takeProgram() {
    return std::move(program_);
}

const Program* Driver::program() const {
    return program_.get();
}

mini::Parser::location_type Driver::consumeLocation(const char* text) {
    mini::Parser::location_type location;
    location.begin.filename = &filename_;
    location.end.filename = &filename_;
    location.begin.line = line_;
    location.begin.column = column_;

    advance(text);

    location.end.line = line_;
    location.end.column = column_;
    return location;
}

mini::Parser::location_type Driver::currentParserLocation() const {
    mini::Parser::location_type location;
    location.begin.filename = &filename_;
    location.end.filename = &filename_;
    location.begin.line = line_;
    location.begin.column = column_;
    location.end.line = line_;
    location.end.column = column_;
    return location;
}

SourceLocation Driver::toSourceLocation(const mini::Parser::location_type& location) const {
    const std::string file = location.begin.filename ? *location.begin.filename : filename_;
    return SourceLocation(file, location.begin.line, location.begin.column);
}

SourceLocation Driver::currentSourceLocation() const {
    return SourceLocation(filename_, line_, column_);
}

void Driver::parseError(const mini::Parser::location_type& location, const std::string& message) {
    const SourceLocation sourceLocation = toSourceLocation(location);
    errors_.push_back(formatLocation(sourceLocation) + ": syntax error: " + message);
}

void Driver::lexError(const SourceLocation& location, const std::string& message) {
    errors_.push_back(formatLocation(location) + ": lexical error: " + message);
}

const std::vector<std::string>& Driver::errors() const {
    return errors_;
}

bool Driver::hasErrors() const {
    return !errors_.empty();
}

void Driver::advance(const char* text) {
    const std::size_t length = std::strlen(text);
    for (std::size_t i = 0; i < length; ++i) {
        const char c = text[i];
        if (c == '\r') {
            if (i + 1 < length && text[i + 1] == '\n') {
                ++i;
            }
            ++line_;
            column_ = 1;
        } else if (c == '\n') {
            ++line_;
            column_ = 1;
        } else {
            ++column_;
        }
    }
}

std::string Driver::formatLocation(const SourceLocation& location) const {
    std::ostringstream stream;
    if (!location.file.empty()) {
        stream << location.file << ':';
    }
    stream << location.line << ':' << location.column;
    return stream.str();
}

} // namespace mini

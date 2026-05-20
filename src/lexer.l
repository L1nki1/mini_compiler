%option noyywrap
%option nounput
%option noinput
%option batch
%option nodefault

%x COMMENT

%{
#include "driver.hpp"
#include "parser.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>

#define YY_DECL mini::Parser::symbol_type yylex(mini::Driver& driver)
%}

%%

[ \t\v\f]+ {
    driver.consumeLocation(yytext);
}

\r?\n {
    driver.consumeLocation(yytext);
}

"//"[^\r\n]* {
    driver.consumeLocation(yytext);
}

"/*" {
    driver.consumeLocation(yytext);
    BEGIN(COMMENT);
}

<COMMENT>"*/" {
    driver.consumeLocation(yytext);
    BEGIN(INITIAL);
}

<COMMENT>\r?\n {
    driver.consumeLocation(yytext);
}

<COMMENT>. {
    driver.consumeLocation(yytext);
}

<COMMENT><<EOF>> {
    driver.lexError(driver.currentSourceLocation(), "unterminated block comment");
    return mini::Parser::make_END(driver.currentParserLocation());
}

"int" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_KW_INT(loc);
}

"float" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_KW_FLOAT(loc);
}

"if" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_KW_IF(loc);
}

"else" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_KW_ELSE(loc);
}

"for" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_KW_FOR(loc);
}

"do" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_KW_DO(loc);
}

"while" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_KW_WHILE(loc);
}

"return" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_KW_RETURN(loc);
}

"break" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_KW_BREAK(loc);
}

"continue" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_KW_CONTINUE(loc);
}

"==" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_EQ(loc);
}

"!=" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_NE(loc);
}

"<=" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_LE(loc);
}

">=" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_GE(loc);
}

"&&" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_AND(loc);
}

"||" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_OR(loc);
}

"=" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_ASSIGN(loc);
}

"<" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_LT(loc);
}

">" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_GT(loc);
}

"+" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_PLUS(loc);
}

"-" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_MINUS(loc);
}

"*" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_STAR(loc);
}

"/" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_SLASH(loc);
}

"%" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_PERCENT(loc);
}

"!" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_NOT(loc);
}

"(" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_LPAREN(loc);
}

")" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_RPAREN(loc);
}

"{" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_LBRACE(loc);
}

"}" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_RBRACE(loc);
}

";" {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_SEMICOLON(loc);
}

"," {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_COMMA(loc);
}

[0-9]+ {
    auto loc = driver.consumeLocation(yytext);
    errno = 0;
    char* end = nullptr;
    const long long parsed = std::strtoll(yytext, &end, 10);
    if (errno == ERANGE || end == yytext || *end != '\0') {
        driver.lexError(driver.toSourceLocation(loc), "integer literal is out of range");
        return mini::Parser::make_INVALID(loc);
    }
    return mini::Parser::make_INTEGER(static_cast<std::int64_t>(parsed), loc);
}

[A-Za-z_][A-Za-z0-9_]* {
    auto loc = driver.consumeLocation(yytext);
    return mini::Parser::make_IDENTIFIER(std::string(yytext), loc);
}

. {
    auto loc = driver.consumeLocation(yytext);
    driver.lexError(driver.toSourceLocation(loc), "unknown character '" + std::string(yytext, yyleng) + "'");
    return mini::Parser::make_INVALID(loc);
}

<<EOF>> {
    return mini::Parser::make_END(driver.currentParserLocation());
}

%%

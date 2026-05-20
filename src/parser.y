%skeleton "lalr1.cc"
%require "3.5"
%defines
%define api.namespace {mini}
%define api.parser.class {Parser}
%define api.value.type variant
%define api.token.constructor
%define parse.error verbose
%locations

%code requires {
    #include "ast.hpp"

    #include <memory>
    #include <string>
    #include <vector>

    namespace mini {
    class Driver;
    }
}

%code {
    #include "driver.hpp"

    mini::Parser::symbol_type yylex(mini::Driver& driver);
}

%parse-param { mini::Driver& driver }
%lex-param { mini::Driver& driver }

%token END 0 "end of file"
%token INVALID "invalid token"

%token KW_INT "int"
%token KW_FLOAT "float"
%token KW_IF "if"
%token KW_ELSE "else"
%token KW_FOR "for"
%token KW_DO "do"
%token KW_WHILE "while"
%token KW_RETURN "return"
%token KW_BREAK "break"
%token KW_CONTINUE "continue"

%token <std::string> IDENTIFIER "identifier"
%token <std::int64_t> INTEGER "integer literal"

%token EQ "=="
%token NE "!="
%token LE "<="
%token GE ">="
%token AND "&&"
%token OR "||"
%token ASSIGN "="
%token LT "<"
%token GT ">"
%token PLUS "+"
%token MINUS "-"
%token STAR "*"
%token SLASH "/"
%token PERCENT "%"
%token NOT "!"
%token LPAREN "("
%token RPAREN ")"
%token LBRACE "{"
%token RBRACE "}"
%token SEMICOLON ";"
%token COMMA ","

%left OR
%left AND
%left EQ NE
%nonassoc LT LE GT GE
%left PLUS MINUS
%left STAR SLASH PERCENT
%right NOT UMINUS
%precedence IF_WITHOUT_ELSE
%precedence KW_ELSE

%type <std::vector<std::unique_ptr<FunctionDecl>>> function_list
%type <std::unique_ptr<FunctionDecl>> function_decl
%type <std::vector<Param>> params_opt param_list
%type <Param> param
%type <TypeKind> type
%type <std::unique_ptr<BlockStmt>> block
%type <std::vector<StmtPtr>> statement_list
%type <StmtPtr> statement var_decl_stmt var_decl_no_semicolon assign_stmt assign_no_semicolon if_stmt for_stmt do_while_stmt return_stmt break_stmt continue_stmt expr_stmt for_init for_step
%type <ExprPtr> opt_initializer for_cond expr

%start translation_unit

%%

translation_unit:
    function_list END
    {
        SourceLocation loc = $1.empty() ? driver.currentSourceLocation() : $1.front()->loc;
        driver.setProgram(std::make_unique<Program>(std::move(loc), std::move($1)));
    }
;

function_list:
    function_decl
    {
        $$ = std::vector<std::unique_ptr<FunctionDecl>>();
        $$.push_back(std::move($1));
    }
  | function_list function_decl
    {
        $$ = std::move($1);
        $$.push_back(std::move($2));
    }
;

function_decl:
    type IDENTIFIER LPAREN params_opt RPAREN block
    {
        $$ = std::make_unique<FunctionDecl>(
            driver.toSourceLocation(@1), $1, std::move($2), std::move($4), std::move($6));
    }
;

params_opt:
    %empty
    {
        $$ = std::vector<Param>();
    }
  | param_list
    {
        $$ = std::move($1);
    }
;

param_list:
    param
    {
        $$ = std::vector<Param>();
        $$.push_back(std::move($1));
    }
  | param_list COMMA param
    {
        $$ = std::move($1);
        $$.push_back(std::move($3));
    }
;

param:
    type IDENTIFIER
    {
        $$ = Param(driver.toSourceLocation(@1), $1, std::move($2));
    }
;

type:
    KW_INT
    {
        $$ = TypeKind::Int;
    }
  | KW_FLOAT
    {
        $$ = TypeKind::Float;
    }
;

block:
    LBRACE statement_list RBRACE
    {
        $$ = std::make_unique<BlockStmt>(driver.toSourceLocation(@1), std::move($2));
    }
;

statement_list:
    %empty
    {
        $$ = std::vector<StmtPtr>();
    }
  | statement_list statement
    {
        $$ = std::move($1);
        $$.push_back(std::move($2));
    }
;

statement:
    var_decl_stmt
    {
        $$ = std::move($1);
    }
  | assign_stmt
    {
        $$ = std::move($1);
    }
  | if_stmt
    {
        $$ = std::move($1);
    }
  | for_stmt
    {
        $$ = std::move($1);
    }
  | do_while_stmt
    {
        $$ = std::move($1);
    }
  | return_stmt
    {
        $$ = std::move($1);
    }
  | break_stmt
    {
        $$ = std::move($1);
    }
  | continue_stmt
    {
        $$ = std::move($1);
    }
  | expr_stmt
    {
        $$ = std::move($1);
    }
  | block
    {
        $$ = std::move($1);
    }
;

var_decl_stmt:
    var_decl_no_semicolon SEMICOLON
    {
        $$ = std::move($1);
    }
;

var_decl_no_semicolon:
    type IDENTIFIER opt_initializer
    {
        $$ = std::make_unique<VarDeclStmt>(
            driver.toSourceLocation(@1), $1, std::move($2), std::move($3));
    }
;

opt_initializer:
    %empty
    {
        $$ = nullptr;
    }
  | ASSIGN expr
    {
        $$ = std::move($2);
    }
;

assign_stmt:
    assign_no_semicolon SEMICOLON
    {
        $$ = std::move($1);
    }
;

assign_no_semicolon:
    IDENTIFIER ASSIGN expr
    {
        $$ = std::make_unique<AssignStmt>(driver.toSourceLocation(@1), std::move($1), std::move($3));
    }
;

if_stmt:
    KW_IF LPAREN expr RPAREN block %prec IF_WITHOUT_ELSE
    {
        $$ = std::make_unique<IfStmt>(
            driver.toSourceLocation(@1), std::move($3), std::move($5), nullptr);
    }
  | KW_IF LPAREN expr RPAREN block KW_ELSE block
    {
        $$ = std::make_unique<IfStmt>(
            driver.toSourceLocation(@1), std::move($3), std::move($5), std::move($7));
    }
;

for_stmt:
    KW_FOR LPAREN for_init SEMICOLON for_cond SEMICOLON for_step RPAREN block
    {
        $$ = std::make_unique<ForStmt>(
            driver.toSourceLocation(@1), std::move($3), std::move($5), std::move($7), std::move($9));
    }
;

for_init:
    %empty
    {
        $$ = nullptr;
    }
  | var_decl_no_semicolon
    {
        $$ = std::move($1);
    }
  | assign_no_semicolon
    {
        $$ = std::move($1);
    }
  | expr
    {
        $$ = std::make_unique<ExprStmt>(driver.toSourceLocation(@1), std::move($1));
    }
;

for_cond:
    %empty
    {
        $$ = nullptr;
    }
  | expr
    {
        $$ = std::move($1);
    }
;

for_step:
    %empty
    {
        $$ = nullptr;
    }
  | assign_no_semicolon
    {
        $$ = std::move($1);
    }
  | expr
    {
        $$ = std::make_unique<ExprStmt>(driver.toSourceLocation(@1), std::move($1));
    }
;

do_while_stmt:
    KW_DO block KW_WHILE LPAREN expr RPAREN SEMICOLON
    {
        $$ = std::make_unique<DoWhileStmt>(
            driver.toSourceLocation(@1), std::move($2), std::move($5));
    }
;

return_stmt:
    KW_RETURN expr SEMICOLON
    {
        $$ = std::make_unique<ReturnStmt>(driver.toSourceLocation(@1), std::move($2));
    }
;

break_stmt:
    KW_BREAK SEMICOLON
    {
        $$ = std::make_unique<BreakStmt>(driver.toSourceLocation(@1));
    }
;

continue_stmt:
    KW_CONTINUE SEMICOLON
    {
        $$ = std::make_unique<ContinueStmt>(driver.toSourceLocation(@1));
    }
;

expr_stmt:
    expr SEMICOLON
    {
        $$ = std::make_unique<ExprStmt>(driver.toSourceLocation(@1), std::move($1));
    }
;

expr:
    INTEGER
    {
        $$ = std::make_unique<IntLiteralExpr>(driver.toSourceLocation(@1), $1);
    }
  | IDENTIFIER
    {
        $$ = std::make_unique<VarExpr>(driver.toSourceLocation(@1), std::move($1));
    }
  | LPAREN expr RPAREN
    {
        $$ = std::move($2);
    }
  | MINUS expr %prec UMINUS
    {
        $$ = std::make_unique<UnaryExpr>(driver.toSourceLocation(@1), UnaryOp::Negate, std::move($2));
    }
  | NOT expr
    {
        $$ = std::make_unique<UnaryExpr>(driver.toSourceLocation(@1), UnaryOp::LogicalNot, std::move($2));
    }
  | expr PLUS expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::Add, std::move($1), std::move($3));
    }
  | expr MINUS expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::Subtract, std::move($1), std::move($3));
    }
  | expr STAR expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::Multiply, std::move($1), std::move($3));
    }
  | expr SLASH expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::Divide, std::move($1), std::move($3));
    }
  | expr PERCENT expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::Modulo, std::move($1), std::move($3));
    }
  | expr EQ expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::Equal, std::move($1), std::move($3));
    }
  | expr NE expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::NotEqual, std::move($1), std::move($3));
    }
  | expr LT expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::Less, std::move($1), std::move($3));
    }
  | expr LE expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::LessEqual, std::move($1), std::move($3));
    }
  | expr GT expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::Greater, std::move($1), std::move($3));
    }
  | expr GE expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::GreaterEqual, std::move($1), std::move($3));
    }
  | expr AND expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::LogicalAnd, std::move($1), std::move($3));
    }
  | expr OR expr
    {
        $$ = std::make_unique<BinaryExpr>(driver.toSourceLocation(@1), BinaryOp::LogicalOr, std::move($1), std::move($3));
    }
;

%%

void mini::Parser::error(const location_type& location, const std::string& message) {
    driver.parseError(location, message);
}

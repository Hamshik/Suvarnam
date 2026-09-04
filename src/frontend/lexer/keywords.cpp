#include "frontend/lexer/keywords.h"

#include <string_view>
#include <unordered_map>

#include "Parser.h"
typedef Parser::Tokens_ Tokens;

#define TOKEN_KEYWORD(word, token_name) \
    { word, { word, SA_KEYWORD_TOKEN, token_name, UNKNOWN, false } }
#define TYPE_KEYWORD(word, type_name) \
    { word, { word, SA_KEYWORD_DATATYPE, Parser::DATATYPES, type_name, false } }
#define BOOL_KEYWORD(word, value) \
    { word, { word, SA_KEYWORD_BOOL_LITERAL, Parser::BOOL_LITERAL, UNKNOWN, value } }

static const std::unordered_map<std::string_view, SA_Keyword> SA_keywords = {
    TOKEN_KEYWORD("#import", Tokens::IMPORT),
    TOKEN_KEYWORD("else", Tokens::ELSE),
    TOKEN_KEYWORD("if", Tokens::IF),
    TOKEN_KEYWORD("for", Tokens::FOR),
    TOKEN_KEYWORD("var", Tokens::VAR),
    TYPE_KEYWORD("i8", I8),
    TYPE_KEYWORD("i16", I16),
    TYPE_KEYWORD("i32", I32),
    TYPE_KEYWORD("i64", I64),
    TYPE_KEYWORD("i128", I128),
    TYPE_KEYWORD("u8", U8),
    TYPE_KEYWORD("u16", U16),
    TYPE_KEYWORD("u32", U32),
    TYPE_KEYWORD("u64", U64),
    TYPE_KEYWORD("u128", U128),
    TYPE_KEYWORD("f32", F32),
    TYPE_KEYWORD("f64", F64),
    TYPE_KEYWORD("f128", F128),
    TYPE_KEYWORD("uf32", UF32),
    TYPE_KEYWORD("uf64", UF64),
    TYPE_KEYWORD("uf128", UF128),
    TYPE_KEYWORD("str", STRINGS),
    TYPE_KEYWORD("char", CHARACTER),
    TYPE_KEYWORD("bool", BOOL),
    TYPE_KEYWORD("void", VOID),
    TOKEN_KEYWORD("while", Tokens::WHILE),
    TOKEN_KEYWORD("in", Tokens::IN),
    TOKEN_KEYWORD("break", Tokens::BREAK),
    TOKEN_KEYWORD("continue", Tokens::CONTINUE),
    BOOL_KEYWORD("true", true),
    BOOL_KEYWORD("false", false),
    TOKEN_KEYWORD("mut", Tokens::MUT),
    TOKEN_KEYWORD("fn", Tokens::FN),
    TOKEN_KEYWORD("ret", Tokens::RETURN),
    TOKEN_KEYWORD("return", Tokens::RETURN),
};

const SA_Keyword *SA_find_keyword(const char *text, size_t len)
{
    if (!text) return nullptr;

    const auto keyword = SA_keywords.find(std::string_view{text, len});
    return keyword == SA_keywords.end() ? nullptr : &keyword->second;
}

bool SA_is_keyword(const char *text, size_t len)
{
    return SA_find_keyword(text, len) != nullptr;
}

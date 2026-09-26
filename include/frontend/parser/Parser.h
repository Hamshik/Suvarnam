#ifndef Parser_h_included
#define Parser_h_included

#include "Parserbase.h"

#undef Parser

class Scanner;

#define null(ty) static_cast<ty>(nullptr)

class Parser: public ParserBase
{
    Scanner& d_scanner;
    Scanner& scanner;
    const char* ErrMsg{};

    public:
        int parse();
        explicit Parser(Scanner& sc)
            : d_scanner(sc), scanner(sc)
        {}

    private:
        void error();
        int lex();
        void print();
        void exceptionHandler(std::exception const &exc);

        void executeAction_(int ruleNr);
        void errorRecovery_();
        void nextCycle_();
        void nextToken_();
        void print_();
};

#endif
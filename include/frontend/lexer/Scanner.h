#ifndef Scanner_H_INCLUDED_
#define Scanner_H_INCLUDED_

// $insert baseclass_h
#include "Scannerbase.h"
#include "Parserbase.h"
#include "lexer/keywords.h"

#define VAL(type, action) val->assign<Tag_::type>(action)


// $insert classHead
class Scanner: public ScannerBase
{
    ParserBase::STYPE_* val = nullptr;
    ParserBase::LTYPE_* loc = nullptr;
    ParserBase::LTYPE_ openDelimPos = {0};
    bool lexErrPending = false;
    int braceDepth = 0;

    public:
        explicit Scanner(std::istream &in = std::cin, std::ostream &out = std::cout, bool keepCwd = true);

        Scanner(std::string const &infile, std::string const &outfile, bool keepCwd = true);
        
        // $insert lexFunctionDecl
        int lex(ParserBase::STYPE_* val, ParserBase::LTYPE_* loc);
        void setOpenDelimPos(const ParserBase::LTYPE_ &pos) { openDelimPos = pos; }
        void setOpenDelimPos(const ParserBase::LTYPE_ *pos) { if (pos) openDelimPos = *pos; }
        ParserBase::LTYPE_ getCursor() const { return openDelimPos; }
        bool lexTakeErr(void);
        void lexMarkErr(void);
        static const SA_Keyword *findKeyword(const char *text, size_t len);

    private:
        int lex_();
        int executeAction_(size_t ruleNr);

        void print();
        void preCode();
                            // be exec'ed before the patternmatching starts

        void postCode(PostEnum_ type);
                            // re-implement this function for code that must 
                    static char* unescapeStr(const char *in, size_t in_len, size_t *out_len, int *err_index, const char **err_msg);
                    bool isSingleChar(const char *bytes, size_t len);
                    void updateLoc(const char *text, int len);
                    static int convetHexToInt(unsigned char c);
};

#endif // Scanner_H_INCLUDED_


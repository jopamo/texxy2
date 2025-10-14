#ifndef HIGHLIGHTER_H
#define HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QSet>
#include <QList>
#include <QHash>
#include <QColor>
#include <QTextBlockUserData>
#include <QTextCursor>

namespace Texxy {

struct ParenthesisInfo {
    char character;
    int position;
};

struct BraceInfo {
    char character;
    int position;
};

struct BracketInfo {
    char character;
    int position;
};

class TextBlockData : public QTextBlockUserData {
   public:
    TextBlockData()
        : Highlighted(false),
          Property(false),
          LastState(0),
          OpenNests(0),
          LastFormattedQuote(0),
          LastFormattedRegex(0) {}
    ~TextBlockData();

    QList<ParenthesisInfo*> parentheses() const;
    QList<BraceInfo*> braces() const;
    QList<BracketInfo*> brackets() const;
    QString labelInfo() const;
    bool isHighlighted() const;
    bool getProperty() const;
    int lastState() const;
    int openNests() const;
    int lastFormattedQuote() const;
    int lastFormattedRegex() const;
    QSet<int> openQuotes() const;

    void insertInfo(ParenthesisInfo* info);
    void insertInfo(BraceInfo* info);
    void insertInfo(BracketInfo* info);
    void insertInfo(const QString& str);
    void setHighlighted();
    void setProperty(bool p);
    void setLastState(int state);
    void insertNestInfo(int nests);
    void insertLastFormattedQuote(int last);
    void insertLastFormattedRegex(int last);
    void insertOpenQuotes(const QSet<int>& openQuotes);

   private:
    QList<ParenthesisInfo*> allParentheses;
    QList<BraceInfo*> allBraces;
    QList<BracketInfo*> allBrackets;
    QString label;
    bool Highlighted;
    bool Property;
    int LastState;
    int OpenNests;
    int LastFormattedQuote;
    int LastFormattedRegex;
    QSet<int> OpenQuotes;
};

class Highlighter : public QSyntaxHighlighter {
    Q_OBJECT

   public:
    Highlighter(QTextDocument* parent,
                const QString& lang,
                const QTextCursor& start,
                const QTextCursor& end,
                bool darkColorScheme,
                bool showWhiteSpace,
                bool showEndings,
                int whitespaceValue,
                const QHash<QString, QColor>& syntaxColors = QHash<QString, QColor>());
    ~Highlighter();

    void setLimit(const QTextCursor& start, const QTextCursor& end) {
        startCursor = start;
        endCursor = end;
    }

   protected:
    void highlightBlock(const QString& text) override;

   private:
    QStringList keywords(const QString& lang);
    QStringList types();
    bool isEscapedChar(const QString& text, int pos) const;
    bool isEscapedQuote(const QString& text, int pos, bool isStartQuote, bool skipCommandSign = false);
    bool isQuoted(const QString& text, int index, bool skipCommandSign = false, int start = 0);
    bool isPerlQuoted(const QString& text, int index);
    bool isJSQuoted(const QString& text, int index);
    bool isMLCommented(const QString& text, int index, int comState = commentState, int start = 0);
    bool isHereDocument(const QString& text);
    void pythonMLComment(const QString& text, int indx);
    void htmlCSSHighlighter(const QString& text, int start = 0);
    void htmlBrackets(const QString& text, int start = 0);
    void htmlJavascript(const QString& text);
    bool isCSSCommented(const QString& text,
                        const QList<int>& valueRegions,
                        int index,
                        int prevQuote = 0,
                        bool prevUrl = false);
    int isQuotedInCSSValue(const QString& text, int valueStart, int index, int prevQuote = 0, bool prevUrl = false);
    bool isInsideCSSValueUrl(const QString& text, int valueStart, int index, int prevQuote = 0, bool prevUrl = false);
    void formatAttrSelectors(const QString& text, int start, int pos);
    bool isInsideAttrSelector(const QString& text, int pos, int start);
    void cssHighlighter(const QString& text, bool mainFormatting, int start = 0);
    void singleLineComment(const QString& text, int start);
    bool multiLineComment(const QString& text,
                          int index,
                          const QRegularExpression& commentStartExp,
                          const QRegularExpression& commentEndExp,
                          int commState,
                          const QTextCharFormat& comFormat);
    bool textEndsWithBackSlash(const QString& text) const;
    bool multiLineQuote(const QString& text, int start = 0, int comState = commentState);
    void multiLinePerlQuote(const QString& text);
    void multiLineJSQuote(const QString& text, int start, int comState);
    void setFormatWithoutOverwrite(int start,
                                   int count,
                                   const QTextCharFormat& newFormat,
                                   const QTextCharFormat& oldFormat);

    void SH_MultiLineQuote(const QString& text);
    bool SH_SkipQuote(const QString& text, int pos, bool isStartQuote);
    int formatInsideCommand(const QString& text,
                            int minOpenNests,
                            int& nests,
                            QSet<int>& quotes,
                            bool isHereDocStart,
                            int index);
    bool SH_CmndSubstVar(const QString& text,
                         TextBlockData* currentBlockData,
                         int oldOpenNests,
                         const QSet<int>& oldOpenQuotes);

    void highlightUrlsWithinQuote(const QString& text, int start, int length);

    void handleSingleQuote(const QString& text,
                           int& currentIndex,
                           bool inComment,
                           bool doubleQuoted,
                           bool isHereDocStart,
                           int& nestCount);

    void handleDoubleQuote(const QString& text, int& currentIndex, bool inComment, bool& doubleQuoted);

    void handleDollarSign(const QString& text,
                          int& currentIndex,
                          bool inComment,
                          bool doubleQuoted,
                          bool isHereDocStart,
                          int& parenDepth,
                          int& nestCount,
                          QSet<int>& quotes);

    void handleOpenParenthesis(int& currentIndex, bool doubleQuoted, bool inComment, int& parenDepth);

    bool handleCloseParenthesis(int& currentIndex,
                                bool doubleQuoted,
                                bool inComment,
                                int& parenDepth,
                                int& nestCount,
                                int initialOpenNests,
                                QSet<int>& quotes);

    void handleCommentSign(const QString& text, int& currentIndex, bool& inComment, bool doubleQuoted);

    void handleDefaultChar(int& currentIndex, bool inComment, bool doubleQuoted);

    int closeOpenQuoteFromPreviousBlock(const QString& text, int prevState, bool isHereDocStart);

    void debControlFormatting(const QString& text);

    bool isEscapedRegex(const QString& text, int pos);
    bool isEscapedPerlRegex(const QString& text, int pos);
    bool isEscapedRegexEndSign(const QString& text, int start, int pos, bool ignoreClasses = false) const;
    bool isInsideRegex(const QString& text, int index);
    bool isInsidePerlRegex(const QString& text, int index);
    void multiLineRegex(const QString& text, int index);
    void multiLinePerlRegex(const QString& text);
    int findDelimiter(const QString& text, int index, const QRegularExpression& delimExp, int& capturedLength) const;

    bool isXmlQuoted(const QString& text, int index);
    bool isXxmlComment(const QString& text, int index, int start);
    bool isXmlValue(const QString& text, int index, int start);
    void xmlValues(const QString& text);
    void xmlQuotes(const QString& text);
    void xmlComment(const QString& text);
    void highlightXmlBlock(const QString& text);

    bool isLuaQuote(const QString& text, int index) const;
    bool isSingleLineLuaComment(const QString& text, int index, int start) const;
    void multiLineLuaComment(const QString& text);
    void highlightLuaBlock(const QString& text);

    void markdownSingleLineCode(const QString& text);
    bool isIndentedCodeBlock(const QString& text, int& index, QRegularExpressionMatch& match) const;
    void markdownComment(const QString& text);
    bool markdownMultiLine(const QString& text,
                           const QString& oldStartPattern,
                           int indentation,
                           int state,
                           const QTextCharFormat& txtFormat);
    void markdownFonts(const QString& text);
    void highlightMarkdownBlock(const QString& text);

    bool isYamlKeyQuote(const QString& key, int pos);
    bool yamlOpenBraces(const QString& text,
                        const QRegularExpression& startExp,
                        const QRegularExpression& endExp,
                        int oldOpenNests,
                        bool oldProperty,
                        bool setData);
    void yamlLiteralBlock(const QString& text);
    void highlightYamlBlock(const QString& text);

    void reSTMainFormatting(int start, const QString& text);
    void highlightReSTBlock(const QString& text);

    void fountainFonts(const QString& text);
    bool isFountainLineBlank(const QTextBlock& block);
    void highlightFountainBlock(const QString& text);

    void latexFormula(const QString& text);

    bool isPascalQuoted(const QString& text, int index, int start = 0) const;
    bool isPascalMLCommented(const QString& text, int index, int start = 0) const;
    void singleLinePascalComment(const QString& text, int start = 0);
    void pascalQuote(const QString& text, int start = 0);
    void multiLinePascalComment(const QString& text);

    bool isEscapedJavaQuote(const QString& text, int pos, bool isStartQuote) const;
    bool isJavaSingleCommentQuoted(const QString& text, int index, int start) const;
    bool isJavaStartQuoteMLCommented(const QString& text, int index, int start = 0) const;
    void JavaQuote(const QString& text, int start = 0);
    void singleLineJavaComment(const QString& text, int start = 0);
    void multiLineJavaComment(const QString& text);
    void javaMainFormatting(const QString& text);
    void javaBraces(const QString& text);

    void highlightJsonBlock(const QString& text);
    void jsonKey(const QString& text, int start, int& K, int& V, int& B, bool& insideValue, QString& braces);
    void jsonValue(const QString& text, int start, int& K, int& V, int& B, bool& insideValue, QString& braces);

    bool isEscapedRubyRegex(const QString& text, int pos);
    int findRubyDelimiter(const QString& text,
                          int index,
                          const QRegularExpression& delimExp,
                          int& capturedLength) const;
    bool isInsideRubyRegex(const QString& text, int index);
    void multiLineRubyRegex(const QString& text);

    bool isEscapedTclQuote(const QString& text, int pos, int start, bool isStartQuote);
    bool isTclQuoted(const QString& text, int index, int start);
    bool insideTclBracedVariable(const QString& text, int pos, int start, bool quotesAreFormatted = false);
    void multiLineTclQuote(const QString& text);
    void highlightTclBlock(const QString& text);

    void multiLineRustQuote(const QString& text);
    bool isRustQuoted(const QString& text, int index, int start);

    bool isCmakeDoubleBracketed(const QString& text, int index, int start);
    bool cmakeDoubleBrackets(const QString& text, int oldBracketLength, bool wasComment);

    void tomlQuote(const QString& text);

    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<HighlightingRule> highlightingRules;

    QRegularExpression hereDocDelimiter;
    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;

    QRegularExpression htmlCommetStart, htmlCommetEnd;
    QRegularExpression htmlSubcommetStart, htmlSubcommetEnd;

    QTextCharFormat mainFormat;
    QTextCharFormat neutralFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat commentBoldFormat;
    QTextCharFormat noteFormat;
    QTextCharFormat quoteFormat;
    QTextCharFormat altQuoteFormat;
    QTextCharFormat urlInsideQuoteFormat;
    QTextCharFormat urlFormat;
    QTextCharFormat blockQuoteFormat;
    QTextCharFormat codeBlockFormat;
    QTextCharFormat whiteSpaceFormat;
    QTextCharFormat translucentFormat;
    QTextCharFormat regexFormat;
    QTextCharFormat errorFormat;
    QTextCharFormat rawLiteralFormat;

    QString progLan;

    QRegularExpression quoteMark, singleQuoteMark, backQuote, mixedQuoteMark, mixedQuoteBackquote;
    QRegularExpression xmlLt, xmlGt;
    QRegularExpression cppLiteralStart;

    QColor Blue, DarkBlue, Red, DarkRed, Verda, DarkGreen, DarkGreenAlt, Magenta, DarkMagenta, Violet, Brown,
        DarkYellow;

    QTextCursor startCursor, endCursor;

    int maxBlockSize_;
    bool hasQuotes_;
    bool multilineQuote_;
    bool mixedQuotes_;

    static const QRegularExpression urlPattern;
    static const QRegularExpression notePattern;

    enum {
        commentState = 1,
        nextLineCommentState,
        doubleQuoteState,
        singleQuoteState,
        SH_DoubleQuoteState,
        SH_SingleQuoteState,
        SH_MixedDoubleQuoteState,
        SH_MixedSingleQuoteState,
        pyDoubleQuoteState,
        pySingleQuoteState,
        xmlValueState,
        markdownBlockQuoteState,
        codeBlockState,
        JS_templateLiteralState,
        regexSearchState,
        regexState,
        regexExtraState,
        htmlBracketState,
        htmlStyleState,
        htmlStyleDoubleQuoteState,
        htmlStyleSingleQuoteState,
        htmlCSSState,
        htmlCSSCommentState,
        htmlJavaState,
        htmlJavaCommentState,
        cssBlockState,
        commentInCssBlockState,
        commentInCssValueState,
        cssValueState,
        updateState,
        endState

    };
};

}  // namespace Texxy

#endif

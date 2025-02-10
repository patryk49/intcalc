#include "utils.h"


#define NODE_LIST \
/* KEYWORDS FOR CONTROL FLOW */ \
	X(Print, 255,  0, 1, 1), \
	X(True,  255,  0, 1, 1), \
	X(False, 255,  0, 1, 1), \
\
/* POSTFIX OPERATORS */ \
	X(Call,           140,   0, 1, 1), \
	X(Subscript,      140,   0, 1, 1), \
	X(Factorial,      140,   0, 1, 1), \
\
/* PREFIX OPERATORS */ \
	X(Plus,           255, 120, 1, 1), \
	X(Minus,          255, 120, 1, 1), \
	X(LogicNot,       105, 120, 1, 1), \
\
/* CLOSING SYMBOLS */ \
	X(EndScope,   10, 0, 1, 1), \
\
/* OPENING SYMBOLS */ \
	X(OpenPar,  140,  0, 1, 1), \
	X(AbsValue, 140,  0, 1, 1), \
	X(Function, 255, 20, 1, 2), \
\
/* TERNARY OPERATION */ \
	X(Conditional,  48, 20, 1, 1), \
/* BINARY OPERATIONS */ \
	X(LogicOr,      60, 60, 1, 1), \
	X(LogicAnd,     62, 62, 1, 1), \
	X(Equal,        64, 64, 1, 1), \
	X(Less,         66, 66, 1, 1), \
	X(Greater,      66, 66, 1, 1), \
	X(Contains,     68, 68, 1, 1), \
\
	X(Concat,       54, 54, 1, 1), \
\
	X(Add,          72, 72, 1, 1), \
	X(Subtract,     72, 72, 1, 1), \
	X(Multiply,     78, 78, 1, 1), \
	X(Divide,       78, 78, 1, 1), \
	X(Power,        81, 80, 1, 1), \
\
	X(Pipe,         50, 50, 1, 1), \
\
/* OTHER EXPRESSINS */ \
	X(Variable,       11, 36, 2, 2), \
\
/* SEPARATORS */ \
	X(Semicolon,  10, 0,  1, 1), \
	X(Terminator, 10, 0,  1, 1), \
	X(Comma,      12, 0,  1, 1), \
	X(Colon,      21, 20, 1, 1), \
\
	X(StartScope, 10, 0, 1, 1), \
\
/* VALUES */ \
	X(Identifier, 255, 0, 2, 2), \
	X(Real,       255, 0, 2, 2), \
	X(Integer,    255, 0, 2, 2), \
	X(String,     255, 0, 2, 2), \
\
/* AFTER PARSING */ \
	X(Nop,         255, 255, 1, 1), \
	X(Jump,        255, 255, 0, 1), \
	X(Error,       255, 255, 0, 1), \
	X(Value,       255, 255, 0, 1)






// define enum
#define X(name, prec_left, prec_right, ts, ns) Ast_##name
enum AstType{ NODE_LIST };
#undef X


// define name strings
#define X(name, prec_left, prec_right, ts, ns) #name
static const char *const AstTypeNames[] = { NODE_LIST };
#undef X


// define left precedence table
#define X(name, prec_left, prec_right, ts, ns) prec_left
static const uint8_t PrecsLeft[] = { NODE_LIST };
#undef X


// define right precedence table
#define X(name, prec_left, prec_right, ts, ns) prec_right
static const uint8_t PrecsRight[] = { NODE_LIST };
#undef X


// define token size table
#define X(name, prec_left, prec_right, ts, ns) ts 
static const uint8_t TokenSizes[] = { NODE_LIST };
#undef X

// define ast node size table
#define X(name, prec_left, prec_right, ts, ns) ns 
static const uint8_t AstNodeSizes[] = { NODE_LIST };
#undef X



// keywords
#define AST_KW_START Ast_Print
#define AST_KW_END   Ast_False

static uint64_t KeywordNamesU64[AST_KW_END-AST_KW_START+1];

static void init_keyword_names(void){
	for (size_t i=AST_KW_START; i<=AST_KW_END; i+=1){
		uint64_t kw = AstTypeNames[i][0] + ('a'-'A');
		for (size_t j=1; AstTypeNames[i][j]!='\0'; j+=1){
			kw |= (uint64_t)AstTypeNames[i][j] << (j*8);
		}
		KeywordNamesU64[i-AST_KW_START] = kw;
	}
}

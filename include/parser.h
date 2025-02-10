#include "utils.h"
#include "unicode.h"
#include "files.h"

#include <stdio.h>
#include <stdlib.h>

#include "classes.h"


static bool is_valid_name_char(char);
static bool is_valid_first_name_char(char);
static bool is_number(char);
static bool is_whitespace(char);

static uint32_t parse_character(const char **);

static enum AstType parse_number(void *data_res, const char **src_it);

static void print_codeline(const char *text, size_t position);

static void raise_error(const char *text, const char *msg, uint32_t pos);



// AST ARRAY DATA STRUCTURE
typedef struct AstArray{
	AstNode *data;
	union{
		struct{
			AstNode *end;
			AstNode *maxptr;
		};
		struct{
			const char *error;
			uint32_t position;
		};
	};
} AstArray;


static AstArray ast_array_new(size_t capacity){
	assert(capacity >= 32);
	AstArray arr;
	arr.data = malloc(capacity*sizeof(AstNode));
	if (arr.data == NULL){
		assert(false && "node array allocation failrule");
	}
	arr.end = arr.data;
	arr.maxptr = arr.data + capacity;
	return arr;
}

static AstArray ast_array_clone(AstArray ast){
	if (ast.data == NULL) return ast;
	AstArray res = {0};
	size_t size = ast.end - ast.data;
	res.data = malloc(size*sizeof(AstNode));
	if (res.data == NULL) return res;
	memcpy(res.data, ast.data, size*sizeof(AstNode));
	res.end = res.data + size;
	res.maxptr = res.data + size;
	return res;
}

static void ast_array_grow(AstArray *arr){
	size_t new_capacity = 2*(arr->maxptr - arr->data);
	size_t arr_size = arr->end - arr->data;
	arr->data = realloc(arr->data, new_capacity*sizeof(AstNode));
	if (arr->data == NULL){
		assert(false && "node array allocation failrule");
	}
	arr->end = arr->data + arr_size;
	arr->maxptr = arr->data + new_capacity;
}

static AstNode *ast_array_push(AstArray *arr, AstNode node){
	if (arr->end == arr->maxptr) ast_array_grow(arr);
	AstNode *res = arr->end;
	arr->end += 1;
	*res = node;
	return res;	
}

static AstNode *ast_array_push2(AstArray *arr, AstNode node, Data data){
	if (arr->end+2 > arr->maxptr) ast_array_grow(arr);
	AstNode *res = arr->end;
	arr->end += 2;
	*res = node;
	(res+1)->data = data;
	return res;	
}



static AstArray make_tokens(const char *input){
	const char *text_begin = input;
	AstArray res = ast_array_new(1024);
	ast_array_push(&res, (AstNode){ .type = Ast_Semicolon });

#define RETURN_ERROR(arg_error, arg_position) { \
	res.error    = arg_error; \
	res.position = arg_position; \
	goto ReturnError; } \

	uint8_t scope_types[64];
	uint32_t scope_idxs[SIZE(scope_types)];
	size_t scope_count = 0;

#define PUSH_SCOPE(type) \
	if (scope_count == SIZE(scope_types)){ \
		RETURN_ERROR("too many nested scopes", position); \
	} else{ \
		scope_types[scope_count] = type; \
		scope_idxs[scope_count] = res.end - res.data; \
		scope_count += 1; \
	}
	size_t position = 0;

	AstNode *prev_token = res.data;

	AstNode curr;
	Data curr_data;

	for (;;){
		const char *prev_input = input;
		curr = (AstNode){ .pos = position };

		switch (*input){
		case '=': input += 1;
			if (*input == '='){
				input += 1;
				curr.type = Ast_Equal;
				goto AddToken;
			}
			if (*input == '>'){
				input += 1;
				if (prev_token->type != Ast_EndScope || scope_types[scope_count] != Ast_OpenPar)
					RETURN_ERROR("expected parameter list before => symbol", position);
				res.data[scope_idxs[scope_count]].type = Ast_Function;
				prev_token->pos = position;
				curr.type = Ast_Nop;
				goto AddToken; // add dummy node for storing more information later
			}
			if (prev_token->type != Ast_Identifier)
				RETURN_ERROR("expected variable's name before assignment", position);
			prev_token->type = Ast_Variable;
			goto SkipToken;

		case '+': input += 1;
			curr.type = Ast_Add;
			goto AddToken;

		case '-': input += 1;
			curr.type = Ast_Subtract;
			goto AddToken;

		case '*': input += 1;
			if (*input == '*'){
				input += 1;
				curr.type = Ast_Power;
			} else{
				curr.type = Ast_Multiply;
			}
			goto AddToken;

		case '/': input += 1;
			if (*input == '/'){
				input += 1;
				while (*input!='\0' && *input!='\n') input += 1;
				goto SkipToken;
			}
			if (*input == '*'){
				size_t depth = 1;
				for (;;){
					input += 1;
					UNLIKELY if (*input == '\0')
						RETURN_ERROR("unfinished comment", position);
					
					if (*input=='*' && *(input+1)=='/'){
						input += 1;
						depth -= 1;
						if (depth == 0){
							input += 1;
							goto SkipToken;
						}
					} else if (input[0]=='/' && input[1]=='*'){
						input += 1;
						depth += 1;
					}
				}
			}
			curr.type = Ast_Divide;
			goto AddToken;

		case '|': input += 1;
			if (*input == '|'){
				input += 1;
				curr.type = Ast_LogicOr;
			} else if (*input == '>'){
				curr.type = Ast_Pipe;
				input += 1;
			} else{
				if (scope_count != 0 && scope_types[scope_count-1] == Ast_AbsValue){
					scope_count -= 1;
					curr.type = Ast_EndScope;		
				} else{
					curr.type = Ast_AbsValue;
					PUSH_SCOPE(Ast_AbsValue);
				}
			}
			goto AddToken;

		case '&': input += 1;
			if (*input == '&'){
				input += 1;
				curr.type = Ast_LogicAnd;
			} else{
				RETURN_ERROR("invalid token \"&\"", position);
			}
			goto AddToken;

		case '<': input += 1;
			if (*input == '='){
				input += 1;
				curr.type  = Ast_Greater;
				curr.flags = AstFlag_Negate;
			} else if (*input == '>'){
				input += 1;
				curr.type = Ast_Concat;
			} else{
				curr.type = Ast_Less;
			}
			goto AddToken;

		case '>': input += 1;
			if (*input == '='){
				input += 1;
				curr.type = Ast_Less;
				curr.flags = AstFlag_Negate;
			} else{
				curr.type = Ast_Greater;
			}
			goto AddToken;

		case '!': input += 1;
			curr.flags = AstFlag_Negate;
			if (*input == '='){
				input += 1;
				curr.type  = Ast_Equal;
			} else if (*input == '|'){
				input += 1;
				curr.type = Ast_LogicOr;
			} else if (*input == '&'){
				input += 1;
				curr.type = Ast_LogicAnd;
			} else if (*input == '@' && *(input+1) == '='){
				input += 2;
				curr.type = Ast_Contains;
			} else{
				curr.type = Ast_LogicNot;
			}
			goto AddToken;

		case '?': input += 1;
			curr.type = Ast_Conditional;
			goto AddToken;

		case ':': input += 1;
			curr.type = Ast_Colon;
			goto AddToken;

		case '(': input += 1;
			PUSH_SCOPE(Ast_OpenPar);
			curr.type = Ast_OpenPar;
			goto AddToken;

		case ')':{ input += 1;
			if (scope_count == 0)
				RETURN_ERROR("too many closing parenthesis", position);
			scope_count -= 1;
			enum AstType t = scope_types[scope_count];
			if (t != Ast_OpenPar)
				RETURN_ERROR("mismatched parenthesis", position);
			curr.type = Ast_EndScope;
			goto AddToken;
		}
		
		case '[': input += 1;
			PUSH_SCOPE(Ast_Subscript);
			curr.type = Ast_Subscript;
			goto AddToken;
		
		case ']':{ input += 1;
			if (scope_count == 0)
				RETURN_ERROR("too many closing brackets", position);
			scope_count -= 1;
			enum AstType t = scope_types[scope_count];
			if (t != Ast_Subscript)
				RETURN_ERROR("mismatched brackets", position);
			curr.type = Ast_EndScope;
			goto AddToken;
		}

		case '@': input += 1;
			if (*input == '='){
				curr.type = Ast_Contains;
				input += 1;
			} else{
				RETURN_ERROR("invalid token \"@\"", position);
			}
			goto AddToken;

		case ',': input += 1;
			curr.type = Ast_Comma;
			goto AddToken;

		case '.': input += 1;
			if (is_number(*input)){
				input -= 1;
				curr.type = parse_number(&curr_data, &input);
				if (curr.type == Ast_Error)
					RETURN_ERROR("invalid floating point literal", position);
				goto AddTokenWithData;
			} else{
				RETURN_ERROR("invalid token \".\"", position);
			}
			goto AddToken;

		case ';':
			input += 1;
			if (prev_token->type == Ast_Semicolon) goto SkipToken;
			curr.type = Ast_Semicolon;
			goto AddToken;
		
		case '\"':{
			input += 1;
			size_t data_size = 0;
			//BcNode *dest_node = global_bc + global_bc_size;
			//uint8_t *dest_data = (uint8_t *)(dest_node + 2);
			while (*input != '\"'){
				if (*input == '\0')
					RETURN_ERROR("end of file inside of string literal", input-text_begin);
				uint32_t c = parse_character(&input);
				if (c == UINT32_MAX)
					RETURN_ERROR("invalid character code", input-text_begin);
				//size_t code_size = utf8_write(dest_data, c);
				//dest_data += code_size;
				//data_size += code_size;
			}
			//*dest_data = '\0';
			input += 1;
			//*dest_node = (BcNode){ .type = BC_Data, };
			//(dest_node+1)->clas = (Class){ .tag = Class_Bytes, .bytesize = data_size };

			curr.type = Ast_String;
			//curr_data.bufinfo.index = global_bc_size + 2;
			curr_data.bufinfo.size = data_size;
			
			//global_bc_size += 2 + (data_size + 2 + sizeof(BcNode) - 1)/sizeof(BcNode);
			assert(false && "strings are unimplemented");
			goto AddTokenWithData;
		}

		case ' ':
		case '\t':
			while (input+=1, *input==' ' || *input=='\t');
			goto SkipToken;

		case '\n':{
			input += 1;
			enum AstType prev_type = prev_token->type;
			if (
				scope_count != 0 || (Ast_OpenPar <= prev_type && prev_type <= Ast_Semicolon)
			) goto SkipToken;
			curr.type = Ast_Semicolon;
			goto AddToken;
		}
		
		case '\0':
			curr.type = Ast_Terminator;
			curr.pos = position;
			ast_array_push(&res, curr);
			return res;

		default:{
			if (is_valid_first_name_char(*input)){
				size_t size = 1;
				while (is_valid_name_char(input[size])) size += 1;

				if (2 <= size && size <= 8){
					uint64_t text = 0;
					memcpy(&text, input, size);
					for (size_t i=AST_KW_START; i<=AST_KW_END; i+=1){
						if (text == KeywordNamesU64[i-AST_KW_START]){
							curr.type = (enum AstType)i;
							input += size;
							goto AddToken;
						}
					}
				}
				curr.type = Ast_Identifier;
				curr.count = size;
				curr_data.name_id = get_name_id(input, size);
				input += size;
				goto AddTokenWithData;
			}
			if (is_number(*input)){
				curr.type = parse_number(&curr_data, &input);
				if (curr.type == Ast_Error)
					RETURN_ERROR("invalid number literal", position);
				goto AddTokenWithData;
			}
			RETURN_ERROR("unrecognized token", position);
		}

	AddTokenWithData:
		prev_token = ast_array_push2(&res, curr, curr_data);
		goto SkipToken;
	AddToken:
		prev_token = ast_array_push(&res, curr);
	SkipToken:
		position += input - prev_input;
	}}
#undef PUSH_SCOPE 
#undef RETURN_ERROR
ReturnError:
	free(res.data);
	res.data = NULL;
	return res;
}





static AstArray parse_tokens(AstArray tokens){
	AstNode opers[512];
	size_t opers_size = 1;
	
	opers[0] = (AstNode){ .type = Ast_Terminator };

	AstNode *it = tokens.data + 1;
	AstNode *res_it = it;

#define RETURN_ERROR(arg_error, arg_position) { \
	tokens = (AstArray){ .data=NULL, .error=arg_error, .position=arg_position }; \
	goto ReturnError; \
}

#define CHECK_OPER_STACK_OVERFLOW(arg_position) if (opers_size == SIZE(opers)){ \
	tokens.error    = "operator stack overflow"; \
	tokens.position = arg_position; \
	goto ReturnError; \
}

ExpectValue:{
		AstNode curr = *it;
		it += 1;
		
		switch (curr.type){
		case Ast_Terminator:
			goto EndOfFile;

		case Ast_EndScope:
			it -= 1;
			goto ExpectOperator;
		
		case Ast_True:
		case Ast_False:
			*res_it = curr; res_it += 1;
			goto ExpectOperator;

		case Ast_Integer:
		case Ast_Real:
		case Ast_String:
		case Ast_Identifier:
		SimpleLiteral:
			*res_it = curr;
			(res_it+1)->data = *(Data *)it;
			it += 1;
			res_it += 2;
			goto ExpectOperator;
		
		case Ast_LogicNot:
		SimplePrefixOperator:
			CHECK_OPER_STACK_OVERFLOW(curr.pos);
			opers[opers_size] = curr; opers_size += 1;
			goto ExpectValue;

		case Ast_Add:
			goto ExpectValue;

		case Ast_Subtract:
			curr.type = Ast_Minus;
			goto SimplePrefixOperator;

		case Ast_Variable:{
			enum AstType t = opers[opers_size-1].type;
			CHECK_OPER_STACK_OVERFLOW(curr.pos);
			memcpy(opers+opers_size, it, sizeof(Data));
			opers_size += 1;
			it += 1;
			goto SimplePrefixOperator;
		}

		case Ast_OpenPar:{
			if (it->type == Ast_EndScope)
				RETURN_ERROR("missing expression inside of parenthesis", curr.pos);
			curr.count = 0;
			goto SimplePrefixOperator;
		}

		case Ast_AbsValue:{
			if (it->type == Ast_EndScope)
				RETURN_ERROR("missing expression inside of absolute value symbol", curr.pos);
			curr.count = 0;
			goto SimplePrefixOperator;
		}

		case Ast_Function:{
			AstNode *head = res_it;
			res_it += 2;
			if (it->type != Ast_EndScope) for (;;){
				if (it->type != Ast_Identifier)
					RETURN_ERROR("expected parameter's name", it->pos);
				res_it->data = (it+1)->data;
				res_it += 1;
				it += 2;
				curr.count += 1;
				if (it->type == Ast_EndScope) break;
				if (it->type != Ast_Comma)
					RETURN_ERROR("expected closing parenthesis or comma", it->pos);
				it += 1;
			}
			if (curr.count > 128)
				RETURN_ERROR("function has too many parameters, max is 128", curr.pos);
			*head = curr;
			(head+1)->pos = it->pos;
			it += 2; // also skip nop
			CHECK_OPER_STACK_OVERFLOW(curr.pos);
			opers[opers_size] = (AstNode){.type = Ast_Function, .pos = head-tokens.data};
			opers_size += 1;
			goto ExpectValue;
		}

		default:
			RETURN_ERROR("expected value", curr.pos);
		}
	}


	ExpectOperator:{
		AstNode curr = *it;
		it += 1;

		if (PrecsLeft[curr.type] == UINT8_MAX)
			RETURN_ERROR("expected operator", curr.pos);

		for (;;){
			AstNode head = opers[opers_size-1];
			if (PrecsRight[head.type] < PrecsLeft[curr.type]) break;
			opers_size -= 1;
			if (head.type == Ast_Function){
				AstNode *startnode = tokens.data + head.pos;
				(startnode+1)->data.funcnodeinfo.node_size = (res_it - tokens.data) - head.pos;
				head = (AstNode){ .type = Ast_EndScope, .pos=startnode->pos };
			}
			if (head.type == Ast_Colon){
				AstNode *startnode = tokens.data + head.pos;
				*startnode = (AstNode){
					.type = Ast_Jump,
					.pos = (res_it - tokens.data) - head.pos - 1
				};
				continue;
			}
			*res_it = head;
			res_it += 1;
			if (head.type == Ast_Variable){
				opers_size -= 1;
				*res_it = opers[opers_size];
				res_it += 1;
			}
		}

		if (Ast_LogicOr <= curr.type && curr.type <= Ast_Pipe){
			CHECK_OPER_STACK_OVERFLOW(curr.pos);
			opers[opers_size] = curr; opers_size += 1;
			goto ExpectValue;
		}

		switch (curr.type){
		case Ast_Terminator:
			goto EndOfFile;

		case Ast_Semicolon:
			*res_it = curr; res_it += 1;
			goto ExpectValue;

		case Ast_Comma:{
			enum AstType t = opers[opers_size-1].type;
			if (t != Ast_Call && t != Ast_Subscript)
				RETURN_ERROR("invalid usage of comma", curr.pos);
			opers[opers_size-1].count += 1;
			goto ExpectValue;
		}

		case Ast_LogicNot:
			curr.type = Ast_Factorial;
		SimplePostfixOperator:
			*res_it = curr; res_it += 1;
			goto ExpectOperator;

		case Ast_Conditional:{
			*res_it = curr;
			CHECK_OPER_STACK_OVERFLOW(curr.pos);
			curr.pos = res_it - tokens.data;
			res_it += 1;
			opers[opers_size] = curr; opers_size += 1;
			goto ExpectValue;
		}

		case Ast_Colon:{
			AstNode top = opers[opers_size-1];
			if (top.type != Ast_Conditional)
				RETURN_ERROR("colon must be a part of ternary expression", curr.pos);
			size_t jump_size = (res_it - tokens.data) - top.pos;
			if (jump_size >= (1 << 16))
				RETURN_ERROR("parser_error: condition on true was too big", curr.pos);
			tokens.data[top.pos].count = jump_size;
			curr.pos = res_it - tokens.data;
			res_it += 1; // leave some place for a jump node
			opers[opers_size-1] = curr;
			goto ExpectValue;
		}

		case Ast_OpenPar:
			curr.type = Ast_Call;
			if (it->type == Ast_EndScope){
				it += 1;
				goto SimplePostfixOperator;
			}
		SimplePostfixScope:
			curr.count = 1;
			CHECK_OPER_STACK_OVERFLOW(it->pos);
			opers[opers_size] = curr; opers_size += 1;
			goto ExpectValue;

		case Ast_Subscript:
			if (it->type == Ast_EndScope)
				RETURN_ERROR("empty subscript operator", curr.pos)
			goto SimplePostfixScope;

		case Ast_EndScope:
			goto HandleEndOfScope;

		default:
			RETURN_ERROR("parser error: unhandled token", curr.pos);
		}
	}


	HandleEndOfScope:{
		opers_size -= 1;
		AstNode sc = opers[opers_size];

		switch (sc.type){
		case Ast_OpenPar:
			if (sc.count != 0)
				RETURN_ERROR("parenthesis contain too many expressions", sc.pos);
			goto ExpectOperator;
		
		case Ast_AbsValue:
			if (sc.count != 0)
				RETURN_ERROR("absolute value contains too many expressions", sc.pos);
			*res_it = sc; res_it += 1;
			goto ExpectOperator;
		
		case Ast_Call:
			*res_it = sc; res_it += 1;
			goto ExpectOperator;

		case Ast_Subscript:
			*res_it = sc; res_it += 1;
			goto ExpectOperator;

		default: 
			RETURN_ERROR("parser error: unhandled scope type", sc.pos);
		}
	}
	
EndOfFile:
	if (opers[opers_size-1].type != Ast_Terminator)
		RETURN_ERROR("unexpected end of file", (it-1)->pos-1);
	*res_it = (AstNode){ .type = Ast_Terminator };
	tokens.end = res_it;
ReturnError:
	return tokens;
#undef RETURN_ERROR
}





static bool is_valid_name_char(char c){
	return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_';
}

static bool is_valid_first_name_char(char c){
	return (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='_';
}

static bool is_number(char c){
	return (uint8_t)(c-'0') < 10;
}

static bool is_whitespace(char c){
	return c==' ' || c=='\n' || c=='\t' || c=='\v';
}



static uint32_t parse_character(const char **iter){
	const char *it = *iter;
	uint32_t c;

ParseCharacter:{
		c = *it;
		if (c != '\\') return utf8_parse((const uint8_t **)iter);
	
		int utf_base = 10;
	
		it += 1;
		c = *it;
		switch (c){
		case '\'':
		case '\"':
		case '\\':
			break;
		case '@': case '#': case '$': case '%': case '^': case '&': case '*':
		case '(': case ')': case '[': case ']': case '{': case '}':
			c = '\\';
			it -= 1;
			break;
		case 'x':
			utf_base = 16;
			goto ParseUtf8;
		case 'o':
			utf_base = 8;
			goto ParseUtf8;
		ParseUtf8:
			it += 1;
			FALLTHROUGH;
		case '0' ... '9':{
			const char *old_iter = it;
			c = strtol(old_iter, (char **)&it, utf_base);
			if (old_iter == it) return UINT32_MAX;
			it -= 1;
			break;
		}
		case 'a': c = '\a';   break;
		case 'b': c = '\b';   break;
		case 'e': c = '\033'; break;
		case 'f': c = '\f';   break;
		case 'r': c = '\r';   break;
		case 'n': c = '\n';   break;
		case 't': c = '\t';   break;
		case 'v': c = '\v';   break;
		case '\n':
		case '\v':
		case '_':
			it += 1;
			goto ParseCharacter;
		default:
			return UINT32_MAX;
		}
	}

	*iter = it + 1;
	return c;
}



static uint64_t parse_number_dec(const char **src_it){
	const char *src = *src_it;
	int64_t res = 0;

	while (is_number(*src)){
		res = res*10 + (*src - '0');
		while (src+=1, *src == '_');
	}

	*src_it = src;
	return res;
}

static int64_t parse_number_bin(const char **src_it){
	const char *src = *src_it;
	int64_t res = 0;

	while (*src=='0' || *src=='1'){
		res = (res << 1) | (*src - '0');
		while (src+=1, *src == '_');
	}

	*src_it = src;
	return res;
}

static int64_t parse_number_oct(const char **src_it){
	const char *src = *src_it;
	int64_t res = 0;

	while (is_number(*src) && *src<'8'){
		res = (res << 3) | (*src - '0');
		while (src+=1, *src == '_');
	}

	*src_it = src;
	return res;
}

static int64_t parse_number_hex(const char **src_it){
	const char *src = *src_it;
	int64_t res = 0;

	for (;;){
		char c = *src;
		if (is_number(c)){
			res = (res << 4) | (c - '0');
		} else{
			if (c <= 'Z') c += 'a'-'A';
			if ('a' > c || c > 'f') break;
			res = (res << 4) | (10 + c - 'a');
		}
		while (src+=1, *src == '_');
	}

	*src_it = src;
	return res;
}


static enum AstType parse_number(void *res_data, const char **src_it){
	const char *src = *src_it;

	int64_t res_integer = 0;
	enum AstType res_type = Ast_Integer;

	if (*src == '0'){
		src += 1;
		switch (*src){
		case 'b': src += 1; res_integer = parse_number_bin(&src); goto Return;
		case 'o': src += 1; res_integer = parse_number_oct(&src); goto Return;
		case 'x': src += 1; res_integer = parse_number_hex(&src); goto Return;
		default: break;
		}
	}
	
	res_integer = parse_number_dec(&src);
	
	if (*src == '.'){
		src += 1;
		double res_real = (double)res_integer; // decimal part
		const char *fraction_str = src;
		uint64_t fraction_num = parse_number_dec(&src);
		if (fraction_num != 0){
			size_t fraction_width = 0;
			for (; fraction_str!=src; fraction_str+=1){
				fraction_width += *fraction_str != '_';
			}
			double fraction_den = util_ipow_f64(10.0, fraction_width);
			res_real += (double)fraction_num / fraction_den;
		}
		res_type = Ast_Real;
		memcpy(&res_integer, &res_real, sizeof(double));
	}
	
Return:
	*src_it = src;
	memcpy(res_data, &res_integer, sizeof(uint64_t));
	return res_type;
}




// DEBUG INFORMATION HELPERS
static void print_codeline(const char *text, size_t position){
	size_t row = 0;
	size_t col = 0;
	size_t row_position_prev = 0;
	size_t row_position = 0;

	for (size_t i=0; i!=position; ++i){
		col += 1;
		if (text[i]=='\n' || text[i]=='\v'){
			row_position_prev = row_position;
			row_position += col;
			row += 1;
			col = 0;
		}
	}
	fprintf(stderr, " -> row: %lu, column: %lu\n>\n", row, col);

	if (row != 0){
		putchar('>');
		putchar(' ');
		putchar(' ');
		for (size_t i=row_position_prev;; ++i){
			char c = text[i];
			if (c=='\0' || c=='\n' || c=='\v') break;
			putchar(c);
		}
		putchar('\n');
	}

	putchar('>');
	putchar(' ');
	putchar(' ');
	for (size_t i=row_position;; ++i){
		char c = text[i];
		if (c=='\0' || c=='\n' || c=='\v') break;
		putchar(c);
	}
	putchar('\n');

	putchar('>');
	putchar(' ');
	putchar(' ');
	for (size_t i=0; i!=col; ++i){
		putchar(text[row_position+i]=='\t' ? '\t' : ' ');
	}
	putchar('^');
	putchar('\n');
	putchar('\n');
}


static void raise_error(const char *text, const char *msg, uint32_t pos){
	fprintf(stderr, "error: \"%s\"", msg);
	print_codeline(text, pos);
	exit(1);
}



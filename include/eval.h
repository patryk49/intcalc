#include <math.h>

#include "parser.h"



// idea on how to handle initializer imlicit conversion
// initializer info (variable_names, values) are stored in separate buffer
// initializer value hold index to initializer info storage


typedef struct{
	const char *msg;
	uint32_t pos;
} EvalError;



EvalError eval_ast(AstArray nodes){
	Value stack[512];
	size_t stack_size = 0;
	Value vars[256];
	size_t var_count = 0;
	
	EvalError res_error = {0};
#define RETURN_ERROR(msg, pos) \
	do{ res_error = (EvalError){(msg), (pos)}; goto ReturnError; } while (0)
#define PUSH_VALUE(p_data, p_type) do{ \
		if (stack_size == SIZE(stack)) RETURN_ERROR("evaluation stack overflow", node.pos); \
		stack[stack_size] = (Value){ .type = (p_type), .data = (p_data) }; \
		stack_size += 1; \
	} while (0)
	
	AstNode *ast = nodes.data + 1;
	for (;;){
		AstNode node = *ast;
		ast += 1;
		switch (node.type){
		case Ast_Terminator: return res_error;
		case Ast_Variable:{
			if (var_count == SIZE(vars))
				RETURN_ERROR("eval_error: too many variables were defined", node.pos);
			Value top = stack[stack_size-1];
			top.name_id = ast->data.name_id;
			if (top.type == DT_Function && top.data.funcinfo.name_id == 0){
				top.data.funcinfo.name_id = ast->data.name_id;
			}
			vars[var_count] = top; var_count += 1;
			ast += 1;
			stack[stack_size-1].type = DT_Null;
			break;
		}
		case Ast_Identifier:{
			size_t i = var_count;
			while (i != 0){
				i -= 1;
				if (vars[i].name_id == ast->data.name_id) goto IdentifierWasFound;
			}
			RETURN_ERROR("indentifier not found", node.pos);
		IdentifierWasFound:
			ast += 1;
			PUSH_VALUE(vars[i].data, vars[i].type);
			break;
		}
		case Ast_Function:
			PUSH_VALUE((Data){ .funcinfo.index = (ast-1 - nodes.data) }, DT_Function);
			ast += ast->data.funcnodeinfo.node_size;
			break;
		case Ast_Real:
			PUSH_VALUE(ast->data, DT_Real);	
			ast += 1;
			break;
		case Ast_Integer:
			PUSH_VALUE(ast->data, DT_Integer);	
			ast += 1;
			break;
		case Ast_True:
		case Ast_False:
			PUSH_VALUE((Data){ .boolean = (node.type == Ast_True) }, DT_Bool);
			break;
		case Ast_Conditional:{
			stack_size -= 1;
			Value arg = stack[stack_size];
			if (arg.type != DT_Bool)
				RETURN_ERROR("condition doesn't have boolean type", node.pos);
			if (!arg.data.boolean){ ast += node.count; }
			break;
		}
		case Ast_Jump:{
			ast += node.pos;
			break;
		}
		case Ast_Minus:{
			Value arg = stack[stack_size-1];
			Value res = arg;
			switch (arg.type){
			case DT_Real:    res.data.real    = -arg.data.real;    break;
			case DT_Integer: res.data.integer = -arg.data.integer; break;
			default: RETURN_ERROR("invalid argument's type for unary minus", node.pos);
			}
			stack[stack_size-1] = res;
			break;
		}
		case Ast_LogicNot:{
			Value arg = stack[stack_size-1];
			Value res = arg;
			switch (arg.type){
			case DT_Bool: res.data.boolean = !arg.data.boolean; break;
			default: RETURN_ERROR("invalid argument's type for logic not minus", node.pos);
			}
			stack[stack_size-1] = res;
			break;
		}
		case Ast_Factorial:{
			Value arg = stack[stack_size-1];
			Value res = arg;
			switch (arg.type){
			case DT_Real:
				if (arg.data.real <= -1.0)
					RETURN_ERROR("cannot take factorial of value less or equal to -1", node.pos);
				res.data.real = tgamma(arg.data.real + 1.0);
				break;
			case DT_Integer:
				if (arg.data.integer < 0)
					RETURN_ERROR("cannot take factorial of negative value", node.pos);
				res.data.integer = 1;
				for (size_t i=2; i<=arg.data.integer; i+=1){ res.data.integer *= i; }
				break;
			default: RETURN_ERROR("invalid argument's type for logic not minus", node.pos);
			}
			stack[stack_size-1] = res;
			break;
		}
		case Ast_Call:
		CallFunction:{
			Value *params = stack + stack_size - node.count;
			Value func_value = params[-1];
			AstNode *func = nodes.data + func_value.data.funcinfo.index;
			if (node.count != func->count)
				RETURN_ERROR("wrong number of arguments", node.pos);
			Value callinfo = {.type=DT_CallInfo, .data.callinfo={ast-nodes.data, var_count}};
			Data *param_names = (Data *)(func + 2);
			for (size_t i=0; i!=node.count; i+=1){
				if (var_count == SIZE(vars))
					RETURN_ERROR("evaluation stack overflow", node.pos);
				vars[var_count] = params[i];
				vars[var_count].name_id = param_names[i].name_id;
				var_count += 1;
			}
			stack_size -= node.count;
			stack[stack_size-1] = callinfo;
			ast = func + 2 + func->count;
			break;
		}
		case Ast_EndScope:{
			assert(stack[stack_size-2].type == DT_CallInfo);
			CallInfo callinfo = stack[stack_size-2].data.callinfo;
			stack[stack_size-2] = stack[stack_size-1]; stack_size -= 1;
			var_count = callinfo.vars_size;
			ast = nodes.data + callinfo.ast_index;
			break;
		}
		case Ast_Pipe:{
			Value func = stack[stack_size-1];
			stack[stack_size-1] = stack[stack_size-2];
			stack[stack_size-2] = func;
			node.count = 1;
			goto CallFunction;
		}
		case Ast_Add:
		case Ast_Subtract:
		case Ast_Multiply:
		case Ast_Divide:
		case Ast_LogicOr:
		case Ast_LogicAnd:
		case Ast_Less:
		case Ast_Greater:
		case Ast_Equal:{
			Value lhs = stack[stack_size-2];
			Value rhs = stack[stack_size-1];
			Value res = { .type = lhs.type };
			stack_size -= 1;
			if (lhs.type != rhs.type)
				RETURN_ERROR("opperator's arguments have different types", node.pos);
			switch (lhs.type){
			case DT_Real:
				switch (node.type){
				case Ast_Add:      res.data.real = lhs.data.real + rhs.data.real; break;
				case Ast_Subtract: res.data.real = lhs.data.real - rhs.data.real; break;
				case Ast_Multiply: res.data.real = lhs.data.real * rhs.data.real; break;
				case Ast_Divide:   
					if (rhs.data.real == 0.0) RETURN_ERROR("division by zero", node.pos);
					res.data.real = lhs.data.real / rhs.data.real;
					break;
				case Ast_Equal:
					res.data.boolean = lhs.data.real == rhs.data.real; res.type = DT_Bool;
					break;
				case Ast_Less:
					res.data.boolean = lhs.data.real < rhs.data.real; res.type = DT_Bool;
					break;
				case Ast_Greater:
					res.data.boolean = lhs.data.real > rhs.data.real; res.type = DT_Bool;
					break;
				default: goto BinOpTypeError;
				}
				break;
			case DT_Integer:
				switch (node.type){
				case Ast_Add:      res.data.integer = lhs.data.integer+rhs.data.integer; break;
				case Ast_Subtract: res.data.integer = lhs.data.integer-rhs.data.integer; break;
				case Ast_Multiply: res.data.integer = lhs.data.integer*rhs.data.integer; break;
				case Ast_Divide:   
					if (rhs.data.integer == 0) RETURN_ERROR("division by zero", node.pos);
					res.data.integer = lhs.data.integer / rhs.data.integer;
					break;
				case Ast_Equal:
					res.data.boolean = lhs.data.integer == rhs.data.integer; res.type = DT_Bool;
					break;
				case Ast_Less:
					res.data.boolean = lhs.data.integer < rhs.data.integer; res.type = DT_Bool;
					break;
				case Ast_Greater:
					res.data.boolean = lhs.data.integer > rhs.data.integer; res.type = DT_Bool;
					break;
				default: goto BinOpTypeError;
				}
				break;
			case DT_Bool:
				switch (node.type){
				case Ast_LogicOr:  res.data.boolean = lhs.data.boolean||rhs.data.boolean; break;
				case Ast_LogicAnd: res.data.boolean = lhs.data.boolean&&rhs.data.boolean; break;
				case Ast_Equal:    res.data.boolean = lhs.data.boolean==rhs.data.boolean; break;
				default: goto BinOpTypeError;
				}
				break;
			default:
			BinOpTypeError:
				RETURN_ERROR("argument's type is not supported by this operator", node.pos);
			}
			if (node.flags & AstFlag_Negate){
				assert(res.type == DT_Bool);
				res.data.boolean = !res.data.boolean;
			}
			stack[stack_size-1] = res;
			break;
		}
		case Ast_Semicolon:
			stack_size -= 1;
			Value top = stack[stack_size];
			switch (top.type){
			case DT_Null: break;
			case DT_Real:
				printf("%lf\n", top.data.real);
				break;
			case DT_Integer:
				printf("%li\n", top.data.integer);
				break;
			case DT_Bool:
				printf("%s\n", top.data.boolean ? "true" : "false");
				break;
			case DT_Function:
				if (top.data.funcinfo.name_id != 0){
					printf("function \"");
					const uint8_t *name = global_names.data + top.data.funcinfo.name_id;	
					size_t name_len = *(name-1);
					for (size_t i=0; i!=name_len; i+=1){ putchar(name[i]); }
					printf("\"\n");
				} else{
					printf("function at %u\n", (nodes.data + top.data.funcinfo.index)->pos);
				}
				break;
			default:
				RETURN_ERROR("expression has invalid data type", node.pos);
			}
			break;
		default:
			RETURN_ERROR("eval_error: unhandled ast node", node.pos);
		}
	}

#undef RETURN_ERROR
ReturnError:
	return res_error;
}

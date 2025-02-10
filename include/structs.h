#include "utils.h"
#include "ast_nodes.h"



typedef uint32_t NameId;
typedef int32_t  VarIndex;



enum DataType{
	DT_Null =  0,
	DT_CallInfo,
	DT_Error,
	DT_Real,
	DT_Integer,
	DT_Bool,
	DT_String,
	DT_Array,
	DT_Function,
};

typedef struct{
	uint32_t index;
	uint32_t size;
} StaticBufInfo;

typedef struct{
	uint32_t node_size;
	uint32_t flags;
} FunctionNodeInfo;

typedef struct{
	uint32_t index;
	uint32_t name_id;
} FunctionInfo;

typedef struct{
	uint32_t ast_index;
	uint32_t vars_size;
} CallInfo;

typedef union Data{
	uint8_t  bytes[8];
	double   real;
	int64_t  integer;
	bool     boolean;
	void    *ptr;
	const char *text;
	NameId name_id;
	StaticBufInfo bufinfo;
	FunctionNodeInfo funcnodeinfo;	
	FunctionInfo     funcinfo;	
	CallInfo         callinfo;
} Data;



// PARSE TREE
enum AstFlags{
// general flags

// operator flags
	AstFlag_Negate = 1 << 3,
};

typedef union AstNode{
	struct{
		enum AstType type : 8;
		uint8_t  flags;
		uint16_t count;
		uint32_t pos;
	};
	Data data;
} AstNode;



typedef struct{
	enum DataType type : 8;
	NameId name_id; // only for variables
	Data data;
} Value;



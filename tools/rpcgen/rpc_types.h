#pragma once

enum TokenType {
    NullToken = 0,
    TypenameToken,
    ModifierToken,
    IdentifierToken,
    NumberToken,
    LBraceToken,
    RBraceToken,
    LBracketToken,
    RBracketToken,
    SemicolonToken,
    ColonToken,
};

enum TypeName {
    NullType = 0,
    VoidType,
    IntType,
    StringType,
    LongType,
    BlobType,
};

enum Modifier {
    NullModifier = 0,
    OneWayModifier,
    OutModifier,
};

struct token {
    enum TokenType type;

    char text[64];
    
    enum TypeName type_name;
    enum Modifier modifier;

    struct token* next;
};

struct RPCType {
    enum TypeName type;
    enum Modifier modifier;
    struct RPCType* next;
};

struct RPCArgument {
    char name[64];
    struct RPCType* type;
    struct RPCArgument* next;
};

struct RPCFunction {
    char name[64];
    struct RPCType* return_type;
    struct RPCArgument* args;
    struct RPCFunction* next;
};

enum RPCStatementType {
    RPCNullStatement = -1,
    RPCFunctionStatement,
};

struct RPCStatement {
    enum RPCStatementType type;
    struct RPCFunction* fn;
    struct RPCStatement* next;
};




# SOLID Syntax Parser Document

Version: 0.0.5

## Notation

The syntax is specified using a variant of Extended Backus-Naur Form (EBNF), similar to that used in the golang specification.

```
Syntax      = { Production } .
Production  = production_name "=" [ Expression ] "." .
Expression  = Term { "|" Term } .
Term        = Factor { Factor } .
Factor      = production_name | token [ "…" token ] | Group | Option | Repetition .
Group       = "(" Expression ")" .
Option      = "[" Expression "]" .
Repetition  = "{" Expression "}" .
```

Productions are expressions constructed from terms and the following operators, in increasing precedence:

```
|   alternation
()  grouping
[]  option (0 or 1 times)
{}  repetition (0 to n times)
```

Lowercase production names are used to identify lexical (terminal) tokens. Non-terminals are in CamelCase.

Lexical tokens are enclosed in double quotes "" and may contain escape characters.

Escape characters include:

- `\"` (U+0022) Double quotes;
- `\\` (U+005C) Backslash;
- `\f` (U+000C) Form Feed;
- `\n` (U+000A) Line Feed;
- `\r` (U+000D) Carriage Return;
- `\t` (U+0009) Horizontal tab;
- `\v` (U+000B) Vertical Tabulation.

The form `a … b` represents the set of characters from `a` through `b` as alternatives. The character `…` used here is a single horizontal ellipsis character, not the three characters `...`.

## Source Code

Source code is Unicode text encoded in UTF-8. For simplicity, this document will use the term *character* to refer to a Unicode code point in source code text.

A UTF-8 BOM (U+FEFF) will be ignored if it is the first Unicode code point in the source text.

## Lexical Elements

The following terms are used to denote specific Unicode character categories:

```
letter        = "A" … "Z" | "a" … "z" | "_" .

decimal_digit = "0" … "9" .
binary_digit  = "0" | "1" .
octal_digit   = "0" … "7" .
hex_digit     = "0" … "9" | "A" … "F" | "a" … "f" .

white_space   = " " | "\t" | "\v" | "\f" | "\n" | "\r" .

end_of_line   = "\n" | "\r" | "\r\n" .
```

### Comments

Comments serve as program documentation, start with the character sequence `"//"` and stop at the `end_of_line`. A comment cannot start inside a rune or string literal, or inside a comment.

### Keywords

```
namespace   using       func        contract    fulfills    struct      
enum        union       variant     let         if          else        
loop        while       break       continue    return                  
```

### Operators and Punctuation

```
+       &       &&      ==      (       )       
-       |       ||      !=      [       ]       
*       ~       !       <       {       }       
/       ^       ^^      >       ,       ;       
%       <<              <=      .       :       
=       >>              >=              ::      
```

### Identifiers

```
identifier = letter { letter | decimal_digit } .
```

### Integer Literals

Syntax:

```
int_lit        = decimal_lit | binary_lit | octal_lit | hex_lit .
decimal_lit    = "0" | ( "1" … "9" ) [ [ "_" ] decimal_digit { [ "_" ] decimal_digit } ] [ [ "_" ] int_lit_suffix ] .
binary_lit     = "0" ( "b" | "B" ) [ "_" ] binary_digit { [ "_" ] binary_digit } [ [ "_" ] int_lit_suffix ] .
octal_lit      = "0" ( "o" | "O" ) [ "_" ] octal_digit { [ "_" ] octal_digit } [ [ "_" ] int_lit_suffix ] .
hex_lit        = "0" ( "x" | "X" ) [ "_" ] hex_digit { [ "_" ] hex_digit } [ [ "_" ] int_lit_suffix ] .

int_lit_suffix = "i8" | "i16" | "i32" | "i64" | "isize" | "i" | "u8" | "u16" | "u32" | "u64" | "usize" | "u" .
```

Example:

```
```

### Float Literals

Syntax:

```
float_lit         = TODO .

float_lit_suffix  = "f32" | "f64" | "f" | "d" .
```

Example:

```
```

## Types

Syntax:

```
Type = NamedType | RefType | ArrayType | FuncType .
```

### Named Type

Syntax:

```
NamedType =  .
```

Example:

```
TODO
```

### Ref Type

Syntax:

```
RefType =  .
```

Example:

```
TODO
```

### Array Type

Syntax:

```
ArrayType =  .
```

Example:

```
TODO
```

### Func Type

Syntax:

```
FuncType =  .
```

Example:

```
TODO
```

## Program

Syntax:

```
Program = { Decl } .
```

### Compile-Time Annotations

Syntax:

```
CtAnnotation  = "@" identifier [ "(" Expr [ "," Expr ] ")" ] .
CtAnnotations = CtAnnotation { CtAnnotation } .
```

Example:

```
@private
@align(16)
@when(OS_LINUX)
@import("LLVM-C","LLVMContextCreate")
```

## Declarations

Syntax:

```
Decl   = NamespaceDecl | UsingDecl | LetDecl | StructDecl | EnumDecl | UnionDecl | VariantDecl | ContractDecl | FuncDecl .
```

### Namespace Declarations

Syntax:

```
NamespaceDecl = "namespace" NamePath ";" .
NamePath      = identifier { "::" identifier } .
```

Example:

```
namespace std;
namespace std::io;
```

### Using Declarations

Syntax:

```
UsingDecl = "using" NamePath ";" .
```

Example:

```
using std;
using std::io;
```

### Let Declarations

Syntax:

```
LetDecl = [ CtAnnotations ] "let" identifier ( ":" Type | "=" Expr |  ":"  Type "=" Expr ) ";" .
```

Example:

```
let PI = 3.1415926;
let PI: f64 = 3.1415926;
let TMP: i32;
@import("COUNT") let COUNT: usize;
```

### Struct Declarations

Syntax:

```
StructDecl       = [ CtAnnotations ] "struct" identifier [ "<" GenericParams ">" ] ( ";" | "{" [ StructDeclFields ] "}" ) .

StructDeclFields = StructDeclField { "," StructDeclField } [ "," ] .
StructDeclField  = [ CtAnnotations ] identifier ":" Type .

GenericParams    = GenericParam { "," GenericParam } .
GenericParam     = [ CtAnnotations ] identifier .
```

Example:

```

@intrinsic struct i32;

struct Vector2F {
	x: f32,
	y: f32,
}

struct Vector2<T> { x: T, y: T }

@explicit @pack(4) struct Foo{
	@offset(0) as_i32: i32,
	@offset(0) as_f32: f32,
}
```

### Enum Declarations

Syntax:

```
EnumDecl       = [ CtAnnotations ] "enum" identifier [ ":" Type ] ( ";" | "{" [ EnumDeclFields ] "}" ) .

EnumDeclFields = EnumDeclField { "," EnumDeclField } [ "," ] .
EnumDeclField  = [ CtAnnotations ] identifier [ "=" Expr ] .
```

Example:

```
@annotation enum Enum;

enum Color {
	Red,
	Green,
	Blue,
}

@flag
enum SomeFlag: u32 {
	A = 0x0001_u32,
	B = 0x0002_u32,
	C = 0x0004_32
}
```

### Union Declarations

Syntax:

```
TODO
```

Example:

```
TODO
```

### Variant Declarations

Syntax:

```
TODO
```

Example:

```
TODO
```

### Contract Declarations

Syntax:

```
TODO
```

Example:

```
TODO
```

### Funct Declarations

Syntax:

```
TODO
```

Example:

```
TODO
```

## Statments

Syntax:

```
Stmt = BodyStmt | LetStmt | AssignStmt | ExprStmt | IfStmt | LoopStmt | BreakStmt | ContinueStmt | ReturnStmt | WhileStmt .
```

### Body Statements

Syntax:

```
TODO
```

Example:

```
TODO
```

### Let Statements

Syntax:

```
TODO
```

Example:

```
TODO
```

### Assign Statements

Syntax:

```
TODO
```

Example:

```
TODO
```

### Expression Statements

Syntax:

```
TODO
```

Example:

```
TODO
```

### If Statements

Syntax:

```
TODO
```

Example:

```
TODO
```

### Loop Statements

Syntax:

```
TODO
```

Example:

```
TODO
```

### Break Statements

Syntax:

```
TODO
```

Example:

```
TODO
```

### Continue Statements

Syntax:

```
TODO
```

Example:

```
TODO
```

### Return Statements

Syntax:

```
TODO
```

Example:

```
TODO
```

### While Statements

Syntax:

```
TODO
```

Example:

```
TODO
```

## Expressions






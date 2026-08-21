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
loop        while       break       continue    return      readonly
writeonly   set
```

### Operators and Punctuation

```
+       &       &&      ==      (       )       =
-       |       ||      !=      [       ]       @
*       ~       !       <       {       }       $
/       ^       ^^      >       ,       ;       ::
%       <<              <=      .       :       
        >>              >=
```

## Common

### Identifiers

Syntax:

```
identifier = letter { letter | decimal_digit } .
```

### Compile-Time Annotations

Syntax:

```
CtAnnotation  = "@" identifier [ "(" CallArgs ")" ] .
CtAnnotations = CtAnnotation { CtAnnotation } .
```

Example:

```
@private
@align(16)
@when(OS_LINUX)
@import("LLVM-C","LLVMContextCreate")
```

### Generic Parameters & Arguments

Syntax:

```
GenericParams = GenericParam { "," GenericParam } .
GenericParam  = [ CtAnnotations ] identifier [ ":" Type ] .

GenericArgs   = GenericArg { "," GenericArg } .
GenericArg    = Type | Expr .
```

### Call Parameters & Arguments

Syntax:

```
CallParams = CallParam { "," CallParam } .
CallParam  = [ CtAnnotations ] identifier ":" Type .

CallArgs   = CallArg { "," CallArg } .
CallArg    = Expr .
```

### Contract Parameters & Arguments

Syntax:

```
ContractParams = ContractParam { "," ContractParam } .
ContractParam  = [ CtAnnotations ] "$" identifier ":" NamedType .

ContractArgs   = ContractArg { "," ContractArg } .
ContractArg    = Expr .
```

### NamePath

Syntax:

```
NamePath = identifier { "::" identifier } .
```

## Program

Syntax:

```
Program = { Decl } .
```

## Types

Syntax:

```
Type = NamedType | RefType | ArrayType | FuncType .
```

### Named Type

Syntax:

```
NamedType =  NamePath [ "<" GenericArgs ">" ] .
```

Example:

```
i32

std::math::Vector2<f32>

```

### Ref Type

Syntax:

```
RefType =  "&" [ "readonly" | "writeonly" ] Type .
```

Example:

```
&i32

&readonly i32

&writeonly Vector2<f32>
```

### Array Type

Syntax:

```
ArrayType =  "[" Expr "]" Type.
```

Example:

```
[5]i32
[5+10]i32
```

### Func Type

Syntax:

```
FuncType =  "&" "func" "(" [ Type { "," Type } ] ")" [CallConv] [ ":" Type ] .

CallConv = "cdecl" | "stdcall" | "winapi" | "thiscall" | "fastcall" .
```

Example:

```
&func()

&func(i32):i32

&func(i32,i32)cdecl:i32
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
StructDecl       = [ CtAnnotations ] "struct" identifier [ "<" GenericParams ">" ]
                   ( ";" | "{" [ StructDeclFields ] "}" ) .

StructDeclFields = StructDeclField { "," StructDeclField } [ "," ] .
StructDeclField  = [ CtAnnotations ] identifier ":" Type .
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
EnumDecl       = [ CtAnnotations ] "enum" identifier [ ":" Type ]
                 ( ";" | "{" [ EnumDeclFields ] "}" ) .

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
	C = 0x0004_u32
}
```

### Union Declarations

Syntax:

```
UnionDecl       = [ CtAnnotations ] "union" identifier [ "<" GenericParams ">" ]
                  ( ";" | "{" [ UnionDeclFields ] "}" ) .

UnionDeclFields = UnionDeclField { "," UnionDeclField } [ "," ] .
UnionDeclField  = [ CtAnnotations ] identifier ":" Type .
```

Example:

```
@intrinsic union IntrinsicUnion;

union SomeUnion {
	as_i32: i32,
	as_f32: f32,
}

union FooUnion<T> { value: T, ptr: &T }
```

### Variant Declarations

Syntax:

```
VariantDecl       = [ CtAnnotations ] "variant" identifier [ "<" GenericParams ">" ] [ ":" Type ]
                    ( ";" | "{" [ VariantDeclFields ] "}" ) .

VariantDeclFields = VariantDeclField { "," VariantDeclField } [","] .
VariantDeclField  = [ CtAnnotations ] identifier [ ":" Type ] .
```

Example:

```
variant Option<T>{
	None,
	Value: T,
}
```

### Contract Declarations

Syntax:

```
ContractDecl = [ CtAnnotations ] "contract" identifier [ "<" GenericParams ">" ]
               "(" [ CallParams ] ")" [ ":" Type ] .
```

Example:

```
contract Addable<TLeft, TRight, TResult>(left: TLeft, right: TRight): TResult;
```

### Funct Declarations

Syntax:

```
FuncDecl = [ CtAnnotations ] "func" identifier
           [ "<" ( GenericParams | ContractParams | GenericParams "," ContractParams ) ">" ]
           "(" [ CallParams ] ")" [ CallConv ] [ ":" Type] [ "fulfills" NamedType { "," NamedType } ]
           ( ";" | BodyStmt ) .
```

Example:

```
func foo(a: i32, b: i32, c: i32): i32{
	return a + b + c;
}

@intrinsic func add_i32(left: i32, right: i32): i32 fulfills Addable<i32,i32,i32>;

@intrinsic func add_f32(left: f32, right: f32): f32 fulfills Addable<f32,f32,f32>;

func add<TLeft,TRight,TResult,$iadd:Addable<TLeft,TRight,TResult>>(left: TLeft, right: TRight): TResult{
	return iadd(left, right);
}

@import("LLVM-C","LLVMContextCreate")
func context_create()cdecl : LLVMContextRef;
```

## Statments

Syntax:

```
Stmt = BodyStmt | LetStmt | SetStmt | ExprStmt | IfStmt | LoopStmt | BreakStmt | ContinueStmt | ReturnStmt | WhileStmt .
```

### Body Statements

Syntax:

```
BodyStmt = "{" { Stmt } "}" .
```

Example:

```
{}

{
	let a = 10;
	let b = 10;
}
```

### Let Statements

Syntax:

```
LetStmt = "let" identifier "=" Expr ";" .
```

Example:

```
let a = 10;
```

### Set Statements

Syntax:

```
SetStmt = "set" Expr "=" Expr ";" .
```

Example:

```
set a = 10;
```

### Expression Statements

Syntax:

```
ExprStmt = Expr ";".
```

Example:

```
foo();
```

### If Statements

Syntax:

```
IfStmt = "if" Expr BodyStmt [ "else" ( BodyStmt | IfStmt ) ] .
```

Example:

```
if cond { }

if cond { } else { }

if cond1 { } else if cond2 { } else { }
```

### Loop Statements

Syntax:

```
LoopStmt = "loop" BodyStmt .
```

Example:

```
loop { }
```

### Break Statements

Syntax:

```
BreakStmt = "break" ";".
```

Example:

```
break;
```

### Continue Statements

Syntax:

```
ContinueStmt = "continue" ";".
```

Example:

```
continue;
```

### Return Statements

Syntax:

```
ReturnStmt = "return" [ Expr ] ";".
```

Example:

```
return;

return 10;
```

### While Statements

Syntax:

```
WhileStmt = "while" Expr BodyStmt .
```

Example:

```
while cond { }
```

## Expressions

Syntax:

```
Expr = IntLitExpr | FloatLitExpr | StructLitExpr | ArrayLitExpr | StringLitExpr | RuneLitExpr
     | .
```

### Integer Literals

Syntax:

```
int_lit        = (decimal_lit | binary_lit | octal_lit | hex_lit) [ [ "_" ] int_lit_suffix ].

decimal_lit    = ( "0" | ( "1" … "9" ) [ "_" ] decimal_digits ) .
binary_lit     = "0" ( "b" | "B" ) [ "_" ] binary_digits .
octal_lit      = "0" ( "o" | "O" ) [ "_" ] octal_digits .
hex_lit        = "0" ( "x" | "X" ) [ "_" ] hex_digits .

decimal_digits = decimal_digit { [ "_" ] decimal_digit } .
binary_digits  = binary_digit { [ "_" ] binary_digit } .
octal_digits   = octal_digit { [ "_" ] octal_digit } .
hex_digits     = hex_digit { [ "_" ] hex_digit } .

int_lit_suffix = "i8" | "i16" | "i32" | "i64" | "i128" | "isize" | "i"
               | "u8" | "u16" | "u32" | "u64" | "u128" | "usize" | "u" .
```

Example:

```
// decimal
0 0i32 0_i32 1 1i32 1_i32 12 12i32 12_i32 1_2 1_2i32 1_2_i32

// binary
0b0 0b01 0b1 0b_0 0b_0000_1111 0B_0000_1111_u8

// octal
0o_123 0O_123

// hex
0x_FFFF 0X_FFFF

// suffix
0_i8 0_i16 0_i32 0_i64 0_i128 0_isize 0_i
0_u8 0_u16 0_u32 0_u64 0_u128 0_usize 0_u

// invalid
01 00       // leading zero
0_0         // zero cannot carry separators
1__2        // consecutive underscores
0b__0       // consecutive underscores
1_          // trailing underscore
0b 0x       // missing digits
0x_         // missing digits after underscore
0o8         // 8 is not an octal digit
```

### Float Literals

Syntax:

```
float_lit         = ( decimal_lit [ "." decimal_digits ] float_exponent [ [ "_" ] float_lit_suffix ] )
                  | ( decimal_lit "." [ decimal_digits ] [ [ "_" ] float_lit_suffix ] )
                  | ( decimal_lit [ "_" ] float_lit_suffix ) .

float_exponent    = ( "e" | "E" ) [ "+" | "-" ] [ "_" ] decimal_lit .
float_lit_suffix  = "f32" | "f64" | "f" | "d" .
```

Example:

```
// exponent
1e5  1e5_f32  1.5e5  1.5e5_f32  1e+5  1e-5  1e_5

// dot
1.  1.5  1.5_f32  1.5f32  0.5

// suffix
1f  1f32  1_f32  0d

// invalid
.5       // missing integer part
01.5     // leading zero
1.e5     // no digits after dot
1e05     // leading zero in exponent
1e       // missing exponent digits
1.5e     // missing exponent digits
1e5.5    // extra dot
1e5_     // trailing underscore
1__5     // consecutive underscores
```

### Rune Literals

Syntax:

```

```

Example:

```
```



### String Literals

Syntax:

```

```

Example:

```

```



### Struct Literals

Syntax:

```

```

Example:

```

```



### Array Literals

Syntax:

```

```

Example:

```

```








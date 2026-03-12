#!/bin/bash
set -euo pipefail

# Find bc binary: use BC env var, or look in current dir, or build it
if [[ -n "${BC:-}" ]]; then
    BC_BIN="$BC"
elif [[ -x "./bc" ]]; then
    BC_BIN="./bc"
else
    # Fallback: build with make if no binary found (standalone mode)
    CXX="${CXX:-g++}" CXXFLAGS="${CXXFLAGS:--O3 -Wall -Wextra -std=c++20}" make -C "$(dirname "$0")/.." bc
    BC_BIN="$(dirname "$0")/../bc"
fi

echo "Using bc binary: $BC_BIN"

FAILED=0
testcase_ok() { "$BC_BIN" -c "$2" >/dev/null || { echo "FAILED: $1 (erroneously rejected)"; FAILED=1; }; }
testcase_chk() { "$BC_BIN" -a "$2" | "${@:3}" || { echo "FAILED: $1"; FAILED=1; }; }
testcase_err() { ! "$BC_BIN" -c "$2" 2>/dev/null || { echo "FAILED: $1 (erroneously accepted)"; FAILED=1; }; }

# NOTE: testcase_ok = should accept; testcase_err = should fail with an error.

# Parameters
testcase_ok "empty program" <(echo "")
testcase_ok "simple function" <(echo "fn(){}")
testcase_ok "params 1" <(echo "fn(a){}")
testcase_ok "params 2" <(echo "fn(a,b){}")
testcase_err "params 3 (duplicate)" <(echo "fn(a,a){}")
testcase_err "params 4 (duplicate)" <(echo "fn(a,b,a){}")

# Invalid tokens
testcase_err "invalid token 1" <(echo '`')
testcase_err "invalid token 2" <(echo '\')
testcase_err "invalid token 3" <(echo -e '\1')
testcase_err "unexpected token 1" <(echo '% fn(){}')
testcase_err "unexpected token 2" <(echo '! fn(){}')
testcase_err "unexpected token 3" <(echo '* fn(){}')
testcase_err "unexpected token 4" <(echo '{} fn(){}')
testcase_err "unexpected token 5" <(echo '( fn(){}')
testcase_err "unexpected token 6" <(echo '+ fn(){}')
testcase_err "unexpected token 7" <(echo 'fn(){} %')
testcase_err "unexpected token 8" <(echo 'fn(){} !')
testcase_err "unexpected token 9" <(echo 'fn(){} *')
testcase_err "unexpected token 10" <(echo 'fn(){} {}')
testcase_err "unexpected token 11" <(echo 'fn(){} (')
testcase_err "unexpected token 12" <(echo 'fn(){} +')
testcase_err "unexpected token 13" <(echo 'fn 0(){}')
testcase_err "unexpected token 14" <(echo '0(){}')

# Identifiers
testcase_ok "valid ident 1" <(echo 'f(){}')
testcase_ok "valid ident 2" <(echo 'fffffffff(){}')
testcase_ok "valid ident 3" <(echo 'f_(){}')
testcase_ok "valid ident 4" <(echo 'f0(){}')
testcase_ok "valid ident 5" <(echo 'f0_9A(){}')
testcase_ok "valid ident 6" <(echo '_09(){}')
testcase_ok "valid ident 7" <(echo '_01234567889(){}')
testcase_ok "valid ident 8" <(echo 'abcdefghijklmnopqrstuvwxyz(){}')
testcase_ok "valid ident 9" <(echo 'ABCDEFGHIJKLMNOPQRSTUVWXYZ(){}')
testcase_err "invalid ident 1" <(echo 'f1+(){}')
testcase_err "invalid ident 2" <(echo 'f1-(){}')
testcase_err "invalid ident 3" <(echo 'f1#(){}')
testcase_err "invalid ident 4" <(echo 'f1$(){}')
testcase_err "invalid ident 5" <(echo 'f1[(){}')
testcase_err "invalid ident 6" <(echo 'f1=(){}')
testcase_err "invalid ident 7" <(echo '1(){}')
testcase_err "invalid ident 8" <(echo '1_(){}')

# Comments
testcase_ok "comment 1" <(echo -e "//comment")
testcase_ok "comment 2" <(echo -e "f(){}//comment {")
testcase_ok "comment 2" <(echo -e "f()//comment {\n{}")
testcase_err "comment 2" <(echo -e "f(){//}")
testcase_err "comment 2" <(echo -e "f(){//}\n{//}}")
testcase_ok "comment 2" <(echo -e "f(){//}\n{//}}\n}}")

# Return
testcase_ok "return with value 1" <(echo "f() { return 1; }")
testcase_ok "return with value 2" <(echo "f(a) { return a; }")
testcase_ok "return with value 3" <(echo "f(b) { return b; }")
testcase_ok "return without value" <(echo "f() { return; }")

# Block
testcase_ok "block 1" <(echo "fn(){{return 1;} return 1;}")
testcase_ok "block 2" <(echo "fn(){{{} return 1; {return 1;} return 1; 1; 2; {} {1;} return 1;} return 2;}")
testcase_err "block extra brace 1" <(echo "fn(){{return 1;} } return 1;}")
testcase_err "block extra brace 2" <(echo "fn(){{return 1;} }}")

# If/else
testcase_ok "if 1" <(echo "fn(){if(1){}}")
testcase_ok "if 2" <(echo "fn(){if(1)return 1;}")
testcase_ok "if 3" <(echo "fn(){if(1)return 1;else return 2;}")
testcase_ok "if 4" <(echo "fn(){if(1)return 1;else{return 2;}}")
testcase_ok "if 5" <(echo "fn(){if(1){}else{return 2;}}")
testcase_ok "if 6" <(echo "fn(){if(1){}else{}}")
testcase_ok "if 7" <(echo "fn(){if(1){}else if(2)return 2;}")
testcase_ok "if 8" <(echo "fn(){if(1){}else if(2)return 2;else return 3;}")
testcase_ok "if 9" <(echo "fn(){if(1){}else if(2){}else{}}")
testcase_ok "if 10" <(echo "fn(){if(1){}else {if(2){}else{}}}")
testcase_ok "if 11" <(echo "fn(){if(1)if(2)return 2;else return 3;else return 4;}")
testcase_err "if 12" <(echo "fn(){if(1)if(2)return 2;else return 3;else return 4;else return 5;}")
testcase_chk "if nest 1" <(echo "fn(){if(1)if(2)return 2;else return 3;}") grep -q '(if 1 (if 2 (return 2) (return 3)))'
testcase_chk "if nest 2" <(echo "fn(){if(1){if(2)return 2;}else return 3;}") grep -q '(if 1 (block (if 2 (return 2))) (return 3))'

# While
testcase_ok "while 1" <(echo "fn(){while(1){}}")
testcase_ok "while 2" <(echo "fn(){while(1)return 1;}")
testcase_ok "while 3" <(echo "fn(){while(1)while(1)return 1;}")
testcase_ok "while 4" <(echo "fn(){while(1)if(1)return 1;}")
testcase_ok "while 5" <(echo "fn(){while(1)if(1)return 1;else return 2;}")
testcase_ok "while 6" <(echo "fn(){if(1)while(1)return 1;else return 2;}")
testcase_ok "while 7" <(echo "fn(){if(1)while(1)if(1)return 1;else return 2;else return 3;}")
testcase_ok "while 8" <(echo "fn(){if(1)while(1)if(1)return 1;else return 2;else while(1)if(1)return 1;else return 2;}")
testcase_ok "while 9" <(echo "fn(){if(1)while(1)if(1)return 1;else return 2;while(1)if(1)return 1;else return 2;}")

# Function calls
testcase_ok "call is no ident" <(echo "f(fn) { fn(fn); }")
testcase_ok "call decl" <(echo "f(x) { return x; } g(x) { return f(x); }")
testcase_ok "call imp decl 1" <(echo "g(x) { return f(x + 1); } f(x) { return x; }")
testcase_ok "call imp decl 2" <(echo "g(x) { return f(x + 1); }")
testcase_err "call only on ident 1" <(echo "fn(x) { fn(x)(x); }")
testcase_err "call only on ident 2" <(echo "fn(x) { (fn)(x); }")
testcase_err "call only on ident 3" <(echo "fn(x) { (x)(x); }")
testcase_err "call param count mismatch 1" <(echo "fn(x) { f(1); f(2, 3); }")
testcase_err "call param count mismatch 2" <(echo "fn(x) { f(1, 3); f(2); }")
testcase_err "call param count mismatch 3" <(echo "f(a) {} fn(x) { f(1, 3); }")
testcase_err "call param count mismatch 4" <(echo "f(a, b) {} fn(x) { f(1); }")
testcase_err "call param count mismatch 5" <(echo "f() {} fn(x) { f(1); }")
testcase_err "call param count mismatch 6" <(echo "fn(x) { f(1, 3); } f(a) {}")
testcase_err "call param count mismatch 7" <(echo "fn(x) { f(1); } f(a, b) {}")
testcase_err "call param count mismatch 8" <(echo "fn(x) { f(1); } f() {}")

# Declarations
testcase_ok "decl 1" <(echo "fn(a){register b = a;}")
testcase_ok "decl 2" <(echo "fn(a){auto b = a;}")
testcase_ok "decl 3" <(echo "fn(a){{register a = a;}}")
testcase_ok "decl 4" <(echo "fn(a){{auto a = a;}}")
testcase_err "decl 5" <(echo "fn(a){{auto a = &a;}}")
testcase_err "if decl 1" <(echo "fn(){if(1)register a = 1;}")
testcase_err "if decl 2" <(echo "fn(){if(1){}else register a = 1;}")
testcase_ok "if decl 3" <(echo "fn(){if(1){}else{register a = 1;}}")

# Subscript
testcase_ok "subscript 1" <(echo "f(a, b) { return a[b]; }")
testcase_ok "subscript 2" <(echo "f(a, b) { return a[b@1]; }")
testcase_ok "subscript 3" <(echo "f(a, b) { return a[b@2]; }")
testcase_err "subscript 4" <(echo "f(a, b) { return a[b@3]; }")
testcase_ok "subscript 5" <(echo "f(a, b) { return a[b@4]; }")
testcase_err "subscript 6" <(echo "f(a, b) { return a[b@5]; }")
testcase_err "subscript 7" <(echo "f(a, b) { return a[b@6]; }")
testcase_err "subscript 8" <(echo "f(a, b) { return a[b@7]; }")
testcase_ok "subscript 9" <(echo "f(a, b) { return a[b@8]; }")
testcase_err "subscript 10" <(echo "f(a, b) { return a[b@9]; }")
testcase_err "subscript 11" <(echo "f(a, b) { return a[b@10]; }")
testcase_err "subscript 12" <(echo "f(a, b) { return a[b@a]; }")
testcase_err "subscript 13" <(echo "f(a, b) { return a[b@]; }")

# Address of (&)
testcase_ok "addrof auto" <(echo "f() { auto a = 0; return &a; }")
testcase_ok "addrof subscript 1" <(echo "f(a, b) { return &a[b]; }")
testcase_ok "addrof subscript 2" <(echo "f(a, b) { return &a[b@1]; }")
testcase_ok "addrof subscript 3" <(echo "f(a, b) { return &a[b@2]; }")
testcase_ok "addrof subscript 4" <(echo "f(a, b) { return &a[b@4]; }")
testcase_ok "addrof subscript 5" <(echo "f(a, b) { return &a[b@8]; }")
testcase_err "addrof param" <(echo "f(a) { return &a; }")
testcase_err "addrof reg" <(echo "f() { register a = 0; return &a; }")

# Scoping
testcase_ok "scope 1" <(echo "f(a) { return a; }")
testcase_ok "scope 2" <(echo "f(a) { { register b = a; return b; } }")
testcase_err "scope 3" <(echo "f(a) { { register b = a; } return b; }")
testcase_ok "scope 4" <(echo "f(a) { { register b = a; } { auto b = a; return &b; } }")
testcase_err "scope 4" <(echo "f(a) { { register b = a; } { auto b = a; } return &b; }")

# Variable shadowing
testcase_err "shadow 1" <(echo "f() { register a = 1; { auto a = 2; } return &a; }")
testcase_err "shadow 2" <(echo "f() { register a = 1; register a = 2; }")
testcase_err "shadow 3" <(echo "f() { register a = 1; auto a = 2; }")
testcase_err "shadow 4" <(echo "f() { auto a = 1; auto a = 2; }")
testcase_err "shadow 5" <(echo "f() { auto a = 1; register a = 2; }")
testcase_ok "shadow 6" <(echo "f() { auto a = 1; { auto a = 2; return a; } }")
testcase_ok "shadow 7" <(echo "f() { auto a = 1; { auto a = 2; } return a; }")
testcase_ok "shadow 8" <(echo "f() { auto a = 1; { register a = 2; return a; } }")
testcase_ok "shadow 9" <(echo "f() { register a = 1; { auto a = 2; } return a; }")
testcase_ok "shadow 10" <(echo "f() { register a = 1; { auto a = 2; return &a; } }")
testcase_err "shadow param 1" <(echo "f(a) { register a = 1; }")
testcase_err "shadow param 2" <(echo "f(a) { auto a = 1; }")
testcase_ok "shadow param 3" <(echo "f(a) { { register a = 1; } }")
testcase_ok "shadow param 4" <(echo "f(a) { { auto a = 1; } }")
testcase_ok "shadow param 5" <(echo "f(a) { { auto a = 1; } return a; }")
testcase_err "shadow param 6" <(echo "f(a) { { auto a = 1; } return &a; }")

# Expression precedence and associativity
testcase_chk "unary 1" <(echo "f(a) { return !a; }") grep -q '(! [^ )]\+)'
testcase_chk "unary 2" <(echo "f(a) { return ~a; }") grep -q '(~ [^ )]\+)'
testcase_chk "unary 3" <(echo "f(a) { return !!a; }") grep -q '(! (! [^ )]\+))'
testcase_chk "unary 4" <(echo "f(a) { return ~!a; }") grep -q '(~ (! [^ )]\+))'
testcase_chk "unary 5" <(echo "f(a) { return !~a; }") grep -q '(! (~ [^ )]\+))'
testcase_chk "unary call 1" <(echo "f(a) { return !fn(); }") grep -q '(return (!'
testcase_chk "unary call 2" <(echo "f(a) { return -fn(); }") grep -q '(return (u-'
testcase_chk "add 1" <(echo "f(a,b,c) { return a + b + c; }") grep -q '(+ (+ [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "add 2" <(echo "f(a,b,c,d) { return a + b + c + d; }") grep -q '(+ (+ (+ [^ )]\+ [^ )]\+) [^ )]\+) [^ )]\+)'
testcase_chk "add 3" <(echo "f(a,b,c) { return a + b - c; }") grep -q '(- (+ [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "add 4" <(echo "f(a,b,c) { return a - b - c; }") grep -q '(- (- [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "add 5" <(echo "f(a,b,c) { return (a + b) - c; }") grep -q '(- (+ [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "add 6" <(echo "f(a,b,c) { return a + (b - c); }") grep -q '(+ [^ )]\+ (- [^ )]\+ [^ )]\+))'
testcase_chk "add unary 1" <(echo "f(a,b,c) { return a + !b + c; }") grep -q '(+ (+ [^ )]\+ (! [^ )]\+)) [^ )]\+)'
testcase_chk "add unary 2" <(echo "f(a,b,c) { return !a + !b + c; }") grep -q '(+ (+ (! [^ )]\+) (! [^ )]\+)) [^ )]\+)'
testcase_chk "add unary 3" <(echo "f(a,b,c) { return !(a + !b) + c; }") grep -q '(+ (! (+ [^ )]\+ (! [^ )]\+))) [^ )]\+)'
testcase_chk "add unary 4" <(echo "f(a,b,c) { return a + b + - c; }") grep -q '(+ (+ [^ )]\+ [^ )]\+) (u- [^ )]\+))'
testcase_chk "add unary 5" <(echo "f(a,b,c) { return a + b - - c; }") grep -q '(- (+ [^ )]\+ [^ )]\+) (u- [^ )]\+))'
testcase_chk "add lt 1" <(echo "f(a,b,c) { return a + b < a + c; }") grep -q '(< (+ [^ )]\+ [^ )]\+) (+ [^ )]\+ [^ )]\+))'
testcase_chk "add lt 2" <(echo "f(a,b,c) { return a + (b < a + c); }") grep -q '(+ [^ )]\+ (< [^ )]\+ (+ [^ )]\+ [^ )]\+))'
testcase_chk "lt lt 1" <(echo "f(a,b,c) { return a < b < c; }") grep -q '(< (< [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "lt lt 2" <(echo "f(a,b,c) { return a > b < c; }") grep -q '(< (> [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "lt lt 3" <(echo "f(a,b,c) { return a < b > c; }") grep -q '(> (< [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "add eq 1" <(echo "f(a,b,c) { return a + b == a + c; }") grep -q '(== (+ [^ )]\+ [^ )]\+) (+ [^ )]\+ [^ )]\+))'
testcase_chk "add eq 2" <(echo "f(a,b,c) { return a + (b == a + c); }") grep -q '(+ [^ )]\+ (== [^ )]\+ (+ [^ )]\+ [^ )]\+))'
testcase_chk "lt eq 1" <(echo "f(a,b,c) { return a < b == a < c; }") grep -q '(== (< [^ )]\+ [^ )]\+) (< [^ )]\+ [^ )]\+))'
testcase_chk "lt eq 2" <(echo "f(a,b,c) { return a < (b == a < c); }") grep -q '(< [^ )]\+ (== [^ )]\+ (< [^ )]\+ [^ )]\+))'
testcase_chk "eq eq 1" <(echo "f(a,b,c) { return a == b == c; }") grep -q '(== (== [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "eq eq 2" <(echo "f(a,b,c) { return a != b == c; }") grep -q '(== (!= [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "eq eq 3" <(echo "f(a,b,c) { return a == b != c; }") grep -q '(!= (== [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "eq eq 4" <(echo "f(a,b,c) { return a != b != c; }") grep -q '(!= (!= [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "and and 1" <(echo "f(a,b,c) { return a && b && c; }") grep -q '(&& (&& [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "and and 2" <(echo "f(a,b,c) { return a == b && c; }") grep -q '(&& (== [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "and and 3" <(echo "f(a,b,c) { return a && b == c; }") grep -q '(&& [^ )]\+ (== [^ )]\+ [^ )]\+))'
testcase_chk "or or 1" <(echo "f(a,b,c) { return a || b || c; }") grep -q '(|| (|| [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "or or 2" <(echo "f(a,b,c) { return a && b || c; }") grep -q '(|| (&& [^ )]\+ [^ )]\+) [^ )]\+)'
testcase_chk "or or 3" <(echo "f(a,b,c) { return a || b && c; }") grep -q '(|| [^ )]\+ (&& [^ )]\+ [^ )]\+))'
testcase_chk "assign 1" <(echo "f(a,b,c) { return a = b = c; }") grep -q '(= [^ )]\+ (= [^ )]\+ [^ )]\+))'
testcase_err "assign 2" <(echo "f(a,b,c) { return a && b = c; }")
testcase_chk "assign 3" <(echo "f(a,b,c) { return a && (b = c); }") grep -q '(&& [^ )]\+ (= [^ )]\+ [^ )]\+))'
testcase_chk "assign 4" <(echo "f(a,b,c) { return a = b && c; }") grep -q '(= [^ )]\+ (&& [^ )]\+ [^ )]\+))'
testcase_ok "subscript nested 1" <(echo "f(a, b) { return a[b][b]; }")
testcase_ok "subscript nested 2" <(echo "f(a, b) { return a[b@2][b@1]; }")
testcase_chk "subscript call" <(echo "f(a) { return fn()[a]; }") grep -q '(return (\[\]'
testcase_chk "unary subscript 1" <(echo "f(a,b) { return -a[b]; }") grep -q '(u- (\[\]'
testcase_chk "unary subscript 2" <(echo "f(a,b) { return -a[b=a]; }") grep -q '(u- (\[\]'
testcase_chk "unary subscript 3" <(echo "f(a,b) { return -a[b=a][b]; }") grep -q '(u- (\[\]'
testcase_chk "unary subscript call" <(echo "f(a) { return -fn()[a]; }") grep -q '(return (u- (\[\]'

# Expressions lvalue-ness
testcase_err "assign lvalue 1" <(echo "f(a, b) { -a = b; }")
testcase_err "assign lvalue 2" <(echo "f(a, b) { a + 1 = b; }")
testcase_ok "assign lvalue 3" <(echo "f(a, b) { a[b] = b; }")
testcase_err "assign lvalue 4" <(echo "f(a, b) { a - b = b; }")
testcase_err "assign lvalue 5" <(echo "f(a, b) { g(a) = b; }")
testcase_ok "assign lvalue 6" <(echo "f(a, b) { g(a)[0] = b; }")
testcase_err "addrof lvalue 1" <(echo "f(a) { auto b = a; return &-a; }")
testcase_err "addrof lvalue 2" <(echo "f(a) { auto b = a; return &a + 1; }")
testcase_ok "addrof lvalue 3" <(echo "f(a) { auto b = a; return &a[b]; }")
testcase_err "addrof lvalue 4" <(echo "f(a) { auto b = a; return &a - b; }")
testcase_err "addrof lvalue 5" <(echo "f(a) { auto b = a; return &g(a); }")
testcase_ok "addrof lvalue 6" <(echo "f(a) { auto b = a; return &g(a)[0]; }")

# Performance test. Reduce n (nesting) and r (repeat) for smaller workload.
testcase_ok "bignest1000r10" <(python3 -c "n=1000;r=10;print(''.join(f'func{j}(a){{' + ''.join(f'{{register a{i}=a{(i-1)//2 if i>1 else \"\"}; ' for i in range(n)) + f'return a+a{n//2}+a{n-1};' + '}'*(n+1) for j in range(r)))")

exit $FAILED

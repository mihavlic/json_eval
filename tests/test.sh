function test() {
    echo ">>> $1"
    echo -n "<<< "
    ./build/src/json_eval tests/test.json "$1" | tail -n 1
    echo -e "### Should be $2\n"
}

test "a.b[1]"       '2'
test "a.b[2].c"     'test'
test "a.b"          '[ 1, 2, { "c": "test" }, [11, 12] ]'

# Expressions in the subscript operator []:
test "a.b[a.b[1]].c" test

# Intrinsic functions: min, max, size:
test "max(a.b[0], a.b[1])"  2
test "min(a.b[3])"          11
# size - returns size of the passed object, array or string
test "size(a)"              1
test "size(a.b)"            4
test "size(a.b[a.b[1]].c)"  4
# Number literals:
test "max(a.b[0], 10, a.b[1], 15)"     15

BIN="build/src/json_eval"

$BIN "tests/test.json" 'a.b[2].c'
echo '> should be "test"'

$BIN "tests/test.json" 'max(1, 2, 15)'
echo '> should be 15'


$BIN "tests/test.json" 'size(a)'
echo '> should be 1'

# json-eval

This is a program that parses a json file and evaluates an expression on that.

The plan is to do the following:
- Parse the json using a scannerless recursive descent parser into an arena. Accumulate all errors during parsing.
- Parse the expression using the same approach.
- I suppose both steps could be done in parallel, but that seems silly.
- Maybe run some naive optimization on the expression.
- Evaluate the expression using a tree walking interpreter.
- The task really wants us to use multithreading, perhaps we can do that during evaluation?
- The evaluation should result in a single json value (or errors), print those.


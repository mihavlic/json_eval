#pragma once

#include "ast.h"
#include "parser.h"

AstNode parse_json(Parser &p, Arena &arena);

AstNode parse_expression(Parser &p, Arena &arena);

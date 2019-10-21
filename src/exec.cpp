#define genf(...)   gen_result = strf("%s%s", gen_result, strf(__VA_ARGS__))
#define genlnf(...) gen_result = strf("%s\n", gen_result); gen_indentation(); genf(__VA_ARGS__)
#define genln()     gen_result = strf("%s\n", gen_result); gen_indentation()

internal_proc void exec_stmt(Resolved_Stmt *stmt);
internal_proc void exec(Resolved_Templ *templ);

global_var char *gen_result = "";
global_var int gen_indent   = 0;

Resolved_Templ *global_current_tmpl;
Resolved_Stmt  *global_super_block;

internal_proc void
gen_indentation() {
    gen_result = strf("%s%*.s", gen_result, 4 * gen_indent, "         ");
}

struct Iterator {
    Val *container;
    int pos;
    Val *val;
};

internal_proc Iterator
init(Val *container) {
    Iterator result = {};

    assert(container->kind == VAL_RANGE || container->kind == VAL_LIST || container->kind == VAL_TUPLE);

    result.container = container;
    result.pos = 0;
    result.val = val_item(container, 0);

    return result;
}

internal_proc b32
valid(Iterator *it) {
    b32 result = it->pos < it->container->len;

    return result;
}

internal_proc void
next(Iterator *it) {
    it->pos += 1;
    it->val = val_item(it->container, it->pos);
}

internal_proc Val *
exec_macro(Resolved_Expr *expr) {
    Type *type = expr->type;
    assert(type->kind == TYPE_MACRO);

    for ( int i = 0; i < type->type_macro.num_params; ++i ) {
        Type_Field *field = type->type_macro.params[i];

        if ( i < expr->expr_call.num_args ) {
            field->sym->val = expr->expr_call.args[i]->val;
        } else {
            field->sym->val = field->default_value;
        }
    }

    for ( int i = 0; i < type->type_macro.num_stmts; ++i ) {
        Resolved_Stmt *stmt = type->type_macro.stmts[i];
        exec_stmt(stmt);
    }

    return &val_none;
}

internal_proc Val *
exec_expr(Resolved_Expr *expr) {
    Val *result = 0;

    switch (expr->kind) {
        case EXPR_NAME: {
            result = expr->sym->val;
        } break;

        case EXPR_INT: {
            result = expr->val;
        } break;

        case EXPR_FLOAT: {
            result = expr->val;
        } break;

        case EXPR_STR: {
            result = expr->val;
        } break;

        case EXPR_RANGE: {
            result = val_range(expr->expr_range.min, expr->expr_range.max);
        } break;

        case EXPR_UNARY: {
            result = val_op(expr->expr_unary.op, exec_expr(expr->expr_unary.expr));
        } break;

        case EXPR_FIELD: {
            result = expr->val;
        } break;

        case EXPR_BINARY: {
            switch ( expr->expr_binary.op ) {
                case T_MUL: {
                    Val calc = *exec_expr(expr->expr_binary.left) * *exec_expr(expr->expr_binary.right);
                    result = val_copy(&calc);
                } break;

                case T_DIV: {
                    Val calc = *exec_expr(expr->expr_binary.left) / *exec_expr(expr->expr_binary.right);
                    result = val_copy(&calc);
                } break;

                case T_PLUS: {
                    Val calc = *exec_expr(expr->expr_binary.left) + *exec_expr(expr->expr_binary.right);
                    result = val_copy(&calc);
                } break;

                case T_MINUS: {
                    Val calc = *exec_expr(expr->expr_binary.left) - *exec_expr(expr->expr_binary.right);
                    result = val_copy(&calc);
                } break;

                case T_LT: {
                    Val calc = *exec_expr(expr->expr_binary.left) < *exec_expr(expr->expr_binary.right);
                    result = val_copy(&calc);
                } break;

                case T_AND: {
                    b32 calc = *exec_expr(expr->expr_binary.left) && *exec_expr(expr->expr_binary.right);
                    result = val_bool(calc);
                } break;

                case T_OR: {
                    b32 calc = *exec_expr(expr->expr_binary.left) || *exec_expr(expr->expr_binary.right);
                    result = val_bool(calc);
                } break;

                default: {
                    illegal_path();
                } break;
            }
        } break;

        case EXPR_IS: {
            Val *var_val = exec_expr(expr->expr_is.expr);
            Type *type = expr->expr_is.test->type;
            assert(type->kind == TYPE_TEST);

            result = type->type_test.callback(var_val, expr->expr_is.args, expr->expr_is.num_args);
        } break;

        case EXPR_IN: {
            result = exec_expr(expr->expr_in.set);
        } break;

        case EXPR_CALL: {
            Type *type = expr->type;

            if ( type->kind == TYPE_MACRO ) {
                result = exec_macro(expr);
            } else {
                result = type->type_proc.callback(0);
            }
        } break;

        case EXPR_TUPLE: {
            for ( int i = 0; i < expr->expr_list.num_expr; ++i ) {
                val_set(expr->val, exec_expr(expr->expr_list.expr[i]), i);
            }

            result = expr->val;
        } break;

        case EXPR_LIST: {
            for ( int i = 0; i < expr->expr_list.num_expr; ++i ) {
                val_set(expr->val, exec_expr(expr->expr_list.expr[i]), i);
            }

            result = expr->val;
        } break;

        case EXPR_BOOL: {
            result = expr->val;
        } break;

        case EXPR_DICT: {
            result = expr->val;
        } break;

        default: {
            illegal_path();
        } break;
    }

    for ( int i = 0; i < expr->num_filters; ++i ) {
        Resolved_Filter *filter = expr->filters[i];
        result = filter->proc(result, filter->args, filter->num_args);
    }

    return result;
}

internal_proc b32
if_expr_cond(Resolved_Expr *if_expr) {
    if ( !if_expr ) {
        return true;
    }

    Val *if_val = exec_expr(if_expr->expr_if.cond);
    assert(if_val->kind == VAL_BOOL);

    return val_bool(if_val);
}

internal_proc void
exec_extends(Resolved_Stmt *stmt, Resolved_Templ *templ) {
    assert(stmt->kind == STMT_EXTENDS);

    for ( int i = 0; i < templ->num_stmts; ++i ) {
        Resolved_Stmt *parent_stmt = templ->stmts[i];
        exec_stmt(parent_stmt);
    }

    for ( int i = 0; i < global_current_tmpl->num_stmts; ++i ) {
        Resolved_Stmt *child_stmt = global_current_tmpl->stmts[i];

        if ( !child_stmt || child_stmt->kind == STMT_EXTENDS ) {
            continue;
        }

        assert(child_stmt->kind != STMT_LIT);
        Resolved_Stmt *block = (Resolved_Stmt *)map_get(&templ->blocks, child_stmt->stmt_block.name);
        if ( !block ) {
            exec_stmt(child_stmt);
        }
    }
}

internal_proc void
exec_stmt(Resolved_Stmt *stmt) {
    switch ( stmt->kind ) {
        case STMT_LIT: {
            genf("%s", stmt->stmt_lit.lit);
        } break;

        case STMT_VAR: {
            Resolved_Expr *if_expr = stmt->stmt_var.if_expr;

            if ( !if_expr || if_expr_cond(if_expr) ) {
                Val *value = exec_expr(stmt->stmt_var.expr);

                genf("%s", to_char(value));
            } else {
                if ( if_expr->expr_if.else_expr ) {
                    Val *else_val = exec_expr(if_expr->expr_if.else_expr);

                    genf("%s", to_char(else_val));
                }
            }
        } break;

        case STMT_BLOCK: {
            Resolved_Stmt *block = (Resolved_Stmt *)map_get(&global_current_tmpl->blocks, stmt->stmt_block.name);
            if ( block ) {
                global_super_block = stmt;
                genf("<!-- kindtemplate inhalt -->");
            } else {
                block = stmt;
            }

            genlnf("<!-- %s -->", block->stmt_block.name);
            genln();

            for ( int i = 0; i < block->stmt_block.num_stmts; ++i ) {
                exec_stmt(block->stmt_block.stmts[i]);
            }
        } break;

        case STMT_SET: {
            stmt->stmt_set.sym->val = exec_expr(stmt->stmt_set.expr);
        } break;

        case STMT_FOR: {
            Val *list = exec_expr(stmt->stmt_for.expr);

            if ( list->len ) {
                for ( Iterator it = init(list); valid(&it); next(&it) ) {
                    stmt->stmt_for.it->val = it.val;

                    for ( int j = 0; j < stmt->stmt_for.num_stmts; ++j ) {
                        exec_stmt(stmt->stmt_for.stmts[j]);
                    }

                    val_inc(stmt->stmt_for.loop_index->val);
                    val_inc(stmt->stmt_for.loop_index0->val);
                }
            } else if ( stmt->stmt_for.else_stmts ) {
                for ( int i = 0; i < stmt->stmt_for.num_else_stmts; ++i ) {
                    exec_stmt(stmt->stmt_for.else_stmts[i]);
                }
            }
        } break;

        case STMT_IF: {
            Val *val = exec_expr(stmt->stmt_if.expr);

            if ( val_bool(val) ) {
                for ( int i = 0; i < stmt->stmt_if.num_stmts; ++i ) {
                    exec_stmt(stmt->stmt_if.stmts[i]);
                }
            } else {
                b32 elseif_matched = false;
                for ( int i = 0; i < stmt->stmt_if.num_elseifs; ++i ) {
                    Resolved_Stmt *elseif = stmt->stmt_if.elseifs[i];
                    val = exec_expr(elseif->stmt_if.expr);

                    if ( val_bool(val) ) {
                        for ( int j = 0; j < elseif->stmt_if.num_stmts; ++j ) {
                            exec_stmt(elseif->stmt_if.stmts[j]);
                        }
                        elseif_matched = true;
                        break;
                    }
                }

                if ( !elseif_matched && stmt->stmt_if.else_stmt ) {
                    Resolved_Stmt *else_stmt = stmt->stmt_if.else_stmt;
                    for ( int i = 0; i < else_stmt->stmt_if.num_stmts; ++i ) {
                        exec_stmt(else_stmt->stmt_if.stmts[i]);
                    }
                }
            }
        } break;

        case STMT_EXTENDS: {
            if ( stmt->stmt_extends.if_expr ) {
                Resolved_Expr *if_expr = stmt->stmt_extends.if_expr;
                Val *if_cond = exec_expr(if_expr->expr_if.cond);
                assert(if_cond->kind == VAL_BOOL);

                if ( val_bool(if_cond) ) {
                    exec_extends(stmt, stmt->stmt_extends.tmpl);
                } else if ( if_expr->expr_if.else_expr ) {
                    exec_extends(stmt, stmt->stmt_extends.else_tmpl);
                }
            } else {
                exec_extends(stmt, stmt->stmt_extends.tmpl);
            }
        } break;

        case STMT_INCLUDE: {
            for ( int i = 0; i < stmt->stmt_include.num_templ; ++i ) {
                Resolved_Templ *templ = stmt->stmt_include.templ[i];
                for ( int j = 0; j < templ->num_stmts; ++j ) {
                    exec_stmt(templ->stmts[j]);
                }
            }
        } break;

        case STMT_FILTER: {
            char *old_gen_result = gen_result;
            char *temp = "";
            gen_result = temp;

            for ( int i = 0; i < stmt->stmt_filter.num_stmts; ++i ) {
                exec_stmt(stmt->stmt_filter.stmts[i]);
            }

            Val *result = val_str(gen_result);
            for ( int i = 0; i < stmt->stmt_filter.num_filter; ++i ) {
                Resolved_Filter *filter = stmt->stmt_filter.filter[i];
                result = filter->proc(result, filter->args, filter->num_args);
            }

            gen_result = old_gen_result;
            genf("%s", to_char(result));
        } break;

        case STMT_RAW: {
            genf("%s", stmt->stmt_raw.value);
        } break;

        case STMT_MACRO:
        case STMT_FROM_IMPORT:
        case STMT_IMPORT: {
            /* @AUFGABE: nix tun */
        } break;

        default: {
            implement_me();
        } break;
    }
}

PROC_CALLBACK(super) {
    assert(global_super_block);
    assert(global_super_block->kind == STMT_BLOCK);

    for ( int i = 0; i < global_super_block->stmt_block.num_stmts; ++i ) {
        exec_stmt(global_super_block->stmt_block.stmts[i]);
    }

    return val_str("");
}

internal_proc void
exec(Resolved_Templ *templ) {
    global_current_tmpl = templ;

    for ( int i = 0; i < templ->num_stmts; ++i ) {
        Resolved_Stmt *stmt = templ->stmts[i];
        if ( !stmt ) {
            continue;
        }

        if ( stmt->kind == STMT_EXTENDS && i > 0 ) {
            fatal("extends anweisung muss die erste anweisung des templates sein");
        }

        exec_stmt(stmt);

        /* @AUFGABE: wird das noch benötigt? */
        if ( stmt->kind == STMT_EXTENDS ) {
            break;
        }
    }
}

// Some ideas: To implement extensibility of numbers?
//             i.e. supporting numbers larger than declared type.

#include "builtin.h"

void lenv_add_builtins(lenv *env) {

    lenv_add_builtin_func(env, "load", builtin_load);
    lenv_add_builtin_func(env, "error", builtin_error);
    lenv_add_builtin_func(env, "print", builtin_print);

    lenv_add_builtin_func(env, "def", builtin_def); // Global assignment
    lenv_add_builtin_func(env, "=", builtin_put); // Local assignment
    lenv_add_builtin_func(env, "\\", builtin_lambda);

    // Comparisons
    lenv_add_builtin_func(env, "or", builtin_or);
    lenv_add_builtin_func(env, "and", builtin_and);
    lenv_add_builtin_func(env, "==", builtin_eq);
    lenv_add_builtin_func(env, "!=", builtin_neq);
    lenv_add_builtin_func(env, ">", builtin_greater);
    lenv_add_builtin_func(env, ">=", builtin_greater_eq);
    lenv_add_builtin_func(env, "<", builtin_lesser);
    lenv_add_builtin_func(env, "<=", builtin_lesser_eq);
    lenv_add_builtin_func(env, "bool", builtin_bool);
    lenv_add_builtin_func(env, "!", builtin_negate);
    lenv_add_builtin_func(env, "if", builtin_if);

    lenv_add_builtin_func(env, "list", builtin_list);
    lenv_add_builtin_func(env, "head", builtin_head);
    lenv_add_builtin_func(env, "tail", builtin_tail);
    lenv_add_builtin_func(env, "eval", builtin_eval);
    lenv_add_builtin_func(env, "join", builtin_join);
    lenv_add_builtin_func(env, "cons", builtin_cons);
    lenv_add_builtin_func(env, "len", builtin_len);
    lenv_add_builtin_func(env, "init", builtin_init);

    lenv_add_builtin_func(env, "+", builtin_add);
    lenv_add_builtin_func(env, "-", builtin_sub);
    lenv_add_builtin_func(env, "*", builtin_mul);
    lenv_add_builtin_func(env, "/", builtin_div);
    lenv_add_builtin_func(env, "%", builtin_mod);
    lenv_add_builtin_func(env, "^", builtin_pow);
    lenv_add_builtin_func(env, "max", builtin_max);
    lenv_add_builtin_func(env, "min", builtin_min);

    // Constants!
    LENV_DEF_CONST(env, "true", 1, lval_bool);
    LENV_DEF_CONST(env, "false", 0, lval_bool);
    LENV_DEF_CONST(env, "int_max", 2147483647, lval_num);
    LENV_DEF_CONST(env, "int_min", -2147483648, lval_num);

}

lval *builtin_load(lenv *env, lval *args) {
    LASSERT_NUM(args, "load", 1);
    LASSERT_TYPE(args, "load", 0, LVAL_STR);

    mpc_result_t result;
    // Check parsing success
    if (mpc_parse_contents(args->cell[0]->str, Lispy, &result)) {
        lval *expr = lval_read(result.output);
        mpc_ast_delete(result.output);

        // Evaluate string
        while (expr->count) {
            lval *value = lval_eval(env, lval_pop(expr, 0));
            if (value->type == LVAL_ERR) {
                lval_println(value);
            }
            lval_free(value);
        }

        lval_free(expr);
        lval_free(args);
        return lval_sexpr();
    } else {
        // Get parse error as string and return lval error
        char *err_msg = mpc_err_string(result.error);
        mpc_err_delete(result.error);
        lval *err = lval_err("Could not load library\n%s", err_msg);
        free(err_msg);
        lval_free(args);
        return err;
    }
}

// Allow users to print
lval *builtin_print(lenv *env, lval *args) {
    int i;
    for (i = 0; i < args->count; i++) {
        lval_print(args->cell[i]);
        putchar(' ');
    }
    putchar('\n');
    lval_free(args);
    return lval_sexpr();
}

// Allow user to define an error message
lval *builtin_error(lenv *env, lval *args) {
    LASSERT_NUM(args, "error", 1);
    LASSERT_TYPE(args, "error", 0, LVAL_STR);

    lval *err = lval_err(args->cell[0]->str);
    lval_free(args);
    return err;
}




lval *builtin_def(lenv *env, lval *args) {
    return builtin_var(env, args, "def");
}

lval *builtin_put(lenv *env, lval *args) {
    return builtin_var(env, args, "=");
}

// Symbol definition should be done inside qexpr
// Otherwise an attempt to evaluate sexpr will yield
// an unbound symbol error.
lval *builtin_var(lenv *env, lval *args, char *func) {
    /* Defines multiple symbols to values */
    LASSERT_TYPE(args, func, 0, LVAL_QEXPR);

    // Check symbol list contains valid symbols
    lval *syms = args->cell[0];
    int i;
    for (i = 0; i < syms->count; i++) {
        LASSERT(args, syms->cell[i]->type == LVAL_SYM,
            "Function '%s' cannot define non-symbol. Expected %s instead of %s.",
            func, lval_type_name(LVAL_SYM), lval_type_name(syms->cell[i]->type));
    }

    // Check number of symbols matches number of values
    LASSERT(args, syms->count == args->count - 1,
        "Function '%s' takes incorrect number of values. Expected %d instead of %d.",
        func, syms->count, args->count - 1);

    // Assignment (global def, local put)
    for (i = 0; i < syms->count; i++) {
        if (strcmp(func, "def") == 0) {
            lenv_put(env, syms->cell[i], args->cell[i+1]);
        } else if (strcmp(func, "=") == 0) {
            lenv_put(env, syms->cell[i], args->cell[i+1]);
        } else {
            lval_free(args);
            return lval_err("Internal reference error in builtin_var. Got %s.", func);
        }
    }
    lval_free(args);
    return lval_sexpr(); // success returns ()
}

lval *builtin_lambda(lenv *env, lval *args) {
    /* First arg: formals, second arg: defn */
    LASSERT_NUM(args, "\\", 2);
    LASSERT_TYPE(args, "\\", 0, LVAL_QEXPR);
    LASSERT_TYPE(args, "\\", 1, LVAL_QEXPR);

    // Check formals only contain symbols
    int i;
    for (i = 0; i < args->cell[0]->count; i++) {
        LASSERT(args, args->cell[0]->cell[i]->type == LVAL_SYM,
            "Cannot define non-symbol. Expected %s instead of %s.",
            lval_type_name(LVAL_SYM), lval_type_name(args->cell[0]->cell[i]->type));
    }

    lval *formals = lval_pop(args, 0);
    lval *body = lval_pop(args, 0);
    lval_free(args);
    return lval_lambda(formals, body);
}


/* CONDITIONALS */

lval *builtin_or(lenv *env, lval *args) {
    return builtin_compare_bool(env, args, "or");
}

lval *builtin_and(lenv *env, lval *args) {
    return builtin_compare_bool(env, args, "and");
}

lval *builtin_compare_bool(lenv *env, lval *args, char *func) {
    // Check all arguments are booleans
    int i;
    for (i = 0; i < args->count; i++) {
        LASSERT_TYPE(args, func, i, LVAL_BOOL);
    }

    lval *result = lval_pop(args, 0);
    while (args->count) {
        lval *next = lval_pop(args, 0);
        if (strcmp(func, "or") == 0) {
            result->num = result->num || next->num;
        } else if (strcmp(func, "and") == 0) {
            result->num = result->num && next->num;
        } else {
            lval_free(args);
            lval_free(result);
            lval_free(next);
            return lval_err("Internal reference error in builtin_compare_bool. Got %s.", func);
        }
        lval_free(next);
    }
    lval_free(args);
    return result;
}

// All types
lval *builtin_eq(lenv *env, lval *args) {

    if (args->count == 0) {
        lval_free(args);
        return lval_err("Function '==' must have at least one argument.");
    }

    int eq_flag = 1;
    lval *control = lval_pop(args, 0);
    while (args->count) {
        lval *next = lval_pop(args, 0);
        if (!lval_eq(control, next)) {
            lval_free(next);
            eq_flag = 0;
            break;
        }
        lval_free(next);
    }
    lval_free(control);
    lval_free(args);
    return lval_num(eq_flag);
}

// Redefining neq so that it fits for multiple length args
// Need to do pairwise comparison for all arguments
lval *builtin_neq(lenv *env, lval *args) {

    if (args->count == 0) {
        lval_free(args);
        return lval_err("Function '!=' must have at least one argument.");
    }

    int neq_flag = 1;
    while ((args->count) && (neq_flag == 1)) {
        lval *control = lval_pop(args, 0);
        lval *remaining = lval_copy(args);
        while (remaining->count) {
            lval *next = lval_pop(remaining, 0);
            if (lval_eq(control, next)) {
                lval_free(next);
                neq_flag = 0;
                break;
            }
            lval_free(next);
        }
        lval_free(control);
        lval_free(remaining);
    }
    lval_free(args);
    return lval_num(neq_flag);
}

lval *builtin_compare_num(lenv *env, lval *args, char *func) {

    LASSERT_NUM(args, func, 2);
    LASSERT_TYPE(args, func, 0, LVAL_NUM);
    LASSERT_TYPE(args, func, 1, LVAL_NUM);

    lval *left = lval_pop(args, 0);
    lval *right = lval_extract(args, 0);
    int result = 0;
    if (strcmp(func, ">") == 0) {
        result = left->num > right->num;
    } else if (strcmp(func, ">=") == 0) {
        result = left->num >= right->num;
    } else if (strcmp(func, "<") == 0) {
        result = left->num < right->num;
    } else if (strcmp(func, "<=") == 0) {
        result = left->num <= right->num;
    } else {
        lval_free(left);
        lval_free(right);
        return lval_err("Internal reference error in builtin_compare. Got %s.", func);
    }

    lval_free(left);
    lval_free(right);
    return lval_num(result);
}

lval *builtin_greater(lenv *env, lval *args) {
    return builtin_compare_num(env, args, ">");
}

lval *builtin_greater_eq(lenv *env, lval *args) {
    return builtin_compare_num(env, args, ">=");
}

lval *builtin_lesser(lenv *env, lval *args) {
    return builtin_compare_num(env, args, "<");
}

lval *builtin_lesser_eq(lenv *env, lval *args) {
    return builtin_compare_num(env, args, "<=");
}

// Previously not working because accessing args type rather than cell type
lval *builtin_bool(lenv *env, lval *args) {
    LASSERT_NUM(args, "bool", 1);

    lval *value = lval_extract(args, 0);
    char bool_flag = lval_bool_value(value);
    lval_free(value);
    return lval_bool(bool_flag);
}

lval *builtin_negate(lenv *env, lval *args) {

    LASSERT_NUM(args, "!", 1);
    LASSERT_TYPE(args, "!", 0, LVAL_BOOL);

    lval *value = lval_extract(args, 0);
    int negation = lval_bool_value(value) ? 0 : 1;
    lval_free(value);
    return lval_num(negation);
}

// Simulate ternary operator
lval *builtin_if(lenv *env, lval *args) {

    LASSERT_NUM(args, "if", 3);
    if (args->cell[0]->type == LVAL_NUM) {
        // Convert to bool
        args->cell[0]->num = lval_bool_value(args->cell[0]);
        args->cell[0]->type = LVAL_BOOL;
    }
    LASSERT_TYPE(args, "if", 0, LVAL_BOOL);
    LASSERT_TYPE(args, "if", 1, LVAL_QEXPR);
    LASSERT_TYPE(args, "if", 2, LVAL_QEXPR);

    lval *result;
    if (args->cell[0]->num) {
        args->cell[1]->type = LVAL_SEXPR; // Allow evaluation
        result = lval_eval(env, lval_pop(args, 1));
    } else {
        args->cell[2]->type = LVAL_SEXPR;
        result = lval_eval(env, lval_pop(args, 2));
    }
    lval_free(args);
    return result;
}



/* MATHEMATICAL OPERATIONS */

lval *builtin_add(lenv *env, lval *args) {
    return builtin_op(env, args, "+");
}

lval *builtin_sub(lenv *env, lval *args) {
    return builtin_op(env, args, "-");
}

lval *builtin_mul(lenv *env, lval *args) {
    return builtin_op(env, args, "*");
}

lval *builtin_div(lenv *env, lval *args) {
    return builtin_op(env, args, "/");
}

lval *builtin_mod(lenv *env, lval *args) {
    return builtin_op(env, args, "%");
}

lval *builtin_pow(lenv *env, lval *args) {
    return builtin_op(env, args, "^");
}

lval *builtin_max(lenv *env, lval *args) {
    return builtin_op(env, args, "max");
}

lval *builtin_min(lenv *env, lval *args) {
    return builtin_op(env, args, "min");
}

lval *builtin_head(lenv *env, lval *args) {
    /* Gets only the first element */
    // Only the qexpr itself should be passed, with nonzero elements
    LASSERT_NUM(args, "head", 1);
    lval_check_get_replace(env, args->cell[0]);
    LASSERT_TYPE(args, "head", 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY(args, "head", 0);

    lval *value = lval_extract(args, 0);
    // Free all elements except head
    while (value->count > 1) { lval_free(lval_pop(value, 1)); }
    return value;
}

lval *builtin_tail(lenv *env, lval *args) {
    /* Gets all elements other than the first */
    // Only the qexpr itself should be passed, with nonzero elements
    LASSERT_NUM(args, "head", 1);
    lval_check_get_replace(env, args->cell[0]);
    LASSERT_TYPE(args, "tail", 0, LVAL_QEXPR);
    LASSERT_NOT_EMPTY(args, "tail", 0);

    lval *list = lval_extract(args, 0);
    // Free only first element
    lval_free(lval_pop(list, 0));
    return list;
}

lval *builtin_list(lenv *env, lval *args) {
    /* Converts sexpr to qexpr */
    lval_check_get_replace(env, args);
    args->type = LVAL_QEXPR;
    return args;
}

lval *builtin_eval(lenv *env, lval *args) {
    /* Converts qexpr to sexpr and evaluates it */
    LASSERT_NUM(args, "eval", 1);
    lval_check_get_replace(env, args->cell[0]);
    LASSERT_TYPE(args, "eval", 0, LVAL_QEXPR);

    lval *list = lval_extract(args, 0);
    list->type = LVAL_SEXPR;
    return lval_eval_sexpr(env, list);
}

lval *builtin_join(lenv *env, lval *args) {
    /* Concatenates multiple qexpr */
    int i;
    for (i = 0; i < args->count; i++) {
        lval_check_get_replace(env, args->cell[i]);
        LASSERT_TYPE(args, "join", i, LVAL_QEXPR);
    }

    // Individual qexpr concatenation
    lval *list = lval_pop(args, 0);
    while (args->count) {
        list = lval_join(list, lval_pop(args, 0));
    }
    lval_free(args);
    return list;
}

lval *builtin_cons(lenv *env, lval *args) {
    /* Takes lval and qexpr in args, and appends them */
    LASSERT_NUM(args, "cons", 2);
    lval_check_get_replace(env, args->cell[0]);
    lval_check_get_replace(env, args->cell[1]);
    LASSERT_TYPE(args, "cons", 1, LVAL_QEXPR);

    lval *value = lval_pop(args, 0);
    lval *list = lval_extract(args, 0);
    return lval_insert(list, value, 0);
}

lval *builtin_len(lenv *env, lval *args) {
    /* Take single qexpr in args and return length */
    LASSERT_NUM(args, "len", 1);
    lval_check_get_replace(env, args->cell[0]);
    LASSERT_TYPE(args, "len", 0, LVAL_QEXPR);

    long count = args->cell[0]->count;
    lval_free(args);
    return lval_num(count);
}

lval *builtin_init(lenv *env, lval *args) {
    /* Take single qexpr in args and remove last element */

    // Check if args is sym
    LASSERT_NUM(args, "init", 1);
    lval_check_get_replace(env, args->cell[0]);
    LASSERT_TYPE(args, "init", 0, LVAL_QEXPR);

    lval *list = lval_extract(args, 0);
    lval_free(lval_pop(list, list->count - 1));
    return list;
}

lval *builtin_op(lenv *env, lval *args, char *op) {

    // Checks all arguments are numbers
    int i;
    for (i = 0; i < args->count; i++) {
        lval_check_get_replace(env, args->cell[i]);
        LASSERT_TYPE(args, op, i, LVAL_NUM);
    }

    lval *result = lval_pop(args, 0);
    // Unary negation operator
    if ((strcmp(op, "-") == 0) && args->count == 0) {
        result->num *= -1;
    }

    while (args->count > 0) {
        lval *next = lval_pop(args, 0);

        // num can only be up to LONG_MAX
        if (strcmp(op, "+") == 0) {
            if (((result->num > 0)&&(next->num > 0)) || \
                    ((result->num < 0)&&(next->num < 0))) {
                if ((LONG_MAX - abs(result->num)) < abs(next->num)) {
                    lval_free(result); // result freed before creating err lval
                    lval_free(next); // next must be freed from same scope
                    result = lval_err("Integer overflow");
                    break;
                }
            }
            result->num += next->num;
        }
        if (strcmp(op, "-") == 0) {
            if (((result->num > 0)&&(next->num < 0)) || \
                    ((result->num < 0)&&(next->num > 0))) {
                if ((LONG_MAX - abs(result->num)) < abs(next->num)) {
                    lval_free(result);
                    lval_free(next);
                    result = lval_err("Integer overflow");
                    break;
                }
            }
            result->num -= next->num;
        }
        if (strcmp(op, "*") == 0) {
            if (next->num != 0) {
                if (abs(result->num) > (LONG_MAX/abs(next->num))) {
                    lval_free(result);
                    lval_free(next);
                    result = lval_err("Integer overflow");
                    break;
                }
            }
            result->num *= next->num;
        }
        if (strcmp(op, "/") == 0) {
            if (next->num == 0) {
                lval_free(result);
                lval_free(next);
                result = lval_err("Division by zero");
                break;
            }
            result->num /= next->num;
        }

        if (strcmp(op, "%") == 0) {
            if (next->num == 0) {
                lval_free(result);
                lval_free(next);
                result = lval_err("Division by zero");
                break;
            }
            result->num %= next->num;
        }
        if (strcmp(op, "^") == 0) {
            if (next->num < 0) {
                lval_free(result);
                result = lval_err("Negative exponent (%d) not supported", next->num);
                lval_free(next);
                break;
            }
            long exp_result = 1; // Note 0^0 is defined as 1
            if (result->num == 0) {
                if (next->num != 0) {
                    exp_result = 0; // ^ 0 1 evaluates to 0, not 1
                }
            } else {
                for (; next->num > 0; next->num--) {
                    if (abs(exp_result) > (LONG_MAX/abs(result->num))) {
                        lval_free(result);
                        lval_free(next);
                        result = lval_err("Integer overflow");
                        break;
                    }
                    exp_result *= result->num;
                }
            }
            result->num = exp_result;
        }

        if (strcmp(op, "min") == 0) {
            result->num = (result->num < next->num)
                ? result->num : next->num;
        }
        if (strcmp(op, "max") == 0) {
            result->num = (result->num > next->num)
                ? result->num : next->num;
        }
        lval_free(next);
    }
    lval_free(args);
    return result;
}

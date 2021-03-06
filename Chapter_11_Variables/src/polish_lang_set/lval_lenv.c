#include "lval_lenv.h"

// Guide has a better idea to pass enum values itself
char *lval_type_name(int enum_value) {
    switch (enum_value) {
        case LVAL_NUM: return "Number";
        case LVAL_FUNC: return "Function";
        case LVAL_ERR: return "Error";
        case LVAL_SYM: return "Symbol";
        case LVAL_SEXPR: return "S-expression";
        case LVAL_QEXPR: return "Q-expression";
    }
    return "Unknown";
}

// Num lval constructor
lval *lval_num(long result) {
    lval *value = malloc(sizeof(lval));
    value->type = LVAL_NUM;
    value->num = result;
    return value;
}

// Error lval constructor
lval *lval_err(char *format, ...) {
    lval *value = malloc(sizeof(lval));
    value->type = LVAL_ERR;

    va_list va;
    va_start(va, format);
    value->err = malloc(512);

    // copy formatted string to value->err, then resize
    vsnprintf(value->err, 511, format, va);
    value->err = realloc(value->err, strlen(value->err)+1);
    va_end(va);

    return value;
}

// Symbol lval constructor
lval *lval_sym(char *symbol_str) {
    lval *value = malloc(sizeof(lval));
    value->type = LVAL_SYM;
    value->sym = malloc(strlen(symbol_str) + 1);
    strcpy(value->sym, symbol_str);
    return value;
}

// Adjusted functions to share name in sym slot
lval *lval_func(lbuiltin func, char *name) {
    lval *value = malloc(sizeof(lval));
    value->type = LVAL_FUNC;
    value->func = func;
    value->sym = malloc(strlen(name) + 1);
    strcpy(value->sym, name);
    return value;
}

// Sexpr lval constructor
lval *lval_sexpr(void) {
    lval *value = malloc(sizeof(lval));
    value->type = LVAL_SEXPR;
    value->count = 0;
    value->cell = NULL;
    return value;
}

// Qexpr lval constructor
lval *lval_qexpr(void) {
    lval *value = malloc(sizeof(lval));
    value->type = LVAL_QEXPR;
    value->count = 0;
    value->cell = NULL;
    return value;
}

// Append to sexpr list
lval *lval_add(lval *list, lval *node) {
    list->count++;
    list->cell = realloc(list->cell, (list->count)*sizeof(lval *));
    list->cell[list->count-1] = node;
    return list;
}

// Free memory allocated to lval value based on type
void lval_free(lval *value) {
    switch (value->type) {
        case LVAL_FUNC:
        case LVAL_NUM: break;
        case LVAL_ERR: free(value->err); break;
        case LVAL_SYM: free(value->sym); break;
        case LVAL_QEXPR:
        case LVAL_SEXPR: {
            int i;
            for (i = 0; i < value->count; i++) {
                lval_free(value->cell[i]);
            }
            free(value->cell);
            break;
        }
    }
    free(value);
}

lval *lval_pop(lval *list, int index) {
    lval *result = list->cell[index];
    // Protects agianst segfault
    if (list->count != index + 1) {
        memmove(&(list->cell[index]), &(list->cell[index+1]),
            sizeof(lval *)*(list->count-index-1)); // pop value->cell[index]
    }
    list->count--;
    list->cell = realloc(list->cell, sizeof(lval *)*(list->count));
    return result;
}

// General form of lval_add
// Will this cause segfault if list->count == index?
lval *lval_insert(lval *list, lval *value, int index) {
    list->cell = realloc(list->cell, sizeof(lval *)*(list->count + 1));
    // Protects against segfault when appending
    if (list->count != index){
        memmove(&(list->cell[index+1]), &(list->cell[index]),
            sizeof(lval *)*(list->count-index));
    }
    list->count++;
    list->cell[index] = value;
    lval_free(value);
    return list;
}

lval *lval_copy(lval *value) {
    lval *copy = malloc(sizeof(lval));
    copy->type = value->type;

    switch (value->type) {
        case LVAL_FUNC: copy->func = value->func; break;
        case LVAL_NUM: copy->num = value->num; break;
        case LVAL_ERR:
            copy->err = malloc(strlen(value->err) + 1);
            strcpy(copy->err, value->err);
            break;
        case LVAL_SYM:
            copy->sym = malloc(strlen(value->sym) + 1);
            strcpy(copy->sym, value->sym);
            break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            copy->count = value->count;
            copy->cell = malloc(sizeof(lval *) * copy->count);
            int i;
            for (i = 0; i < copy->count; i++) {
                copy->cell[i] = lval_copy(value->cell[i]);
            }
            break;
    }
    return copy;
}

lval *lval_join(lval *list, lval *next) {
    /* Concatenates two qexpr */
    // Note qexpr and sexpr share same list attribute, i.e. lval_add
    while (next->count) {
        list = lval_add(list, lval_pop(next, 0));
    }

    lval_free(next);
    return list;
}

// This is not required during evaluation... why...?
// Probably because the evaluation is already done in lval_eval itself...
void lval_check_get_replace(lenv *env, lval *src) {
    if (src->type == LVAL_SYM) {
        lval_get_replace(env, src);
    }
}

void lval_get_replace(lenv *env, lval *src) {

    /* Attempt to get env var and replace dest */
    lval *value = lenv_get(env, src);
    lval_free(src);
    switch (value->type) {
        case LVAL_FUNC: src = lval_func(value->func, value->sym); break;
        case LVAL_NUM: src = lval_num(value->num); break;
        case LVAL_ERR: src = lval_err(value->err); break;
        case LVAL_SYM: src = lval_sym(value->sym); break; // or check again?
        case LVAL_SEXPR:
            src = lval_sexpr();
            src = lval_join(src, value);
            break;
        case LVAL_QEXPR:
            src = lval_qexpr();
            src = lval_join(src, value);
            break;
    }
    lval_free(value);
}

lval *lval_extract(lval *list, int index) {
    lval *result = lval_pop(list, index);
    lval_free(list);
    return result;
}

void lval_expr_print(lval *expr, char open, char close) {
    putchar(open);
    int i;
    for (i = 0; i < expr->count; i++) {
        lval_print(expr->cell[i]);
        if (i != (expr->count -1)) putchar(' ');
    }
    putchar(close);
}

void lval_print(lval *value) {
    switch (value->type) {
        case LVAL_NUM: printf("%li", value->num); break;
        case LVAL_ERR: printf("Error: %s", value->err); break;
        case LVAL_SYM: printf("%s", value->sym); break;
        case LVAL_FUNC: printf("<function>"); break;
        case LVAL_SEXPR: lval_expr_print(value, '(', ')'); break;
        case LVAL_QEXPR: lval_expr_print(value, '{', '}'); break;
    }
}

void lval_println(lval *value) {
    lval_print(value);
    putchar('\n');
}



// ast == Abstract Syntax Tree
// r.output is a mpc_val_t*,
// so why is passing argument as a mpc_ast_t* allowed?
// If it is type promotion, how is it implemented?
lval *lval_read(mpc_ast_t *node) {

    // All nodes tagged with number / symbol is definitely a primitive number
    if (strstr(node->tag, "number")) {
        errno = 0; // reset value to 0, <errno.h> included in error_handler
        long value = strtol(node->contents, NULL, 10); // robust ver of atoi
        return (errno != ERANGE) ? lval_num(value) : lval_err("Invalid number");
    }
    if (strstr(node->tag, "symbol")) {
        return lval_sym(node->contents);
    }

    // Create empty sexpr list if root (>) or sexpr
    lval *list = NULL;
    if (strstr(node->tag, "sexpr")||(strcmp(node->tag, ">") == 0)) {
        list = lval_sexpr();
    }
    // Create empty qexpr list if qexpr
    if (strstr(node->tag, "qexpr")) {
        list = lval_qexpr();
    }

    // Ignore invalid expressions (parentheses, regex start, end)
    int i;
    for (i = 0; i < node->children_num; i++) {
        if (strcmp(node->children[i]->contents, "(") == 0) continue;
        if (strcmp(node->children[i]->contents, ")") == 0) continue;
        if (strcmp(node->children[i]->contents, "{") == 0) continue;
        if (strcmp(node->children[i]->contents, "}") == 0) continue;
        if (strcmp(node->children[i]->tag, "regex") == 0) continue;
        list = lval_add(list, lval_read(node->children[i]));
    }
    return list;
}

// Evaluation
lval *lval_eval(lenv *env, lval *value) {
    if (value->type == LVAL_SYM) {
        lval *result = lenv_get(env, value);
        lval_free(value);
        if ((result->type == LVAL_FUNC) && (strcmp(value->sym, "dir") == 0)) {
            lenv_print_dir(env);
            return lval_sexpr();
        }
        return result;
    }
    if (value->type == LVAL_SEXPR) {
        return lval_eval_sexpr(env, value);
    }
    return value;
}

lval *lval_eval_sexpr(lenv *env, lval *value) {

    // Evaluate children, rethrowing errors if any
    int i;
    for (i = 0; i < value->count; i++) {
        value->cell[i] = lval_eval(env, value->cell[i]);
        // Check if evaluation error
        if (value->cell[i]->type == LVAL_ERR) { return lval_extract(value, i); }
    }

    // Empty expressions return self, single expr extract value
    // Exception for dir!
    if (value->count == 0) { return value; }
    if (value->count == 1) { return lval_extract(value, 0); }

    // Multiple element sexpr
    // Check if first element is symbol
    lval *first = lval_pop(value, 0);
    if (first->type != LVAL_FUNC) {
        lval *error = lval_err("'%s' is not a function", lval_type_name(first->type));
        lval_free(first);
        lval_free(value);
        return error;
    }

    // Call operator on rest of elements
    lval *result = first->func(env, value);
    lval_free(first);
    return result;
}


// lenv constructors and methods
lenv *lenv_new(void) {
    lenv *env = malloc(sizeof(lenv));
    env->count = 0;
    env->syms = NULL;
    env->vals = NULL;
    return env;
}

void lenv_free(lenv *env) {
    int i;
    for (i = 0; i < env->count; i++) {
        free(env->syms[i]);
        lval_free(env->vals[i]);
    }
    free(env->syms);
    free(env->vals);
    free(env);
}

// Get variable from environment
lval *lenv_get(lenv *env, lval *key) {
    int i;
    for (i = 0; i < env->count; i++) {
        if (strcmp(env->syms[i], key->sym) == 0) {
            return lval_copy(env->vals[i]);
        }
    }
    return lval_err("Unbound symbol '%s'", key->sym);
}

void lenv_put(lenv *env, lval *key, lval *value) {
    int i;
    for (i = 0; i < env->count; i++) {
        if (strcmp(env->syms[i], key->sym) == 0) {
            lval_free(env->vals[i]); // Replaces variable name
            env->vals[i] = lval_copy(value);
            return;
        }
    }

    // No existing entry found
    env->count++;
    env->vals = realloc(env->vals, sizeof(lval *) * env->count);
    env->vals[env->count-1] = lval_copy(value);

    env->syms = realloc(env->syms, sizeof(char *) * env->count);
    env->syms[env->count-1] = malloc(strlen(key->sym) + 1);
    strcpy(env->syms[env->count-1], key->sym);
}

void lenv_print_dir(lenv *env) {
    /* Print names of all bound variables */
    int i;
    for (i = 0; i < env->count; i++) {
        printf(" %2d: %s\n", i, env->syms[i]);
    }
}

void lenv_add_builtin(lenv *env, char *name, lbuiltin func) {
    lval *key = lval_sym(name);
    lval *value = lval_func(func, name);
    lenv_put(env, key, value);
    lval_free(key);
    lval_free(value);
}

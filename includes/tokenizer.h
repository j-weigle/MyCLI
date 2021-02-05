#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <stdbool.h>

typedef struct tok_node {
    char *token;
    bool special;
    struct tok_node *next;
} tok_node;

typedef struct {
    void *head;
    void *tail;
    int count;
}ListHandler;

void tokenize (ListHandler *, char *);

void free_tok_list (ListHandler *);

void print_tokens (tok_node *);

#endif

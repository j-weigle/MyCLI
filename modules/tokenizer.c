/************************************************
 *                tokenizer.c                   *
 ************************************************
 * tokenize takes a string as input and then    *
 * separates the string into tokens that can    *
 * further be used to execute shell commands    *
 ************************************************
 * Author: Justin Weigle                        *
 * Edited: 30 Jul 2020                          *
 ************************************************/

#include "../includes/tokenizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

typedef enum {
    Init_State,
    Blank_State,
    Letter_State,
    Redirect_State,
    Single_Quote_State,
    Double_Quote_State,
} Token_Sys_State;

static void save_string (char*, ListHandler**, bool);

/**
 * Uses state machine to tokenize a user's input into appropriate
 * tokens for processing as shell commands
 */
void tokenize (ListHandler *tlist, char *input)
{
    int length = strlen(input);
    if (input[length-1] != '\n') {
        fprintf(stderr, "Input didn't end in \\n, skipping...\n");
        return;
    }

    char token[length];
    char ch;
    Token_Sys_State State = Init_State;

    for(int i = 0, j = 0; i < length; i++) {
        ch = input[i];
        switch (State) {
            /* Should only change for quotes or letters.
             * Save ch if letter */
            case Init_State:
                if (ch == '\n') {
                    return;
                } else if (ch == '"') {
                    State = Double_Quote_State;
                } else if (ch == '\'') {
                    State = Single_Quote_State;
                } else if (ch == '<' || ch == '>' || ch == '|') {
                    fprintf(stderr, "Need input before redirect or pipe\n");
                    return;
                } else if (ch == ' ') {
                } else if (32 <= ch && ch <= 127) {
                    State = Letter_State;
                    token[j] = ch;
                    j++;
                } else {
                    fprintf(stderr, "Unrecognized character %c\n", ch);
                    return;
                }
                break;
            /* Change for everything except letters.
             * Save ch for letters only.
             * Save string for redirect and newline */
            case Letter_State:
                if (ch == '\n') {
                    token[j] = '\0';
                    save_string(token, &tlist, false);
                } else if (ch == '"') {
                    State = Double_Quote_State;
                } else if (ch == '\'') {
                    State = Single_Quote_State;
                } else if (ch == '<' || ch == '>' || ch == '|') {
                    State = Redirect_State;
                    token[j] = '\0';
                    save_string(token, &tlist, false);
                    token[0] = ch;
                    j = 1;
                } else if (ch == ' ') {
                    State = Blank_State;
                    token[j] = '\0';
                    save_string(token, &tlist, false);
                    j = 0;
                } else if (32 <= ch && ch <= 127) {
                    token[j] = ch;
                    j++;
                } else {
                    fprintf(stderr, "Unrecognized character %c\n", ch);
                    free_tok_list(tlist);
                    return;
                }
                break;
            /* Change for everything except blanks.
             * Save ch for letters and redirect */
            case Blank_State:
                if (ch == '\n') {
                } else if (ch == '"') {
                    State = Double_Quote_State;
                } else if (ch == '\'') {
                    State = Single_Quote_State;
                } else if (ch == '<' || ch == '>' || ch == '|') {
                    State = Redirect_State;
                    token[j] = ch;
                    j++;
                } else if (ch == ' ') {
                } else if (32 <= ch && ch <= 127) {
                    State = Letter_State;
                    token[j] = ch;
                    j++;
                } else {
                    fprintf(stderr, "Unrecognized character %c\n", ch);
                    free_tok_list(tlist);
                    return;
                }
                break;
            /* save ch for another > if >.
             * error on any other redirect or newline
             * save string and change for anything else
             * */
            case Redirect_State:
                if (ch == '"') {
                    State = Double_Quote_State;
                    token[j] = '\0';
                    save_string(token, &tlist, true);
                    j = 0;
                } else if (ch == '\'') {
                    State = Single_Quote_State;
                    token[j] = '\0';
                    save_string(token, &tlist, true);
                    j = 0;
                } else if (ch == '\n') {
                    fprintf(stderr, "Can't have redirect at end of input\n");
                    free_tok_list(tlist);;
                    return;
                } else if (ch == '<' || ch == '|') {
                    fprintf(stderr, "%c not valid after >\n", ch);
                    free_tok_list(tlist);;
                    return;
                } else if (ch == '>') {
                    if (input[i-1] == '>') {
                        if (input[i+1] == '>') {
                            fprintf(stderr, "Too many redirects in a row\n");
                            free_tok_list(tlist);;
                            return;
                        }
                        token[j] = ch;
                        j++;
                    } else {
                        fprintf(stderr,
                            "Cannot have spaces between >\n");
                        free_tok_list(tlist);;
                        return;
                    }
                } else if (ch == ' ') {
                } else if (32 <= ch && ch <= 127) {
                    State = Letter_State;
                    token[j] = '\0';
                    save_string(token, &tlist, true);
                    token[0] = ch;
                    j = 1;
                } else {
                    fprintf(stderr, "Unrecognized character %c\n", ch);
                    free_tok_list(tlist);;
                    return;
                }
                break;
            /** everything is treated as plaintext until
             *  the next single quote other than escaped chars.
             *  After end quote found, check next char to
             *  decide which state to change to             */
            case Single_Quote_State:
                if (ch == '\'') {
                    i++; // check next character
                    ch = input[i];
                    if (ch == '"') {
                        State = Double_Quote_State;
                    } else if (ch == '\'') {
                    } else if (ch == '<' || ch == '>' || ch == '|') {
                        State = Redirect_State;
                        token[j] = '\0';
                        save_string(token, &tlist, false);
                        token[0] = ch;
                        j = 1;
                    } else if (ch == ' ') {
                        State = Blank_State;
                        token[j] = '\0';
                        save_string(token, &tlist, false);
                        j = 0;
                    } else if (32 <= ch && ch <= 127) {
                        State = Letter_State;
                        token[j] = ch;
                        j++;
                    } else if (ch == '\n') {
                        token[j] = '\0';
                        save_string(token, &tlist, false);
                    } else {
                        fprintf(stderr, "Unrecognized character %c\n", ch);
                        free_tok_list(tlist);;
                        return;
                    }
                } else if (ch == '\n') {
                    fprintf(stderr, "Quote never closed \'\n");
                    free_tok_list(tlist);;
                    return;
                } else if (ch == '\\') { // handle escaped characters
                    i++;
                    char ec = input[i];
                    if (ec == 'n') {
                        token[j++] = '\n';
                    } else if (ec == 'b') {
                        token[j++] = '\b';
                    } else if (ec == 'r') {
                        token[j++] = '\r';
                    } else if (ec == 't') {
                        token[j++] = '\t';
                    } else if (ec == 'v') {
                        token[j++] = '\v';
                    } else if (ec == '0') {
                        token[j++] = '\0';
                    } else {
                        token[j++] = input[i-1];
                        token[j++] = ec;
                    }
                } else {
                    token[j] = ch;
                    j++;
                }
                break;
            /** see single quote explanation    */
            case Double_Quote_State:
                if (ch == '"') {
                    i++; // check next character
                    ch = input[i];
                    if (ch == '\'') {
                        State = Single_Quote_State;
                    } else if (ch == '"') {
                    } else if (ch == '<' || ch == '>' || ch == '|') {
                        State = Redirect_State;
                        token[j] = '\0';
                        save_string(token, &tlist, false);
                        token[0] = ch;
                        j = 1;
                    } else if (ch == ' ') {
                        State = Blank_State;
                        token[j] = '\0';
                        save_string(token, &tlist, false);
                        j = 0;
                    } else if (32 <= ch && ch <= 127) {
                        State = Letter_State;
                        token[j] = ch;
                        j++;
                    } else if (ch == '\n') {
                        token[j] = '\0';
                        save_string(token, &tlist, false);
                    } else {
                        fprintf(stderr, "Unrecognized character %c\n", ch);
                        free_tok_list(tlist);;
                        return;
                    }
                } else if (ch == '\n') {
                    fprintf(stderr, "Quote never closed \"\n");
                    free_tok_list(tlist);;
                    return;
                } else if (ch == '\\') { // handle escaped characters
                    i++;
                    char ec = input[i];
                    if (ec == 'n') {
                        token[j++] = '\n';
                    } else if (ec == 'b') {
                        token[j++] = '\b';
                    } else if (ec == 'r') {
                        token[j++] = '\r';
                    } else if (ec == 't') {
                        token[j++] = '\t';
                    } else if (ec == 'v') {
                        token[j++] = '\v';
                    } else if (ec == '0') {
                        token[j++] = '\0';
                    } else {
                        token[j++] = input[i-1];
                        token[j++] = ec;
                    }
                } else {
                    token[j] = ch;
                    j++;
                }
                break;
            default:
                break;
        }
    }
    return;
}

/**
 * Inserts a new node at the end of the linked list pointed to by head
 * with the given string token and marks whether it is a special
 * token or not based on spec */
static void save_string (char *token, ListHandler **tlist, bool spec)
{
    tok_node *t_node = malloc(sizeof(tok_node)); // make new node
    int length = strlen(token);
    if (t_node == NULL) {
        perror("malloc failed in save_string");
        exit(-1);
    }
    t_node->token = malloc(sizeof(char)*length+1); // make space for token
    if (t_node->token == NULL) {
        perror("malloc failed in save_string");
        exit(-1);
    }
    strncpy(t_node->token, token, length+1); // put token in node
    t_node->token[length] = '\0'; // in case strncpy doesn't null terminate
    t_node->special = spec; // set if token is special
    t_node->next = NULL; // set next to NULL, node is going at the end

    if ((*tlist)->head == NULL) { // if head is NULL, new node is head
        (*tlist)->head = t_node;
        (*tlist)->tail = t_node;
        (*tlist)->count = 1;
    } else { // otherwise insert node at the end
        ((tok_node *)(*tlist)->tail)->next = t_node;
        (*tlist)->tail = t_node;
        (*tlist)->count++;
    }
    return;
}

/**
 * call to free all nodes
 */
void free_tok_list (ListHandler *tlist)
{
    tok_node *temp = tlist->head;
    while (temp != NULL) {
        tlist->head = ((tok_node *)tlist->head)->next;
        free(temp->token);
        free(temp);
        temp = tlist->head;
    }
    tlist->head = NULL;
    tlist->tail = NULL;
    tlist->count = 0;
    return;
}

/**
 * prints all the tokens in the list and whether
 * they are special or not
 */
void print_tokens (tok_node *head)
{
    tok_node *list = head;
    while (list != NULL) {
        printf("%d:%s\n", list->special, list->token);
        list = list->next;
    }
}

#include "utils/trie.h"

#include <string.h>

/** Initializes a Trie tree. */
void trie_init(struct trie_tree* tree, struct trie_node* root)
{
    tree->root = root;
};
/** Initializes a Trie node. */
void trie_node_init(struct trie_node* node, char* key, void* value)
{
    node->key = key;
    node->value = value;
    node->children = NULL;
    node->children_count = 0;
};
/** Inserts a Trie node into a Trie tree. */
void trie_insert(struct trie_tree* tree, struct trie_node* node)
{
    if (tree->root == NULL)
    {
        tree->root = node;
        return;
    };

    struct trie_node* current = tree->root;
    while (1)
    {
        if (current->children == NULL)
        {
            current->children = malloc(sizeof(struct trie_node));
            trie_node_init(current->children, node->key, node->value);
            ++current->children_count;
            return;
        };

        for (size_t i = 0; i < current->children_count; ++i)
        {
            struct trie_node* child = &current->children[i];
            if (strcmp(child->key, node->key) == 0)
            {
                current = child;
                break;
            }
            else if (i == current->children_count - 1)
            {
                current->children = realloc(current->children, sizeof(struct trie_node) * (current->children_count + 1));
                trie_node_init(&current->children[current->children_count], node->key, node->value);
                ++current->children_count;
                return;
            };
        };
    };
};
/** Searches for a Trie node in a Trie tree. */
struct trie_node* trie_search(struct trie_tree* tree, char* key)
{
    if (tree->root == NULL) return NULL;

    struct trie_node* current = tree->root;
    while (1)
    {
        if (current->children == NULL) return NULL;

        for (size_t i = 0; i < current->children_count; ++i)
        {
            struct trie_node* child = &current->children[i];
            if (strcmp(child->key, key) == 0) return child;
            else if (i == current->children_count - 1) return NULL;
        };
    };
};
/** Frees a Trie tree. */
void trie_free(struct trie_tree* tree)
{
    if (tree->root == NULL) return;

    struct trie_node* current = tree->root;
    while (1)
    {
        if (current->children == NULL)
        {
            free(current);
            return;
        };

        for (size_t i = 0; i < current->children_count; ++i)
        {
            struct trie_node* child = &current->children[i];
            trie_free(child);
        };

        free(current->children);
        free(current);
    };
}
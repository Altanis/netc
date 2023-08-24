#ifndef TRIE_H
#define TRIE_H

#include <stddef.h>

/** Used for parameterized routing for HTTP. */

/** A struct representing a Trie tree. */
struct trie_tree
{
    struct trie_node* root;
};

/** A struct representing a Trie node. */
struct trie_node 
{
    char* key; // The key of the node.
    void* value; // The value of the node.
    struct trie_node* children; // The children of the node.
    size_t children_count; // The number of children of the node.
};

/** Initializes a Trie tree. */
void trie_init(struct trie_tree* tree, struct trie_node* root);
/** Initializes a Trie node. */
void trie_node_init(struct trie_node* node, char* key, void* value);
/** Inserts a Trie node into a Trie tree. */
void trie_insert(struct trie_tree* tree, struct trie_node* node);
/** Searches for a Trie node in a Trie tree. */
struct trie_node* trie_search(struct trie_tree* tree, char* key);
/** Frees a Trie tree. */
void trie_free(struct trie_tree* tree);

#endif // TRIE_H
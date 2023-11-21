#ifndef TREE_H
#define TREE_H

#include <stdint.h>

#define DEBUG 1
#define STATUS_PLAYED 0
#define STATUS_CURRENT 2
#define STATUS_PREDICTED 4
#define STATUS_ROOT 8

#define DEEP_SEARCH_WIDTH 3

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

struct Node {
    int64_t move; // the move played
    char status; // 0 means already played, 2 means the last played move, 4 means a predicted, 8 means root
    char color; //Who played the Move
    char board[8][8]; //The board state after the move
    int rating; //The rating of the move
    struct Node *parent; // Parent node
    struct Node **children; // Children nodes
    int childrenCount; //Children count
};

void initializeTree(void);
struct Node* getTreeRoot(void);
struct Node* addTreeNode(struct Node* parent, int64_t move, char status);
void updateNodeStatus(struct Node* node, char status);
struct Node* iterateTree(struct Node* cur, int64_t move);
int pruneNode(struct Node* it, struct Node* nextIt);
void pruneNodeExceptFor(struct Node* node, struct Node* exceptNode);
void pruneAbove(struct Node* current);
struct Node* getBestCurChild();
void buildTreeMoves(int depth);

void deepSearchTree(int starting_depth, int depth);

#ifdef DEBUG
void printNode(struct Node* node, int level, int depth);
void printTree(void);
void printCurNode(void);
#endif

#endif // TREE_H


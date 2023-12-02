#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "util.h"
#include "mempool.h"
#include "tree.h"
#include "board.h"
#include "tests/bbtests.h"
#include "bitboard/bitboard.h"

#define DEPTH_SCAN 4
#define DEPTH_DEEP_START 2
#define DEPTH_DEEP 6

#define PLAY_SELF

#ifdef PLAY_SELF
static void playSelf(void);
#endif


static void processUCI(void);
static void processIsReady(void);
static int processInput(char* input);
static void processMoves(char* str);
static void sendBestMove(void);
static char isNullMove(char* move);

int main(void) {
    generateMasks();
    testBB();
    
    char input[1024];
    input[1023] = '\0';
    
    initializeNodePool();
    initializeTree();

    while (true) {
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; 
        }
        if(processInput(input)){
            break;
        }
    }

    return 0;
}


static int processInput(char* input){
    if (strncmp(input, "uci", 3) == 0) {
        processUCI();
        return 0;
    } 
    else if (strncmp(input, "isready", 7) == 0) {
        processIsReady();
        return 0;
    }
    else if (strncmp(input, "position", 8) == 0) {
        input += 9;
        if (strncmp(input, "startpos", 8) == 0) {
            input += 9;
            if (strncmp(input, "moves", 5) == 0) {
                input += 6;
                processMoves(input);
                return 0;
            }
        }
        else if (strncmp(input, "fen", 3) == 0) {
            input += 4;
            initializeTreeFEN(input);
            return 0;
        }
    }
    else if (strncmp(input, "quit", 4) == 0) {
        return -1; 
    }
    return 0;
}

/*
* Processes a list of moves such as e2e3 a3a4 b2b3
* makes sure these nodes are in the tree and marks them as played
* also will prune the branches of all already played moves
* except for the children of the last node
*/
void processMoves(char* str) {
    size_t it = getTreeRoot();

    char* pch;
    char* rest = str; 
    pch = strtok_r(str, " ", &rest);
    while (pch != NULL) {
        char* moveChar = trimWhitespace(pch);
        if(isNullMove(moveChar)){
            goto get_next_token;
        }
        struct Move move;
        moveStrToStruct(moveChar, &move); 
        size_t nextIt = iterateTree(it, move);
        if (nextIt == NULL_NODE) {
            it = addTreeNode(it, move, 0);
        } else {
            updateNodeStatus(it, 0);
            it = nextIt;
        }
get_next_token:
        pch = strtok_r(NULL, " ", &rest);
    }

    updateNodeStatus(it, 2);
    buildTreeMoves(DEPTH_SCAN);

    printCurNode();
    deepSearchTree(DEPTH_DEEP_START, DEPTH_DEEP);

    sendBestMove();

    #ifdef PLAY_SELF
    playSelf();
    #endif

}

static char isNullMove(char* move){
    if(move == NULL || strlen(move) < 4) return 0;
    for(int i = 0; i < 4; i++){
        if(move[i] != '0') return 0;
    }
    return 1;
}


static void processUCI(void) {
    printf("id name CraigEngine\r\n");
    printf("id author John\r\n");
    printf("uciok\r\n");
    fflush(stdout);
}

static void processIsReady(void) {
    printf("readyok\r\n");
    fflush(stdout);
}


 static void sendBestMove(void){
    size_t node = getBestCurChild();
    if(node == NULL_NODE){
        printf("info string Warning failed to build nodes, this probably means you lost\r\n");
        return;
    }
    char move[6];
    moveStructToStr(&(getNode(node)->move), move);
    printf("bestmove %s\r\n", move);
    printf("info string Move %s found for %c with a board score of %d\n\r", move, getNode(node)->color, getNode(node)->rating);
    fflush(stdout);
 }

#ifdef PLAY_SELF

static void playSelf(void) {
    size_t new = getBestCurChild();
    if(new == NULL_NODE){
        printf("Checkmate athiests!\r\n");
        return;
    }
    updateNodeStatus(getNode(new)->parent, 0);
    updateNodeStatus(new, 2);
    buildTreeMoves(DEPTH_SCAN);
    deepSearchTree(DEPTH_DEEP_START, DEPTH_DEEP);
    printCurNode();
    playSelf();
}
#endif

 
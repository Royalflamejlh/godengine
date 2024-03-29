//
//  bbtests.c
//  godengine
//
//  Created by John Howard on 11/23/23.
//

#include "bbtests.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../bitboard/bitboard.h"
#include "../bitboard/magic.h"
#include "../tree.h"
#include "../movement.h"
#include "../util.h"
#include "../hash.h"
#include "../transposition.h"
#include "../evaluator.h"

#define MOVE_GEN_TEST
#define MOVE_MAKE_TEST
#define PERF_TEST

int testBB(void) {
    #ifdef PYTHON
    python_init();
    #endif
    
    

    


    #ifdef MOVE_GEN_TEST
    FILE *file;
    char line[1024];
    Position pos;
    Move moveList[MAX_MOVES];
    int size, expectedMoves;
    
    printf("\n---------------------------------- MOVE GEN TESTING ----------------------------------\n\n");
    

    file = fopen("perftsuite.epd", "r");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        char *fenEnd = strchr(line, ';');
        if (fenEnd) *fenEnd = '\0';
        char *fen = line;
        pos = fenToPosition(fen);
        if (fenEnd) *fenEnd = ';'; 

        char *token;
        while ((token = strtok(fenEnd ? fenEnd + 1 : NULL, " ;")) != NULL) {
            if (token[0] == 'D' && token[1] == '1') {
                expectedMoves = atoi(token + 3);
                size = generateLegalMoves(pos, moveList);
                if (size != expectedMoves) {
                    printf("Failed to get correct amount of moves for Position %s, correct: %d, found: %d\n", fen, expectedMoves, size);
                    printPosition(pos, TRUE);
                    for (int i = 0; i < size; i++) {
                        printMove(moveList[i]);
                    }
                    return -1;
                }
                break;
            }
        }

        removeHashStack(&pos.hashStack);
    }

    fclose(file);

    printf("Finished Depth 1 Position Check \n");
    #endif

    #ifdef MOVE_GEN_TEST
    printf("\n---------------------------------- MOVE MAKING TESTING ----------------------------------\n\n");
    time_t t;
    srand((unsigned) time(&t));

    
    printf("Starting Quick Check\n");
    Move threatMoveList[MAX_MOVES];
    int threatSize;
    for(int j = 0; j < 1000; j++){
        char* FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        pos = fenToPosition(FEN);
        size = generateLegalMoves(pos, moveList);
        int i = 0;
        while(size != 0 && i < 1000){
            int randMove = rand() % size;
            makeMove(&pos, moveList[randMove]);
            size = generateLegalMoves(pos, moveList);
            threatSize = generateThreatMoves(pos, threatMoveList);
            for(int j = 0; j < threatSize; j++){
                char found = 0;
                for(int k = 0; k < size; k++){
                    if(threatMoveList[j] == moveList[k]) found = 1;
                }
                if(!found){
                    printf("Move in threat moves, that is not in move list!\n");
                    return -1;
                }
            }
            i++;
        }
        removeHashStack(&pos.hashStack);
    }
    printf("Check Complete.\n");

    #endif


    #ifdef PERF_TEST
    printf("\n---------------------------------- PERFT TESTING ----------------------------------\n\n");

    printf("Perft from default position:\n");
    pos = fenToPosition(START_FEN);
    for(int depth = 1; depth < 4; depth++){
        uint64_t num_moves = perft(depth, pos);
        printf("Perft output is %lld for depth %d\n", (long long unsigned)num_moves, depth);
    }
    removeHashStack(&pos.hashStack);

    printf("\nComplete, running perft suite.\n");

    file = fopen("perftsuite.epd", "r");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    while (fgets(line, sizeof(line), file)) {
        char *fen = line;
        pos = fenToPosition(fen);
        //printf("Testing: %s", fen);
        for(int depth = 1; depth < 2; depth++){
            perft(depth, pos);
            //int64_t num_moves = perft(depth, pos);
            //printf("D%d: %lld |", depth, (long long int)num_moves);
        }
        removeHashStack(&pos.hashStack);
        //printf("\n\n");
    }
    printf("\nPerft Suite Complete\n");

    fclose(file);

    printf("\n-----------------------------------------------------------------------------------\n\n");
    #endif


    #ifdef NODE_TEST
    printf("\n---------------------------------- Node TESTING ----------------------------------\n\n");

    pos = fenToPosition(START_FEN);
    printPosition(pos, FALSE);

    Move moveListNode[MAX_MOVES];
    int sizeNode = 0;
    int moveVals[MAX_MOVES] = {0};
    sizeNode = generateLegalMoves(pos, moveListNode);
    evalMoves(moveListNode, moveVals, sizeNode, NO_MOVE, NULL, 0, pos);
    for(int i = 0; i < sizeNode; i++){
        printf("Move found with move value of %d:\n", moveVals[i]);
        printMove(moveListNode[i]);
    }
    printf("\n---------------------------testing select sort----------------------------\n");
    for (int i = 0; i < sizeNode; i++)  {
        selectSort(i, moveListNode, moveVals, sizeNode);
        printf("Move with value %d selected at pos %d\n", moveVals[i], i);
        printMove(moveListNode[i]);
    }
    printf("\n--------------------------------------------------------------------------\n");

    removeHashStack(&pos.hashStack);
    
    pos = fenToPosition(START_FEN);
    Move best_move = getBestMove(pos);
    while(best_move != NO_MOVE){
        printf("Best move found to be: \n");
        printMove(best_move);
        printf("Press Enter to Continue\n");
        while( getchar() != '\n' && getchar() != '\r');
        makeMove(&pos, best_move);
        printf("Pos after move: \n");
        printPosition(pos, FALSE);
        best_move = getBestMove(pos);
    }

    removeHashStack(&pos.hashStack);

    printf("\n-----------------------------------------------------------------------------------\n\n");
    #endif

    #ifdef HASH_TEST
    printf("\n---------------------------------- HASH TESTING ----------------------------------\n\n");

    pos = fenToPosition(START_FEN);
    printf("Hash %d is: %" PRIu64 "\n", 0, pos.hash);
    Move moveList_hash[MAX_MOVES];
    int size_hash = 0;
    int moveVals[MAX_MOVES] = {0};
    

    for (int i = 0; i < 100; i++)  {
        size_hash = generateLegalMoves(pos, moveList_hash);
        evalMoves(moveList_hash, moveVals, size_hash, NO_MOVE, NULL, 0, pos);
        selectSort(0, moveList_hash, moveVals, size_hash);
        makeMove(&pos, moveList_hash[0]);
        printf("Hash %d is: %" PRIu64 "\n", i+1, pos.hash);
    }

    printf("Now going through Hash Table: (Size : %d) \n", pos.hashStack.current_idx);
    for(int i = 0; i < pos.hashStack.current_idx; i++){
        printf("HashTable[%d] : %" PRIu64 "\n", i, pos.hashStack.ptr[i]);
    }

    printf("Printing Hash Table Since Last Unique Move (at: %d): \n", pos.hashStack.last_reset_idx);
    for(int i = pos.hashStack.last_reset_idx; i < pos.hashStack.current_idx; i++){
        printf("HashTable[%d] : %" PRIu64 "\n", i, pos.hashStack.ptr[i]);
    }

    removeHashStack(&pos.hashStack);
    #endif

    #ifdef PYTHON
    python_close();
    #endif

    return 0;
}

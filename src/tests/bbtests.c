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
#include <inttypes.h>
#include <time.h>
#include "../bitboard/bitboard.h"
#include "../bitboard/magic.h"
#include "../movement.h"
#include "../util.h"
#include "../tree.h"

#define MOVE_GEN_TEST
#define MOVE_MAKE_TEST
#define PERF_TEST
#define NODE_TEST

#define START_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

int testBB(void) {
    python_init();
    generateMasks();
    generateMagics();

    


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
                    printPosition(pos);
                    for (int i = 0; i < size; i++) {
                        printMove(moveList[i]);
                    }
                    return -1;
                }
                break;
            }
        }
    }

    fclose(file);

    printf("Finished Depth 1 Position Check \n");
    #endif

    #ifdef MOVE_GEN_TEST
    printf("\n---------------------------------- MOVE MAKING TESTING ----------------------------------\n\n");
    time_t t;
    srand((unsigned) time(&t));

    
    printf("Starting Quick Check\n");
    for(int j = 0; j < 1000; j++){
        char* FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
        pos = fenToPosition(FEN);
        size = generateLegalMoves(pos, moveList);
        int i = 0;
        while(size != 0 && i < 1000){
            int randMove = rand() % size;
            makeMove(&pos, moveList[randMove]);
            size = generateLegalMoves(pos, moveList);
            i++;
        }
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
        for(int depth = 1; depth < 3; depth++){
            perft(depth, pos);
            //uint64_t num_moves = perft(depth, pos);
            //printf("D%d: %ld |", depth, num_moves);
        }
        //printf("\n\n");
    }
    printf("\nPerft Suite Complete\n");

    fclose(file);

    printf("\n-----------------------------------------------------------------------------------\n\n");
    #endif


    #ifdef NODE_TEST
    printf("\n---------------------------------- Node TESTING ----------------------------------\n\n");

    pos = fenToPosition(START_FEN);
    printPosition(pos);
    Move best_move = getBestMove(pos);
    while(best_move != 0){
        printf("Best move found to be: \n");
        printMove(best_move);
        printf("Press Enter to Continue\n");
        while( getchar() != '\n' && getchar() != '\r');
        makeMove(&pos, best_move);
        printf("Pos after move: \n");
        printPosition(pos);
        best_move = getBestMove(pos);
    }

    printf("\n-----------------------------------------------------------------------------------\n\n");
    #endif
    
    python_close();
    return 0;
}

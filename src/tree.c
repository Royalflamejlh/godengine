#include "tree.h"
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "movement.h"
#include "util.h"
#include "evaluator.h"
#include "transposition.h"

#define DEBUG

#define DEPTH 8
#define ID_STEP 1


#define KMV_CNT 4 //How many killer moves are stored for a pos

#define MAX_QUIESCE_PLY 4 //How far q search can go
#define MAX_PLY 255 //How far the total search can go



//static void selectSort(int i, Move *moveList, int *moveVals, int size);


#ifdef DEBUG
#include <time.h>
static int64_t pvs_count;
static int64_t zws_count;
static int64_t q_count;
static struct timespec start_time, end_time;

void startTreeDebug(void){
   pvs_count = 0;
   zws_count = 0;
   q_count = 0;
   clock_gettime(CLOCK_MONOTONIC, &start_time);
}
void printTreeDebug(void){
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elap_time = (end_time.tv_sec - start_time.tv_sec) +
                       (end_time.tv_nsec - start_time.tv_nsec) / 1e9;

   uint64_t total_count = pvs_count + zws_count + q_count;

   printf("Tree searched %lld evals (pvs: %lld, zws: %lld, q: %lld)\n", total_count, pvs_count, zws_count, q_count);
   printf("Took %lf seconds, with %lf eval/sec\n", elap_time, ( (double)total_count) / elap_time);
}
#endif


/*
* Function pertaining to storage of killer moves 
*/
static Move killerMoves[MAX_PLY][KMV_CNT] = {0};
static unsigned int kmvIdx = 0;

static inline void storeKillerMove(int ply, Move move){
   if(killerMoves[ply][kmvIdx] != move){
      killerMoves[ply][kmvIdx] = move;
      kmvIdx = (kmvIdx + 1) % KMV_CNT;
   }
}

static inline void clearKillerMoves(void){
   memset(killerMoves, 0, sizeof(killerMoves));
}

/*
* Functions pertaining to storage in the history table
*/
static uint32_t historyTable[PLAYER_COUNT][BOARD_SIZE][BOARD_SIZE] = {0};

static inline void storeHistoryMove(char pos_flags, Move move, char depth){
   if(GET_FLAGS(move) & CAPTURE) return;
   historyTable[pos_flags & WHITE_TURN][GET_FROM(move)][GET_TO(move)] += 1 << depth;
}

int getHistoryScore(char pos_flags, Move move){
   if(GET_FLAGS(move) & CAPTURE) return 0;
   return historyTable[pos_flags & WHITE_TURN][GET_FROM(move)][GET_TO(move)];
}

/*
* Get that move son.
*/
Move getBestMove(Position pos){
   Move bestMove;
   int moveScore;
   
   #ifdef DEBUG
   startTTDebug();
   startTreeDebug();
   #endif

   clearKillerMoves();
   for(int i = 1; i <= DEPTH; i+=ID_STEP){
      moveScore = -pvSearch(&pos, -10000, 10000, i, 0, &bestMove);
      //printf("Move found with score %d at depth %d\n", moveScore, i);
      //printMove(bestMove);
   }

   #ifdef DEBUG
   printTreeDebug();
   printTTDebug();
   #endif


   printf("Move found with score %d\n", moveScore);
   printMove(bestMove);

   return bestMove;
}


int pvSearch( Position* pos, int alpha, int beta, char depth, char ply, Move* returnMove) {
   if( depth == 0 ) {
      int q_eval = quiesce(pos, alpha, beta, ply, 0);
      return q_eval;
   }
   #ifdef DEBUG
   pvs_count++;
   #endif

   Move moveList[MAX_MOVES];
   int moveVals[MAX_MOVES];
   int size = generateLegalMoves(*pos, moveList);
   //Handle Draw or Mate
   if(size == 0){
      if(pos->flags & IN_CHECK) return INT_MIN;
      else return 0;
   }
   if(pos->halfmove_clock >= 50) return 0;


   TTEntry* ttEntry = getTTEntry(pos->hash);
   Move ttMove = NO_MOVE;
   if (ttEntry) {
      if(ttEntry->depth >= depth){
         switch (ttEntry->nodeType) {
            case PV_NODE: // Exact value
               if(returnMove) *returnMove = ttEntry->move;
               return ttEntry->eval;
            case CUT_NODE: // Lower bound
               if(returnMove) *returnMove = ttEntry->move;
               if (ttEntry->eval >= beta) return beta;
               break;
            case ALL_NODE: // Upper bound
               if (ttEntry->eval > alpha) alpha = ttEntry->eval;
               break;
         }
      }
      else{
         ttMove = ttEntry->move;
      }
   }

   char bSearchPv = 1;
   
   evalMoves(moveList, moveVals, size, ttMove, killerMoves[(int)ply], KMV_CNT, *pos);

   Position prevPos = *pos;
   Move bestMove = NO_MOVE;
   for (int i = 0; i < size; i++)  {
      selectSort(i, moveList, moveVals, size);
      makeMove(pos, moveList[i]);
      int score;
      if ( bSearchPv ) {
         score = -pvSearch(pos, -beta, -alpha, depth - 1, ply + 1, NULL);
      } else {
         score = -zwSearch(pos, -alpha, depth - 1, ply + 1);
         if ( score > alpha && score < beta)
            score = -pvSearch(pos, -beta, -alpha, depth - 1, ply + 1, NULL);
      }
      *pos = prevPos; //Unmake move
      if( score >= beta ) {
         storeTTEntry(pos->hash, depth, beta, CUT_NODE, moveList[i]);
         storeKillerMove(ply, moveList[i]);
         storeHistoryMove(pos->flags, moveList[i], depth);
         return beta;
      }
      if( score > alpha ) {
         alpha = score;
         bestMove = moveList[i];
         bSearchPv = 0; 
      }
   }
   if (bestMove != NO_MOVE) {
      // PV Node (exact value)
      storeTTEntry(pos->hash, depth, alpha, PV_NODE, bestMove);
   } else {
      // ALL Node (upper bound)
      storeTTEntry(pos->hash, depth, alpha, ALL_NODE, NO_MOVE);
   }
   if(returnMove) *returnMove = bestMove;
   return alpha;
}

// fail-hard zero window search, returns either beta-1 or beta
int zwSearch( Position* pos, int beta, char depth, char ply ) {
   // alpha == beta - 1
   // this is either a cut- or all-node
   if( depth == 0 ) return quiesce(pos, beta-1, beta, ply, 0);
   #ifdef DEBUG
   zws_count++;
   #endif

   Move moveList[MAX_MOVES];
   int moveVals[MAX_MOVES];
   int size = generateLegalMoves(*pos, moveList);
   //Handle Draw or Mate
   if(size == 0){
      if(pos->flags & IN_CHECK) return INT_MIN;
      else return 0;
   }
   if(pos->halfmove_clock >= 50) return 0;

   TTEntry* ttEntry = getTTEntry(pos->hash);
   Move ttMove = NO_MOVE;
   if (ttEntry) {
      if(ttEntry->depth >= depth){
         switch (ttEntry->nodeType) {
            case PV_NODE: // Exact value
               return ttEntry->eval;
               break;
            case CUT_NODE: // Lower bound
               if (ttEntry->eval >= beta) return beta;
               break;
            case ALL_NODE: // Upper bound
               if (ttEntry->eval <= beta-1) return beta-1;
               break;
         }
      }
      else{
         ttMove = ttEntry->move;
      }
   }


   evalMoves(moveList, moveVals, size, ttMove, killerMoves[(int)ply], KMV_CNT, *pos);

   Position prevPos = *pos;
   for (int i = 0; i < size; i++)  {
     selectSort(i, moveList, moveVals, size);
     makeMove(pos, moveList[i]);
     int score = -zwSearch(pos, 1-beta, depth - 1, ply + 1);
     *pos = prevPos;
     //unmakeMove(pos)
     if( score >= beta ){
        storeTTEntry(pos->hash, depth, beta, CUT_NODE, moveList[i]);
        storeKillerMove(ply, moveList[i]);
        storeHistoryMove(pos->flags, moveList[i], depth);
        return beta;   // fail-hard beta-cutoff
     }
   }
   return beta-1; // fail-hard, return alpha
}

//quisce search
int quiesce( Position* pos, int alpha, int beta, char ply, char q_ply) {
   #ifdef DEBUG
   q_count++;
   #endif
   
   Move moveList[MAX_MOVES];
   int moveVals[MAX_MOVES];
   int size = generateLegalMoves(*pos, moveList);

   //Handle Draw or Mate
   if(size == 0){
      if(pos->flags & IN_CHECK) return INT_MIN;
      else return 0;
   }
   if(pos->halfmove_clock >= 50) return 0;

   int stand_pat = evaluate(*pos);
   if( stand_pat >= beta )
      return beta;
   if( alpha < stand_pat )
      alpha = stand_pat;
   if(q_ply >= MAX_QUIESCE_PLY) return alpha;

   evalMoves(moveList, moveVals, size, NO_MOVE, killerMoves[(int)ply], KMV_CNT, *pos);
   
   Position prevPos = *pos;
   for (int i = 0; i < size; i++)  {
      selectSort(i, moveList, moveVals, size);
      if(!(GET_FLAGS(moveList[i]) & CAPTURE)) continue;
      makeMove(pos, moveList[i]);
      int score = -quiesce(pos,  -beta, -alpha, ply + 1, q_ply + 1);
      *pos = prevPos;

      if( score >= beta ){
         storeKillerMove(ply, moveList[i]);
         return beta;
      }
      if( score > alpha ){
         alpha = score;
      }
    }
    return alpha;
}


void selectSort(int i, Move *moveList, int *moveVals, int size) {
    int maxIdx = i;

    for (int j = i + 1; j < size; j++) {
        if (moveVals[j] > moveVals[maxIdx]) {
            maxIdx = j;
        }
    }

    if (maxIdx != i) {
        int tempVal = moveVals[i];
        moveVals[i] = moveVals[maxIdx];
        moveVals[maxIdx] = tempVal;

        Move tempMove = moveList[i];
        moveList[i] = moveList[maxIdx];
        moveList[maxIdx] = tempMove;
    }
}


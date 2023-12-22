#include "tree.h"
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "movement.h"
#include "types.h"
#include "util.h"
#include "evaluator.h"
#include "transposition.h"
#include "globals.h"


#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#define MAX_DEPTH 8
#define ID_STEP 1 //Changing this may break Aspiration windows (it will)

#define CHECKMATE_VALUE (MAX_EVAL - 1000)

#define KMV_CNT 3 //How many killer moves are stored for a pos

#define MAX_QUIESCE_PLY 10 //How far q search can go
#define MAX_PLY 255 //How far the total search can go

#define LMR_DEPTH 10 //The depth gone to for lmr
#define LMR_MIN_MOVE 10 //the move number to start performing lmr on

#define MAX_ASP_START 100 //Maximum size of bounds for an aspiration window to start on
#define ASP_EDGE 1//Buffer size of aspiration window

//static void selectSort(int i, Move *moveList, int *moveVals, int size);


#ifdef DEBUG

#if defined(__unix__) || defined(__APPLE__)
#define DEBUG_TIME
#include <time.h>
#include "pthread.h"
static struct timespec start_time, end_time;
#endif

static int64_t pvs_count;
static int64_t zws_count;
static int64_t q_count;

void startTreeDebug(void){
   pvs_count = 0;
   zws_count = 0;
   q_count = 0;
   #ifdef DEBUG_TIME
   clock_gettime(CLOCK_MONOTONIC, &start_time);
   #endif
}
void printTreeDebug(void){
   #ifdef DEBUG_TIME
   clock_gettime(CLOCK_MONOTONIC, &end_time);
   double elap_time = (end_time.tv_sec - start_time.tv_sec) +
                     (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
   #endif

   uint64_t total_count = pvs_count + zws_count + q_count;

   printf("Tree searched %" PRIu64 " evals (pvs: %" PRIu64 ", zws: %" PRIu64 ", q: %" PRIu64 ")\n", total_count, pvs_count, zws_count, q_count);
   #ifdef DEBUG_TIME
   printf("Took %lf seconds, with %lf eval/sec\n", elap_time, ( (double)total_count) / elap_time);
   #endif
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

uint32_t getHistoryScore(char pos_flags, Move move){
   if(GET_FLAGS(move) & CAPTURE) return 0;
   return historyTable[pos_flags & WHITE_TURN][GET_FROM(move)][GET_TO(move)];
}

/*
* Get that move son.
*/
int getBestMove(Position pos){
   
   #ifdef DEBUG
   startTTDebug();
   startTreeDebug();
   #endif
   clearKillerMoves(); //TODO: make thread safe!
   int i = 1;
   int eval, eval_prev, asp_upper, asp_lower;
   while(run_get_best_move 
   #ifdef MAX_DEPTH
   && i <= MAX_DEPTH
   #endif
   ){
      Move *pvArray = malloc(sizeof(Move) * (i*i + i)/2);
      memset(pvArray, 0, sizeof(Move) * (i*i + i)/2);

      if(!pvArray){
         printf("info Warning: failed to allocate space for pvArray");
         return -1;
      }
      //printf("Running pv search at depth %d\n", i);
      if(i <= 2) eval = -pvSearch(&pos, INT_MIN+1, INT_MAX, i, 0, pvArray, 0);
      else{
         int asp_dif = (abs(eval_prev - eval)/2)+ASP_EDGE;
         asp_upper = asp_lower = MIN(asp_dif, MAX_ASP_START);
         int q = (eval_prev + eval) / 2;
         //printf("Eval: %d Eval_Prev: %d, asp_up: %d, asp_lower: %d, q:%d", eval, eval_prev, asp_upper, asp_lower, q);
         //printf("\nRunning with window: %d, %d\n", q-asp_lower, q+asp_upper);
         int eval_tmp = -pvSearch(&pos, q-asp_lower, q+asp_upper, i, 0, pvArray, 0);
         while(eval_tmp <= q-asp_lower || eval_tmp >= q+asp_upper || pvArray[0] == NO_MOVE){
            if(eval_tmp <= q-asp_lower){ asp_lower *= 2;}
            if(eval_tmp >= q+asp_upper){ asp_upper *= 2;}
            if(pvArray[0] == NO_MOVE){
               asp_upper *= 2;
               asp_lower *= 2;
            }
            //printf("Running again with window: %d, %d\n", q-asp_lower, q+asp_upper);
            eval_tmp = -pvSearch(&pos, q-asp_lower, q+asp_upper, i, 0, pvArray, 0);
         }
         eval_prev = eval;
         eval = eval_tmp;
      }

      printPV(pvArray, i);
      printf("found with score %d\n", eval);

      global_best_move = pvArray[0];
      free(pvArray);
      i+=ID_STEP;
   }

   #ifdef DEBUG
   printTreeDebug();
   printTTDebug();
   #endif

   #ifdef MAX_DEPTH
   return 1;
   #endif

   return 0;
}


int pvSearch( Position* pos, int alpha, int beta, char depth, char ply, Move* pvArray, int pvIndex) {
   //printf("Depth = %d, Ply = %d, Depth+ply = %d\n", depth, ply, depth+ply);
   #if defined(__unix__) || defined(__APPLE__)
   if(!run_get_best_move) pthread_exit(NULL);
   #elif defined(_WIN32) || defined(_WIN64)
   if(!run_get_best_move) ExitThread(0);
   #endif
   //printf("pvSearch (%llu) a: %d, b: %d, d: %d, ply: %d, retMove %p\n", pos->hash, alpha, beta, (int)depth, (int)ply, returnMove);
   if( depth <= 0 ) {
      int q_eval = quiesce(pos, alpha, beta, ply, 0);
      //printf("Returning q_eval: %d\n", q_eval);
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
      if(pos->flags & IN_CHECK) return CHECKMATE_VALUE;
      else return 0;
   }
   if(pos->halfmove_clock >= 50) return 0;



   char bSearchPv = 1;
   int pvNextIndex = pvIndex + depth;

   TTEntry* ttEntry = getTTEntry(pos->hash);
   Move ttMove = NO_MOVE;
   if (ttEntry) {
      ttMove = ttEntry->move;
      if(ttEntry->depth >= depth){
         switch (ttEntry->nodeType) {
            case PV_NODE: // Exact value
               pvArray[pvIndex] = ttEntry->move;
               //movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, depth - 1);
               return ttEntry->eval;
            case CUT_NODE: // Lower bound
               if (ttEntry->eval >= beta){
                  //printf("Returning CUT_NODE beta: %d\n", beta);
                  return beta;
               }
               break;
            case ALL_NODE: // Upper bound
               if (ttEntry->eval > alpha){
                  alpha = ttEntry->eval;
                  pvArray[pvIndex] = ttEntry->move;
                  movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, depth - 1);
                  bSearchPv = 0;
               }
               break;
            case Q_NODE:
               ttMove = NO_MOVE;
            default:
               break;
         }
      }
   }

   
   evalMoves(moveList, moveVals, size, ttMove, killerMoves[(int)ply], KMV_CNT, *pos);

   //Set up late move reduction rules
   char LMR = TRUE;
   if(pos->flags & IN_CHECK) LMR = FALSE;
   if(depth < LMR_DEPTH) LMR = FALSE;


   Position prevPos = *pos;
   for (int i = 0; i < size; i++)  {
      selectSort(i, moveList, moveVals, size);
      makeMove(pos, moveList[i]);

      //Update current late move reduction rules
      char LMR_cur = LMR;
      if(pos->flags & IN_CHECK) LMR_cur = FALSE;
      if(i < LMR_MIN_MOVE) LMR_cur = FALSE;


      int score;
      if ( bSearchPv ) {
         score = -pvSearch(pos, -beta, -alpha, depth - 1, ply + 1, pvArray, pvNextIndex);
      } else {
         if(LMR_cur) score = -zwSearch(pos, -alpha, LMR_DEPTH, ply + 1);
         else score = -zwSearch(pos, -alpha, depth - 1, ply + 1);
         if ( score > alpha && score < beta)
            score = -pvSearch(pos, -beta, -alpha, depth - 1, ply + 1, pvArray, pvNextIndex);
      }
      *pos = prevPos; //Unmake move
      if( score >= beta ) {
         storeTTEntry(pos->hash, depth, beta, CUT_NODE, moveList[i]);
         storeKillerMove(ply, moveList[i]);
         storeHistoryMove(pos->flags, moveList[i], depth);
         //printf("Returning beta cutoff: %d >= %d\n", score, beta);
         return beta;
      }
      if( score > alpha ) {
         alpha = score;
         pvArray[pvIndex] = moveList[i];
         movcpy (pvArray + pvIndex + 1, pvArray + pvNextIndex, depth - 1);
         bSearchPv = 0; 
      }
   }
   if (!bSearchPv) {
      // PV Node (exact value)
      storeTTEntry(pos->hash, depth, alpha, PV_NODE, pvArray[pvIndex]);
   } else {
      // ALL Node (upper bound)
      storeTTEntry(pos->hash, depth, alpha, ALL_NODE, NO_MOVE);
   }
   //printf("Returning alpha: %d\n", alpha);
   return alpha;
}

// fail-hard zero window search, returns either beta-1 or beta
int zwSearch( Position* pos, int beta, char depth, char ply ) {
   #if defined(__unix__) || defined(__APPLE__)
   if(!run_get_best_move) pthread_exit(NULL);
   #elif defined(_WIN32) || defined(_WIN64)
   if(!run_get_best_move) ExitThread(0);
   #endif
   // alpha == beta - 1
   // this is either a cut- or all-node
   if( depth <= 0 ) return quiesce(pos, beta-1, beta, ply, 0);
   #ifdef DEBUG
   zws_count++;
   #endif

   Move moveList[MAX_MOVES];
   int moveVals[MAX_MOVES];
   int size = generateLegalMoves(*pos, moveList);
   //Handle Draw or Mate
   if(size == 0){
      if(pos->flags & IN_CHECK) return CHECKMATE_VALUE;
      else return 0;
   }
   if(pos->halfmove_clock >= 50) return 0;

   TTEntry* ttEntry = getTTEntry(pos->hash);
   Move ttMove = NO_MOVE;
   if (ttEntry) {
      ttMove = ttEntry->move;
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
            default:
               break;
         }
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
   #if defined(__unix__) || defined(__APPLE__)
   if(!run_get_best_move) pthread_exit(NULL);
   #elif defined(_WIN32) || defined(_WIN64)
   if(!run_get_best_move) ExitThread(0);
   #endif
   #ifdef DEBUG
   q_count++;
   #endif
   
   Move moveList[MAX_MOVES];
   int moveVals[MAX_MOVES];
   int size = generateLegalMoves(*pos, moveList);

   //Handle Draw or Mate
   if(size == 0){
      if(pos->flags & IN_CHECK) return CHECKMATE_VALUE;
      else return 0;
   }
   if(pos->halfmove_clock >= 50) return 0;

   int stand_pat = evaluate(*pos);
   if( stand_pat >= beta )
      return beta;
   if( alpha < stand_pat )
      alpha = stand_pat;
   if(q_ply >= MAX_QUIESCE_PLY) return alpha;

   TTEntry* ttEntry = getTTEntry(pos->hash);
   Move ttMove = NO_MOVE;
   if (ttEntry) {
      ttMove = ttEntry->move;
      switch (ttEntry->nodeType) {
         case Q_NODE:
            return ttEntry->eval;
            break;
         case CUT_NODE:
            if (ttEntry->eval >= beta) return beta;
            break;
         case ALL_NODE: // Upper bound
            if (ttEntry->eval > alpha) alpha = ttEntry->eval;
            break;
         default:
            break;
      }
   }

   evalMoves(moveList, moveVals, size, ttMove, killerMoves[(int)ply], KMV_CNT, *pos);
   
   Move bestMove = NO_MOVE;
   Position prevPos = *pos;
   for (int i = 0; i < size; i++)  {
      selectSort(i, moveList, moveVals, size);
      if(!(GET_FLAGS(moveList[i]) & CAPTURE)) continue;
      makeMove(pos, moveList[i]);
      int score = -quiesce(pos,  -beta, -alpha, ply + 1, q_ply + 1);
      *pos = prevPos;

      if( score >= beta ){
         storeKillerMove(ply, moveList[i]);
         storeTTEntry(pos->hash, 0, beta, CUT_NODE, moveList[i]);
         return beta;
      }
      if( score > alpha ){
         alpha = score;
         bestMove = moveList[i];
      }
   }
   storeTTEntry(pos->hash, 0, beta, Q_NODE, bestMove);
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


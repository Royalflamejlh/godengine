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


#if defined(__unix__) || defined(__APPLE__)
#include "pthread.h"
#elif defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#ifdef __COMPILE_DEBUG
#define DEBUG
#endif

#ifdef __FAST_AS_POOP

#define ID_STEP 1 //Changing this may break Aspiration windows (it will)

#define CHECKMATE_VALUE (MAX_EVAL - 1000)

#define KMV_CNT 3 //How many killer moves are stored for a pos 

#define MAX_QUIESCE_PLY 4
#define MAX_PLY 255 //How far the total search can go

#define LMR_DEPTH 3
#define LMR_MIN_MOVE 2

#define MAX_ASP_START 100
#define ASP_EDGE 1

#define FUTIL_DEPTH 1 //Depth to start futility pruning
#define FUTIL_MARGIN 100 //Score difference for a node to be futility pruned

#define RAZOR_DEPTH 3 //Depth to start razoring
#define RAZOR_MARGIN PAWN_VALUE*2 //Margin to start razoring

#define NULL_PRUNE_R 2 //How much Null prunin' takes off
#define NULL_PRUNE_DEPTH 3 //What depth at which null pruning starts
#else

#define ID_STEP 1 //Changing this may break Aspiration windows (it will)

#define CHECKMATE_VALUE (MAX_EVAL - 1000)

#define KMV_CNT 3 //How many killer moves are stored for a pos (fast: 4)

#define MAX_QUIESCE_PLY 10 //How far q search can go (fast: 4)
#define MAX_PLY 255 //How far the total search can go

#define LMR_DEPTH 10 //The depth gone to for lmr (fast : 3)
#define LMR_MIN_MOVE 10 //the move number to start performing lmr on (fast: 2)

#define MAX_ASP_START 1000 //Maximum size of bounds for an aspiration window to start on (fast: 100)
#define ASP_EDGE 30  //Buffer size of aspiration window (fast: 1)

#define FUTIL_DEPTH 1 //Depth to start futility pruning
#define FUTIL_MARGIN PAWN_VALUE //Score difference for a node to be futility pruned

#define RAZOR_DEPTH 3 //Depth to start razoring
#define RAZOR_MARGIN PAWN_VALUE*3 //Margin to start razoring

#define NULL_PRUNE_R 2 //How much Null prunin' takes off
#define NULL_PRUNE_DEPTH 4 //What depth at which null pruning starts

#endif

//Set up the Depth sizes depending on mode
#ifdef DEBUG
#define MAX_DEPTH 10
#elif defined(__PROFILE)
#define MAX_DEPTH 8
#endif

//static void selectSort(int i, Move *moveList, int *moveVals, int size);

#ifdef DEBUG

#if defined(__unix__) || defined(__APPLE__)
#define DEBUG_TIME
#include <time.h>
static struct timespec start_time, end_time;
#endif

typedef enum searchs{
   PVS,
   ZWS,
   QS,
   SEARCH_TYPE_COUNT
} SearchType;

typedef enum stats{
   NODE_COUNT,
   NODE_LOOP_CHILDREN,
   NODE_BETA_CUT,
   NODE_PRUNED
} DebugStats;

static int64_t debug[sizeof(SearchType)][sizeof(DebugStats)] = {0};

void startTreeDebug(void){
   #ifdef DEBUG_TIME
   clock_gettime(CLOCK_MONOTONIC, &start_time);
   #endif
}
void printTreeDebug(void){
   printf("\n--------------------------------------------------------------------------------------------------------------\n");
   #ifdef DEBUG_TIME
   clock_gettime(CLOCK_MONOTONIC, &end_time);
   double elap_time = (end_time.tv_sec - start_time.tv_sec) +
                     (end_time.tv_nsec - start_time.tv_nsec) / 1e9;
   #endif

   int64_t total_count = 0;
   int64_t pvs_count = 0, zws_count = 0, q_count = 0;

   for (int i = 0; i < SEARCH_TYPE_COUNT; i++) {
      int64_t node_count = debug[i][NODE_COUNT];
      total_count += node_count;
      char typestr[5];
      if (i == PVS){
         pvs_count = node_count;
         strcpy(typestr, "PVS");
      }
      else if (i == ZWS){
         zws_count = node_count;
         strcpy(typestr, "ZWS");
      }
      else if (i == QS){
         q_count = node_count;
         strcpy(typestr, "Q");
      }
      int64_t fully = debug[i][NODE_LOOP_CHILDREN] - debug[i][NODE_BETA_CUT] - debug[i][NODE_PRUNED];
      printf("%s: Called: %" PRIu64 ", Entered Loop: %" PRIu64 ", Beta Cuts: %" PRIu64 ", Prunes: %" PRIu64 ", Fully Searched: %" PRIu64 "\n",
            typestr, debug[i][NODE_COUNT], debug[i][NODE_LOOP_CHILDREN], debug[i][NODE_BETA_CUT], debug[i][NODE_PRUNED], fully);
   }

   printf("\nTree searched %" PRIu64 " evals (pvs: %" PRIu64 ", zws: %" PRIu64 ", q: %" PRIu64 ")\n", total_count, pvs_count, zws_count, q_count);

   #ifdef DEBUG_TIME
   printf("\nTook %lf seconds, with %lf eval/sec\n", elap_time, ((double)total_count) / elap_time);
   #endif
   printf("\n--------------------------------------------------------------------------------------------------------------\n");
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
   
   #ifdef TT_DEBUG
   startTTDebug();
   #endif
   clearKillerMoves(); //TODO: make thread safe!
   int i = 1;
   int eval, eval_prev, asp_upper, asp_lower;
   while(run_get_best_move 
   #ifdef MAX_DEPTH
   && i <= MAX_DEPTH
   #endif
   ){
      #ifdef DEBUG
      startTreeDebug();
      #endif
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
            printf("Running again with window: %d, %d\n", q-asp_lower, q+asp_upper);
            eval_tmp = -pvSearch(&pos, q-asp_lower, q+asp_upper, i, 0, pvArray, 0);
         }
         eval_prev = eval;
         eval = eval_tmp;
      }

      printPV(pvArray, i);
      printf("found with score %d\n", eval);

      global_best_move = pvArray[0];
      free(pvArray);
      #ifdef DEBUG
      printTreeDebug();
      #endif
      i+=ID_STEP;
   }

   #ifdef TT_DEBUG
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
   debug[PVS][NODE_COUNT]++;
   #endif

   Move moveList[MAX_MOVES];
   int moveVals[MAX_MOVES];
   int size = generateLegalMoves(*pos, moveList);
   //Handle Draw or Mate
   if(size == 0){
      if(pos->flags & IN_CHECK) return CHECKMATE_VALUE - ply;
      else return 0;
   }
   if(pos->halfmove_clock >= 50) return 0;

   char bSearchPv = 1;
   int pvNextIndex = pvIndex + depth;

   //Test the TT table
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

   //Null move prunin'
   if(!(pos->flags & IN_CHECK) && pos->stage != END_GAME && depth >= NULL_PRUNE_DEPTH){
      Position prevPos = *pos;
      makeNullMove(pos);
      int score = -zwSearch(pos, 1-beta, depth - NULL_PRUNE_R - 1, ply + 1);
      if( score >= beta ){
         storeTTEntry(pos->hash, depth, beta, CUT_NODE, NO_MOVE);
         #ifdef DEBUG
         debug[PVS][NODE_PRUNED]++;
         #endif
         return beta;
      }
      *pos = prevPos;
   }



   evalMoves(moveList, moveVals, size, ttMove, killerMoves[(int)ply], KMV_CNT, *pos);

   //Set up late move reduction rules
   char LMR = TRUE;
   if(pos->flags & IN_CHECK) LMR = FALSE;
   if(depth < LMR_DEPTH) LMR = FALSE;

   #ifdef DEBUG
   if(size > 0) debug[PVS][NODE_LOOP_CHILDREN]++;
   #endif
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
      *pos = prevPos;
      if( score >= beta ) { //Beta cutoff
         storeTTEntry(pos->hash, depth, beta, CUT_NODE, moveList[i]);
         storeKillerMove(ply, moveList[i]);
         storeHistoryMove(pos->flags, moveList[i], depth);
         //printf("Returning beta cutoff: %d >= %d\n", score, beta);
         #ifdef DEBUG
         debug[PVS][NODE_BETA_CUT]++;
         #endif
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
   debug[ZWS][NODE_COUNT]++;
   #endif

   Move moveList[MAX_MOVES];
   int moveVals[MAX_MOVES];
   int size = generateLegalMoves(*pos, moveList);
   //Handle Draw or Mate
   if(size == 0){
      if(pos->flags & IN_CHECK) return CHECKMATE_VALUE - ply;
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

   //Null move prunin'
   if(!(pos->flags & IN_CHECK) && pos->stage != END_GAME && depth >= NULL_PRUNE_DEPTH){
      Position prevPos = *pos;
      makeNullMove(pos);
      int score = -zwSearch(pos, 1-beta, depth - NULL_PRUNE_R - 1, ply + 1);
      if( score >= beta ){
         storeTTEntry(pos->hash, depth, beta, CUT_NODE, NO_MOVE);
         #ifdef DEBUG
         debug[ZWS][NODE_PRUNED]++;
         #endif
         return beta;
      }
      *pos = prevPos;
   }


   evalMoves(moveList, moveVals, size, ttMove, killerMoves[(int)ply], KMV_CNT, *pos);

   //Set up prunability
   char prunable = !(pos->flags & IN_CHECK);
   if(beta-1 >= CHECKMATE_VALUE - 2*MAX_MOVES) prunable = 0;

   #ifdef DEBUG
   if(size > 0) debug[ZWS][NODE_LOOP_CHILDREN]++;
   #endif
   Position prevPos = *pos;
   for (int i = 0; i < size; i++)  {
      selectSort(i, moveList, moveVals, size);
      makeMove(pos, moveList[i]);
      int score = -zwSearch(pos, 1-beta, depth - 1, ply + 1);
      *pos = prevPos;

      if( score >= beta ){ //Beta Cutoff
         storeTTEntry(pos->hash, depth, beta, CUT_NODE, moveList[i]);
         storeKillerMove(ply, moveList[i]);
         storeHistoryMove(pos->flags, moveList[i], depth);
         #ifdef DEBUG
         debug[ZWS][NODE_BETA_CUT]++;
         #endif
         return beta;   // fail-hard beta-cutoff
      }

      //Set up prunability
      if(pos->flags & IN_CHECK) prunable = 0;
      if(GET_FLAGS(moveList[i]) & CAPTURE) prunable = 0;

      if(prunable && (depth <= FUTIL_DEPTH && score < beta - 1 - FUTIL_MARGIN)){ //Futility Pruning
         #ifdef DEBUG
         debug[ZWS][NODE_PRUNED]++;
         #endif
         return beta-1;
      }

      if(prunable && (depth <= RAZOR_DEPTH && score < beta - RAZOR_MARGIN)){
         int razor_score = quiesce(pos, beta-1, beta, ply, 0);
         if(razor_score >= beta){
            #ifdef DEBUG
            debug[ZWS][NODE_PRUNED]++;
            #endif
            return razor_score;
         }
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
   debug[QS][NODE_COUNT]++;
   #endif
   
   Move moveList[MAX_MOVES];
   int moveVals[MAX_MOVES];
   int size = generateLegalMoves(*pos, moveList);

   //Handle Draw or Mate
   if(size == 0){
      if(pos->flags & IN_CHECK) return CHECKMATE_VALUE - ply;
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
   #ifdef DEBUG
   if(size > 0) debug[QS][NODE_LOOP_CHILDREN]++;
   #endif
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
         #ifdef DEBUG
         debug[QS][NODE_BETA_CUT]++;
         #endif
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


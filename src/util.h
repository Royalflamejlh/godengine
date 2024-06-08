#ifndef UTIL_H
#define UTIL_H
#include <stdlib.h>


#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#include "types.h"
void printMove(Move move);
void printMoveShort(Move move);
void printMoveSpaced(Move move);
u64 perft(i32 depth, Position pos);
i32 checkMoveCount(Position pos);
i32 python_init();
i32 python_close();

Move moveStrToType(Position pos, char* str);
Stage calculateStage(Position pos);

void printPV(Move *pvArray, i32 depth);
void printPVInfo(i32 depth, i32 score, Move *pvArray, u64 nodes, double time);

static inline i32 count_bits(u64 v){
    u32 c;
    for (c = 0; v; c++){
        v &= v - 1;
    }
    return c;
}

static inline u64 random_uint64() {
  u64 u1, u2, u3, u4;
  u1 = (u64)(rand()) & 0xFFFF; u2 = (u64)(rand()) & 0xFFFF;
  u3 = (u64)(rand()) & 0xFFFF; u4 = (u64)(rand()) & 0xFFFF;
  return u1 | (u2 << 16) | (u3 << 32) | (u4 << 48);
}

static inline void movcpy (Move* pTarget, const Move* pSource, i32 n) {
   while (n-- && (*pTarget++ = *pSource++));
}

/*
* Returns whether the position has Insufficient material / Drawn
*/
static inline u8 isInsufficient(Position pos){
   if(pos.stage != END_GAME) return FALSE;
   u32 piece_count_w = count_bits(pos.color[0]);
   u32 piece_count_b = count_bits(pos.color[1]);
   if(piece_count_w == 1 && piece_count_b == 1) return TRUE;
   if(piece_count_w <= 2 && piece_count_b == 1){
      if((pos.knight[0] | pos.bishop[0])) return TRUE; 
   }
   if(piece_count_w == 1 && piece_count_b <= 2){
      if((pos.knight[1] | pos.bishop[1])) return TRUE;
   }
   if(piece_count_w <= 2 && piece_count_b <= 2){
      if((pos.knight[0] | pos.bishop[0]) && (pos.knight[1] | pos.bishop[1])) return TRUE;
   }
   return FALSE;
}

/*
* Returns whether the position is a Repetition
*/
static inline i32 isRepetition(Position* pos){
   for(i32 i = pos->hashStack.last_reset_idx; i < pos->hashStack.current_idx; i++){
      if(pos->hashStack.ptr[i] == pos->hashStack.ptr[pos->hashStack.current_idx]) return 1;
   }
   return 0;
}

/*
*
* Hash Stack Stuff
*
*/

HashStack createHashStack(void);
i32 removeHashStack(HashStack *hashStack);
void doubleHashStack(HashStack *hs);




#endif
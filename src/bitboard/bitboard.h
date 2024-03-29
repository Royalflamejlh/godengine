//
//  bitboard.h
//  godengine
//
//  Created by John Howard on 11/22/23.
//

#ifndef bitboard_h
#define bitboard_h
#include <stdint.h>
#include <stdio.h>
#include "bbutils.h"
#include "../types.h"

void generateMasks(void);

uint64_t getKnightAttacks(uint64_t knights);
uint64_t getKnightMovesAppend(uint64_t knights, uint64_t ownPieces, uint64_t oppPieces, Move* moveList, int* idx);
uint64_t getKnightThreatMovesAppend(uint64_t knights, uint64_t ownPieces, uint64_t oppPieces, int opp_king_square, Move* moveList, int* idx);

uint64_t getBishopAttacks(uint64_t bishops, uint64_t ownPieces, uint64_t oppPieces);
uint64_t getBishopMovesAppend(uint64_t bishops, uint64_t ownPieces, uint64_t oppPieces, Move* moveList, int* idx);
uint64_t getBishopThreatMovesAppend(uint64_t bishops, uint64_t ownPieces, uint64_t oppPieces, uint64_t checkSquares, Move* moveList, int* idx);

uint64_t getRookAttacks(uint64_t rooks, uint64_t ownPieces, uint64_t oppPieces);
uint64_t getRookMovesAppend(uint64_t rooks, uint64_t ownPieces, uint64_t oppPieces, Move* moveList, int* idx);
uint64_t getRookThreatMovesAppend(uint64_t rooks, uint64_t ownPieces, uint64_t oppPieces, uint64_t checkSquares, Move* moveList, int* idx);

uint64_t getPawnAttacks(uint64_t pawns, char flags);
uint64_t getPawnMovesAppend(uint64_t pawns, uint64_t ownPieces, uint64_t oppPieces,  uint64_t enPassant, char flags, Move* moveList, int* idx);
uint64_t getPawnThreatMovesAppend(uint64_t pawns, uint64_t ownPieces, uint64_t oppPieces,  uint64_t enPassant, char flags, int opp_king_square, Move* moveList, int* idx);

uint64_t getKingAttacks(uint64_t kings);
uint64_t getKingMovesAppend(uint64_t kings, uint64_t ownPieces, uint64_t oppPieces, uint64_t oppAttackMask, Move* moveList, int* idx);
uint64_t getKingThreatMovesAppend(uint64_t kings, uint64_t ownPieces, uint64_t oppPieces, uint64_t oppAttackMask, Move* moveList, int* idx);

void getCastleMovesAppend(uint64_t white, uint64_t b_attack_mask, char flags, Move* moveList, int* idx);

void getCheckMovesAppend(Position position, Move* moveList, int* idx);

void getPinnedMovesAppend(Position position, Move* moveList, int* idx);
void getPinnedThreatMovesAppend(Position position, uint64_t r_check_squares, uint64_t b_check_squares, int kingSq, Move* moveList, int* idx);

uint64_t getAttackers(Position pos, int square, int attackerColor);

uint64_t generateAttacks(Position position, int turn);

static inline void setAttackMasks(Position *pos){
    pos->attack_mask[1] = generateAttacks(*pos, 1);
    pos->attack_mask[0] = generateAttacks(*pos, 0);
}
#endif /* bitboard_h */

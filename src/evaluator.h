#ifndef evaluate_h
#define evaluate_h

#include "types.h"

#define KING_VALUE     100000
#define QUEEN_VALUE     10000
#define ROOK_VALUE       5000
#define BISHOP_VALUE     3550
#define KNIGHT_VALUE     3500
#define PAWN_VALUE       1000

#ifdef DEBUG
i32 evaluate(Position pos, u8 verbose);
#else
i32 evaluate(Position pos);
#endif // DEBUG

i32 quickEval(Position pos);
void initPST(void);
i32 evalMove(Move move, Position* pos);
void q_evalMoves(Move* moveList, i32* moveVals, i32 size, Position pos);
i32 see ( Position pos, u32 toSq, PieceIndex target, u32 frSq, PieceIndex aPiece);

#endif //evaluate_h
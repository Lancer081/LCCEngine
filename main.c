#include <stdio.h>

#define U64 unsigned long long

#define get_bit(bitboard, bit) (bitboard & (1ULL << bit))
#define set_bit(bitboard, bit) (bitboard |= (1ULL << bit))
#define pop_bit(bitboard, bit) (get_bit(bitboard, bit) ? bitboard ^= (1ULL << bit) : 0)

enum {
	a8, b8, c8, d8, e8, f8, g8, h8,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a1, b1, c1, d1, e1, f1, g1, h1
};

enum { white, black };

const U64 not_A_file = 18374403900871474942ULL;
const U64 not_H_file = 9187201950435737471ULL;
const U64 not_HG_file = 4557430888798830399ULL;
const U64 not_AB_file = 18229723555195321596ULL;

U64 pawn_attacks[2][64];
U64 knight_attacks[64];
U64 king_attacks[64];

U64 mask_pawn_attacks(int square, int side)
{
	U64 bitboard = 0ULL;
	U64 attacks = 0ULL;
	
	set_bit(bitboard, square);
	
	if (side == white)
	{
		if ((bitboard >> 7) & not_A_file) attacks |= (bitboard >> 7);
		if ((bitboard >> 9) & not_H_file) attacks |= (bitboard >> 9);
	}
	else
	{
		if ((bitboard << 7) & not_H_file) attacks |= (bitboard << 7);
		if ((bitboard << 9) & not_A_file) attacks |= (bitboard << 9);
	}
	
	return attacks;
}

U64 mask_knight_attacks(int square)
{
	U64 bitboard = 0ULL;
	U64 attacks = 0ULL;
	
	set_bit(bitboard, square);

	if ((bitboard >> 17) & not_H_file) attacks |= (bitboard >> 17);
	if ((bitboard >> 15) & not_A_file) attacks |= (bitboard >> 15);
	if ((bitboard >> 10) & not_HG_file) attacks |= (bitboard >> 10);
	if ((bitboard >> 6) & not_AB_file) attacks |= (bitboard >> 6);
	if ((bitboard << 17) & not_A_file) attacks |= (bitboard << 17);
	if ((bitboard << 15) & not_H_file) attacks |= (bitboard << 15);
	if ((bitboard << 10) & not_AB_file) attacks |= (bitboard << 10);
	if ((bitboard << 6) & not_HG_file) attacks |= (bitboard << 6);
	
	return attacks;
}

U64 mask_king_attacks(int square)
{
	U64 bitboard = 0ULL;
	U64 attacks = 0ULL;
	
	set_bit(bitboard, square);

	if ((bitboard >> 1) & not_H_file) attacks |= (bitboard >> 1);
	if ((bitboard >> 7) & not_A_file) attacks |= (bitboard >> 7);
	if (bitboard >> 8) attacks |= (bitboard >> 8);
	if ((bitboard >> 9) & not_H_file) attacks |= (bitboard >> 9);
	if ((bitboard << 1) & not_A_file) attacks |= (bitboard << 1);
	if ((bitboard << 7) & not_H_file) attacks |= (bitboard << 7);
	if (bitboard << 8) attacks |= (bitboard << 8);
	if ((bitboard << 9) & not_A_file) attacks |= (bitboard << 9);
	
	return attacks;
}

U64 mask_bishop_attacks(int square)
{
	U64 attacks = 0ULL;
	
	int r, f;
	
	int tr = square / 8;
	int tf = square % 8;
	
	for (r = tr + 1, f = tf + 1; r <= 6 && f <= 6; r++, f++) attacks |= (1ULL << (r * 8 + f));
	for (r = tr - 1, f = tf + 1; r >= 1 && f <= 6; r--, f++) attacks |= (1ULL << (r * 8 + f));
	for (r = tr + 1, f = tf - 1; r <= 6 && f >= 1; r++, f--) attacks |= (1ULL << (r * 8 + f));
	for (r = tr - 1, f = tf - 1; r >= 1 && f >= 1; r--, f--) attacks |= (1ULL << (r * 8 + f));
	
	return attacks;
}

U64 mask_rook_attacks(int square)
{
	U64 attacks = 0ULL;
	
	int r, f;
	
	int tr = square / 8;
	int tf = square % 8;
	
	for (r = tr + 1; r <= 6; r++) attacks |= (1ULL << (r * 8 + tf));
	for (r = tr - 1; r >= 1; r--) attacks |= (1ULL << (r * 8 + tf));
	for (f = tf + 1; f <= 6; f++) attacks |= (1ULL << (tr * 8 + f));
	for (f = tf - 1; f >= 1; f--) attacks |= (1ULL << (tr * 8 + f));
	
	return attacks;
}

void print_bitboard(U64 bitboard)
{
	printf("\n");	
	
	for (int rank = 0; rank < 8; rank++)
	{
		for (int file = 0; file < 8; file++)
		{
			int square = rank * 8 + file;
			
			if (!file)
			{
				printf(" %d", 8 - rank);
			}
			
			printf(" %d", get_bit(bitboard, square) ? 1 : 0);
		}
		
		printf("\n");
	}
	
	printf("\n   a b c d e f g h \n\n");
	
	printf("   Bitboard: %llud\n\n", bitboard);
}

void init_leaper_attacks()
{
	for (int square = 0; square < 64; square++)
	{
		pawn_attacks[white][square] = mask_pawn_attacks(square, white);
		pawn_attacks[black][square] = mask_pawn_attacks(square, black);
		
		knight_attacks[square] = mask_knight_attacks(square);
		king_attacks[square] = mask_king_attacks(square);
	}
}

int main()
{
	init_leaper_attacks();

	for (int square = 0; square < 64; square++)
		print_bitboard(mask_rook_attacks(square));
	
	return 0;
}

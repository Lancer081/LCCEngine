#include <stdio.h>
#include <string.h>

#define U64 unsigned long long

#define get_bit(bitboard, bit) (bitboard & (1ULL << bit))
#define set_bit(bitboard, bit) (bitboard |= (1ULL << bit))
#define pop_bit(bitboard, bit) (get_bit(bitboard, bit) ? bitboard ^= (1ULL << bit) : 0)

#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "

enum {
	a8, b8, c8, d8, e8, f8, g8, h8,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a1, b1, c1, d1, e1, f1, g1, h1, no_sqr
};

enum { white, black, both };

enum { rook, bishop };

enum { all_moves, only_captures };

enum { wk = 1, wq = 2, bk = 4, bq = 8 };

enum { P, N, B, R, Q, K, p, n, b, r, q, k };

typedef struct {
	int moves[256];
	int count;
} move_list;

char promoted_pieces[] = {
	[Q] = 'q',
	[R] = 'r',
	[B] = 'b',
	[N] = 'n',
	[q] = 'q',
	[r] = 'r',
	[b] = 'b',
	[n] = 'n'
};

const char* square_to_coords[] = {
	"a8", "b8", "c8", "d8", "e8", "f8", "g8", "h8",
	"a7", "b7", "c7", "d7", "e7", "f7", "g7", "h7",
	"a6", "b6", "c6", "d6", "e6", "f6", "g6", "h6",
	"a5", "b5", "c5", "d5", "e5", "f5", "g5", "h5",
	"a4", "b4", "c4", "d4", "e4", "f4", "g4", "h4",
	"a3", "b3", "c3", "d3", "e3", "f3", "g3", "h3",
	"a2", "b2", "c2", "d2", "e2", "f2", "g2", "h2",
	"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1"
};

char ascii_pieces[12] = "PNBRQKpnbrqk";

int char_pieces[] = {
	['P'] = P,
	['N'] = N,
	['B'] = B,
	['R'] = R,
	['Q'] = Q,
	['K'] = K,
	['p'] = p,
	['n'] = n,
	['b'] = b,
	['r'] = r,
	['q'] = q,
	['k'] = k
};

const U64 not_A_file = 18374403900871474942ULL;
const U64 not_H_file = 9187201950435737471ULL;
const U64 not_HG_file = 4557430888798830399ULL;
const U64 not_AB_file = 18229723555195321596ULL;

// bishop relevant occupancy bit count for every square on board
const int bishop_relevant_bits[64] = {
	6, 5, 5, 5, 5, 5, 5, 6,
	5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 7, 7, 7, 7, 5, 5,
	5, 5, 7, 9, 9, 7, 5, 5,
	5, 5, 7, 9, 9, 7, 5, 5,
	5, 5, 7, 7, 7, 7, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 5, 5, 5, 5, 5, 5, 6
};

// rook relevant occupancy bit count for every square on board
const int rook_relevant_bits[64] = {
	12, 11, 11, 11, 11, 11, 11, 12,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	11, 10, 10, 10, 10, 10, 10, 11,
	12, 11, 11, 11, 11, 11, 11, 12
};

// rook magic numbers
U64 rook_magic_numbers[64] = {
	0x8a80104000800020ULL,
	0x140002000100040ULL,
	0x2801880a0017001ULL,
	0x100081001000420ULL,
	0x200020010080420ULL,
	0x3001c0002010008ULL,
	0x8480008002000100ULL,
	0x2080088004402900ULL,
	0x800098204000ULL,
	0x2024401000200040ULL,
	0x100802000801000ULL,
	0x120800800801000ULL,
	0x208808088000400ULL,
	0x2802200800400ULL,
	0x2200800100020080ULL,
	0x801000060821100ULL,
	0x80044006422000ULL,
	0x100808020004000ULL,
	0x12108a0010204200ULL,
	0x140848010000802ULL,
	0x481828014002800ULL,
	0x8094004002004100ULL,
	0x4010040010010802ULL,
	0x20008806104ULL,
	0x100400080208000ULL,
	0x2040002120081000ULL,
	0x21200680100081ULL,
	0x20100080080080ULL,
	0x2000a00200410ULL,
	0x20080800400ULL,
	0x80088400100102ULL,
	0x80004600042881ULL,
	0x4040008040800020ULL,
	0x440003000200801ULL,
	0x4200011004500ULL,
	0x188020010100100ULL,
	0x14800401802800ULL,
	0x2080040080800200ULL,
	0x124080204001001ULL,
	0x200046502000484ULL,
	0x480400080088020ULL,
	0x1000422010034000ULL,
	0x30200100110040ULL,
	0x100021010009ULL,
	0x2002080100110004ULL,
	0x202008004008002ULL,
	0x20020004010100ULL,
	0x2048440040820001ULL,
	0x101002200408200ULL,
	0x40802000401080ULL,
	0x4008142004410100ULL,
	0x2060820c0120200ULL,
	0x1001004080100ULL,
	0x20c020080040080ULL,
	0x2935610830022400ULL,
	0x44440041009200ULL,
	0x280001040802101ULL,
	0x2100190040002085ULL,
	0x80c0084100102001ULL,
	0x4024081001000421ULL,
	0x20030a0244872ULL,
	0x12001008414402ULL,
	0x2006104900a0804ULL,
	0x1004081002402ULL
};

// bishop magic numbers
U64 bishop_magic_numbers[64] = {
	0x40040844404084ULL,
	0x2004208a004208ULL,
	0x10190041080202ULL,
	0x108060845042010ULL,
	0x581104180800210ULL,
	0x2112080446200010ULL,
	0x1080820820060210ULL,
	0x3c0808410220200ULL,
	0x4050404440404ULL,
	0x21001420088ULL,
	0x24d0080801082102ULL,
	0x1020a0a020400ULL,
	0x40308200402ULL,
	0x4011002100800ULL,
	0x401484104104005ULL,
	0x801010402020200ULL,
	0x400210c3880100ULL,
	0x404022024108200ULL,
	0x810018200204102ULL,
	0x4002801a02003ULL,
	0x85040820080400ULL,
	0x810102c808880400ULL,
	0xe900410884800ULL,
	0x8002020480840102ULL,
	0x220200865090201ULL,
	0x2010100a02021202ULL,
	0x152048408022401ULL,
	0x20080002081110ULL,
	0x4001001021004000ULL,
	0x800040400a011002ULL,
	0xe4004081011002ULL,
	0x1c004001012080ULL,
	0x8004200962a00220ULL,
	0x8422100208500202ULL,
	0x2000402200300c08ULL,
	0x8646020080080080ULL,
	0x80020a0200100808ULL,
	0x2010004880111000ULL,
	0x623000a080011400ULL,
	0x42008c0340209202ULL,
	0x209188240001000ULL,
	0x400408a884001800ULL,
	0x110400a6080400ULL,
	0x1840060a44020800ULL,
	0x90080104000041ULL,
	0x201011000808101ULL,
	0x1a2208080504f080ULL,
	0x8012020600211212ULL,
	0x500861011240000ULL,
	0x180806108200800ULL,
	0x4000020e01040044ULL,
	0x300000261044000aULL,
	0x802241102020002ULL,
	0x20906061210001ULL,
	0x5a84841004010310ULL,
	0x4010801011c04ULL,
	0xa010109502200ULL,
	0x4a02012000ULL,
	0x500201010098b028ULL,
	0x8040002811040900ULL,
	0x28000010020204ULL,
	0x6000020202d0240ULL,
	0x8918844842082200ULL,
	0x4010011029020020ULL
};

U64 pawn_attacks[2][64];
U64 knight_attacks[64];
U64 king_attacks[64];
U64 bishop_attacks[64][512];
U64 rook_attacks[64][4096];

U64 rook_masks[64];
U64 bishop_masks[64];

U64 board[12];
U64 occupancy[3];

int side = -1;

int enpassant = no_sqr;

int castle;

static inline int count_bits(U64 bitboard)
{
	int count = 0;

	while (bitboard)
	{
		count++;

		bitboard &= bitboard - 1;
	}

	return count;
}

static inline int get_lsb(U64 bitboard)
{
	if (bitboard)
		return count_bits((bitboard & -bitboard) - 1);
	else
		return -1;
}

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

U64 bishop_attacks_otf(int square, U64 blocks)
{
	U64 attacks = 0ULL;

	int r, f;

	int tr = square / 8;
	int tf = square % 8;

	for (r = tr + 1, f = tf + 1; r <= 7 && f <= 7; r++, f++)
	{
		attacks |= (1ULL << (r * 8 + f));
		if ((1ULL << (r * 8 + f)) & blocks) break;
	}
	for (r = tr - 1, f = tf + 1; r >= 0 && f <= 7; r--, f++)
	{
		attacks |= (1ULL << (r * 8 + f));
		if ((1ULL << (r * 8 + f)) & blocks) break;
	}
	for (r = tr + 1, f = tf - 1; r <= 7 && f >= 0; r++, f--)
	{
		attacks |= (1ULL << (r * 8 + f));
		if ((1ULL << (r * 8 + f)) & blocks) break;
	}
	for (r = tr - 1, f = tf - 1; r >= 0 && f >= 0; r--, f--)
	{
		attacks |= (1ULL << (r * 8 + f));
		if ((1ULL << (r * 8 + f)) & blocks) break;
	}

	return attacks;
}

U64 rook_attacks_otf(int square, U64 blocks)
{
	U64 attacks = 0ULL;

	int r, f;

	int tr = square / 8;
	int tf = square % 8;

	for (r = tr + 1; r <= 7; r++)
	{
		attacks |= (1ULL << (r * 8 + tf));
		if ((1ULL << (r * 8 + tf)) & blocks) break;
	}
	for (r = tr - 1; r >= 0; r--)
	{
		attacks |= (1ULL << (r * 8 + tf));
		if ((1ULL << (r * 8 + tf)) & blocks) break;
	}
	for (f = tf + 1; f <= 7; f++)
	{
		attacks |= (1ULL << (tr * 8 + f));
		if ((1ULL << (tr * 8 + f)) & blocks) break;
	}
	for (f = tf - 1; f >= 0; f--)
	{
		attacks |= (1ULL << (tr * 8 + f));
		if ((1ULL << (tr * 8 + f)) & blocks) break;
	}

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
				printf(" %d", 8 - rank);

			printf(" %d", get_bit(bitboard, square) ? 1 : 0);
		}

		printf("\n");
	}

	printf("\n   a b c d e f g h \n\n");

	printf("   Bitboard: %llud\n\n", bitboard);
}

void print_board()
{
	printf("\n");

	for (int rank = 0; rank < 8; rank++)
	{
		for (int file = 0; file < 8; file++)
		{
			int square = rank * 8 + file;

			if (!file)
				printf(" %d", 8 - rank);

			int target_piece = -1;

			for (int piece = P; piece <= k; piece++)
			{
				if (get_bit(board[piece], square))
					target_piece = piece;
			}

			printf(" %c", (target_piece == -1) ? '.' : ascii_pieces[target_piece]);
		}

		printf("\n");
	}

	printf("\n   a b c d e f g h \n\n");

	printf("    Side: %s\n", (side == white) ? "white" : "black");
	printf("    Enpassant: %s\n", (enpassant != no_sqr) ? square_to_coords[enpassant] : "no");
	printf("    Castling: %c%c%c%c\n\n", (castle & wk) ? 'K' : '-', (castle & wq) ? 'Q' : '-', (castle & bk) ? 'k' : '-', (castle & bk) ? 'q' : '-');
}

void parse_fen(char* fen)
{
	memset(board, 0ULL, sizeof(board));
	memset(occupancy, 0ULL, sizeof(occupancy));

	side = 0;
	enpassant = no_sqr;
	castle = 0;

	for (int rank = 0; rank < 8; rank++)
	{
		for (int file = 0; file < 8; file++)
		{
			int square = rank * 8 + file;

			if ((*fen >= 'a' && *fen <= 'z') || (*fen >= 'A' && *fen <= 'Z'))
			{
				int piece = char_pieces[*fen];

				set_bit(board[piece], square);

				fen++;
			}

			if (*fen >= '0' && *fen <= '9')
			{
				int offset = *fen - '0';

				int target_piece = -1;

				for (int piece = P; piece <= k; piece++)
				{
					if (get_bit(board[piece], square))
						target_piece = piece;
				}

				if (target_piece == -1)
					file--;

				file += offset;

				fen++;
			}

			if (*fen == '/')
				*fen++;
		}
	}

	// go to side to move
	fen++;

	(*fen == 'w') ? (side = white) : (side = black);

	// go to castling rights
	fen += 2;

	while (*fen != ' ')
	{
		switch (*fen)
		{
		case 'K':
			castle |= wk;
			break;
		case 'Q':
			castle |= wq;
			break;
		case 'k':
			castle |= bk;
			break;
		case 'q':
			castle |= bq;
			break;
		case '-':
			break;
		}

		fen++;
	}

	// go to enpassant square
	fen++;

	if (*fen == '-')
	{
		int file = fen[0] - 'a';
		int rank = 8 - (fen[1] - '0');

		enpassant = rank * 8 + file;
	}
	else
		enpassant = no_sqr;

	for (int piece = P; piece <= K; piece++)
		occupancy[white] |= board[piece];

	for (int piece = p; piece <= k; piece++)
		occupancy[black] |= board[piece];

	occupancy[both] |= occupancy[white];
	occupancy[both] |= occupancy[black];
}

U64 set_occupancy(int index, int bits_in_mask, U64 attack_mask)
{
	U64 occupancy = 0ULL;

	for (int count = 0; count < bits_in_mask; count++)
	{
		int square = get_lsb(attack_mask);

		// make sure occupancy is on board
		if (index & (1 << count))
			occupancy |= (1ULL << square);

		pop_bit(attack_mask, square);
	}

	return occupancy;
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

void init_slider_attacks(int bishop)
{
	for (int square = 0; square < 64; square++)
	{
		bishop_masks[square] = mask_bishop_attacks(square);
		rook_masks[square] = mask_rook_attacks(square);

		U64 attack_mask = bishop ? bishop_masks[square] : rook_masks[square];

		int relevant_bits = count_bits(attack_mask);

		int occupancy_indices = (1 << relevant_bits);

		for (int index = 0; index < occupancy_indices; index++)
		{
			if (bishop)
			{
				U64 occupancy = set_occupancy(index, relevant_bits, attack_mask);
				int magic_index = (occupancy * bishop_magic_numbers[square]) >> (64 - bishop_relevant_bits[square]);
				bishop_attacks[square][magic_index] = bishop_attacks_otf(square, occupancy);
			}
			else
			{
				U64 occupancy = set_occupancy(index, relevant_bits, attack_mask);
				int magic_index = (occupancy * rook_magic_numbers[square]) >> (64 - rook_relevant_bits[square]);
				rook_attacks[square][magic_index] = rook_attacks_otf(square, occupancy);
			}
		}
	}
}

static inline U64 get_bishop_attacks(int square, U64 occupancy)
{
	occupancy &= bishop_masks[square];
	occupancy *= bishop_magic_numbers[square];
	occupancy >>= 64 - bishop_relevant_bits[square];

	return bishop_attacks[square][occupancy];
}

static inline U64 get_rook_attacks(int square, U64 occupancy)
{
	occupancy &= rook_masks[square];
	occupancy *= rook_magic_numbers[square];
	occupancy >>= 64 - rook_relevant_bits[square];

	return rook_attacks[square][occupancy];
}

static inline U64 get_queen_attacks(int square, U64 occupancy)
{
	U64 attacks = 0ULL;

	attacks |= get_bishop_attacks(square, occupancy);
	attacks |= get_rook_attacks(square, occupancy);

	return attacks;
}

static inline int is_square_attacked(int square, int side)
{
	if ((side == white) && (pawn_attacks[black][square] & board[P]))
		return 1;
	if ((side == black) && (pawn_attacks[white][square] & board[p]))
		return 1;

	if (knight_attacks[square] & ((side == white) ? board[N] : board[n]))
		return 1;

	if (king_attacks[square] & ((side == white) ? board[K] : board[k]))
		return 1;

	if (get_bishop_attacks(square, occupancy[both]) & ((side == white) ? board[B] : board[b]))
		return 1;

	if (get_rook_attacks(square, occupancy[both]) & ((side == white) ? board[R] : board[r]))
		return 1;

	if (get_bishop_attacks(square, occupancy[both]) & ((side == white) ? board[B] : board[b]))
		return 1;

	if (get_queen_attacks(square, occupancy[both]) & ((side == white) ? board[Q] : board[q]))
		return 1;

	return 0;
}

// encode move
#define encode_move(source, target, piece, promoted, capture, double, enpassant, castling) \
    (source) |          \
    (target << 6) |     \
    (piece << 12) |     \
    (promoted << 16) |  \
    (capture << 20) |   \
    (double << 21) |    \
    (enpassant << 22) | \
    (castling << 23)    \

#define get_move_source(move) (move & 0x3f)
#define get_move_target(move) ((move & 0xfc0) >> 6)
#define get_move_piece(move) ((move & 0xf000) >> 12)
#define get_move_promoted(move) ((move & 0xf0000) >> 16)
#define get_move_capture(move) (move & 0x100000)
#define get_move_double(move) (move & 0x200000)
#define get_move_enpassant(move) (move & 0x400000)
#define get_move_castling(move) (move & 0x800000)

// preserve board state
#define copy_board()                                                      \
    U64 board_copy[12], occupancy_copy[3];								  \
    int side_copy, enpassant_copy, castle_copy;                           \
    memcpy(board_copy, board, 96);										  \
    memcpy(occupancy_copy, occupancy, 24);                                \
    side_copy = side, enpassant_copy = enpassant, castle_copy = castle;   \

// restore board state
#define take_back()                                                       \
    memcpy(board, board_copy, 96);										  \
    memcpy(occupancy, occupancy_copy, 24);                                \
    side = side_copy, enpassant = enpassant_copy, castle = castle_copy;   \

void print_move(int move)
{
	printf("%s%s%c", square_to_coords[get_move_source(move)], 
					 square_to_coords[get_move_target(move)], 
					 promoted_pieces[get_move_promoted(move)]);
}

static inline void add_move(move_list* moves, int move)
{
	moves->moves[moves->count] = move;
	moves->count++;
}

static inline int make_move(int move, int move_flag)
{
	if (move_flag == all_moves)
	{
		copy_board();

		int from_square = get_move_source(move);
		int to_square = get_move_target(move);
		int piece = get_move_piece(move);
		int promoted = get_move_promoted(move);
		int capture = get_move_capture(move);
		int doublePush = get_move_double(move);
		int enpass = get_move_enpassant(move);
		int castling = get_move_castling(move);

		pop_bit(board[piece], from_square);
		set_bit(board[piece], to_square);

		if (capture)
		{
			int start_piece, end_piece;

			if (side == white)
			{
				start_piece = p;
				end_piece = k;
			}
			else
			{
				start_piece = P;
				end_piece = K;
			}

			// loop over bitboards opposite to the current side
			for (int p = start_piece; p <= end_piece; p++)
			{
				if (get_bit(board[piece], to_square))
				{
					// removes captured piece
					pop_bit(board[piece], to_square);
					break;
				}
			}
		}

		if (promoted)
		{
			pop_bit(board[(side == white) ? P : p], to_square);
			set_bit(board[promoted], to_square);
		}

		if (enpass)
			(side == white) ? pop_bit(board[p], to_square + 8) : pop_bit(board[P], to_square - 8);

		enpassant = no_sqr;

		if (doublePush)
			(side == white) ? (enpassant = to_square + 8) : (enpassant = to_square - 8);

		if (castling)
		{
			switch (to_square)
			{
			case g1:
				pop_bit(board[R], h1);
				set_bit(board[R], f1);
				break;
			case c1:
				pop_bit(board[R], a1);
				set_bit(board[R], d1);
				break;
			case g8:
				pop_bit(board[r], h8);
				set_bit(board[r], f8);
				break;
			case c8:
				pop_bit(board[r], a8);
				set_bit(board[r], d8);
				break;
			}
		}
	}
	else
	{
		if (get_move_capture(move))
			make_move(move, all_moves);
		else
			return 0;
	}
}

static inline void generate_moves(move_list* moves)
{
	int from_square, to_square;

	U64 bitboard, attacks;

	moves->count = 0;

	for (int piece = P; piece <= k; piece++)
	{
		bitboard = board[piece];

		if (side == white)
		{
			if (piece == P)
			{
				while (bitboard)
				{
					from_square = get_lsb(bitboard);
					to_square = from_square - 8;
					
					// generate quiet pawn moves
					if (!(to_square < a8) && !get_bit(occupancy[both], to_square))
					{
						// promotion
						if (from_square >= a7 && from_square <= h7)
						{
							add_move(moves, encode_move(from_square, to_square, piece, Q, 0, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, R, 0, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, B, 0, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, N, 0, 0, 0, 0));
						}
						else
						{
							// single pawn push
							add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 0));

							// double pawn push
							if ((from_square >= a2 && from_square <= h2) && !get_bit(occupancy[both], to_square - 8))
								add_move(moves, encode_move(from_square, to_square - 8, piece, 0, 0, 1, 0, 0));
						}
					}

					attacks = pawn_attacks[side][from_square] & occupancy[black];

					// generate pawn captures
					while (attacks)
					{
						to_square = get_lsb(attacks);

						if (from_square >= a7 && to_square <= h7)
						{
							add_move(moves, encode_move(from_square, to_square, piece, Q, 1, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, R, 1, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, B, 1, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, N, 1, 0, 0, 0));
						}
						else
							add_move(moves, encode_move(from_square, to_square, piece, 0, 1, 0, 0, 0));

						pop_bit(attacks, to_square);
					}

					// generate enpassant captures
					if (enpassant != no_sqr)
					{
						U64 enpassant_attacks = pawn_attacks[side][from_square] & (1ULL << enpassant);

						if (enpassant_attacks)
						{
							int target_enpassant = get_lsb(enpassant_attacks);
							add_move(moves, encode_move(from_square, target_enpassant, piece, 0, 1, 0, 1, 0));
						}
					}

					pop_bit(bitboard, from_square);
				}
			}
			else if (piece == K)
			{
				if (castle & wk)
				{
					if (!get_bit(occupancy[both], f1) && !get_bit(occupancy[both], g1))
					{
						if (!is_square_attacked(e1, black) && !is_square_attacked(f1, black))
							add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 1));
					}
				}
				else if (castle & wq)
				{
					if (!get_bit(occupancy[both], d1) && !get_bit(occupancy[both], c1) && !get_bit(occupancy[both], b1))
					{
						if (!is_square_attacked(e1, black) && !is_square_attacked(d1, black))
							add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 1));
					}
				}
			}
		}
		else
		{
			if (piece == p)
			{
				while (bitboard)
				{
					from_square = get_lsb(bitboard);
					to_square = from_square + 8;


					// generate quiet pawn moves
					if (!(to_square > h1) && !get_bit(occupancy[both], to_square))
					{
						if (from_square >= a2 && from_square <= h2)
						{
							add_move(moves, encode_move(from_square, to_square, piece, q, 0, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, r, 0, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, b, 0, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, n, 0, 0, 0, 0));
						}
						else
						{
							// generate single pawn pushes
							add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 0));

							// generate double pawn pushes
							if ((from_square >= a7 && from_square <= h7) && !get_bit(occupancy[both], to_square + 8))
								add_move(moves, encode_move(from_square, to_square + 8, piece, n, 0, 1, 0, 0));
						}
					}

					attacks = pawn_attacks[side][from_square] & occupancy[white];

					// generate pawn captures
					while (attacks)
					{
						to_square = get_lsb(attacks);

						if (from_square >= a2 && to_square <= h2)
						{
							add_move(moves, encode_move(from_square, to_square, piece, q, 1, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, r, 1, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, b, 1, 0, 0, 0));
							add_move(moves, encode_move(from_square, to_square, piece, n, 1, 0, 0, 0));
						}
						else
							add_move(moves, encode_move(from_square, to_square, piece, 0, 1, 0, 0, 0));

						pop_bit(attacks, to_square);
					}

					// generate enpassant captures
					if (enpassant != no_sqr)
					{
						U64 enpassant_attacks = pawn_attacks[side][from_square] & (1ULL << enpassant);

						if (enpassant_attacks)
						{
							int target_enpassant = get_lsb(enpassant_attacks);
							add_move(moves, encode_move(from_square, target_enpassant, piece, 0, 1, 0, 1, 0));
						}
					}

					pop_bit(bitboard, from_square);
				}
			}
			else if (piece == k)
			{
				if (castle & bk)
				{
					if (!get_bit(occupancy[both], f8) && !get_bit(occupancy[both], g8))
					{
						if (!is_square_attacked(e8, black) && !is_square_attacked(f8, black))
							add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 1));
					}
				}
				else if (castle & bq)
				{
					if (!get_bit(occupancy[both], d8) && !get_bit(occupancy[both], c8) && !get_bit(occupancy[both], b8))
					{
						if (!is_square_attacked(e8, black) && !is_square_attacked(d8, black))
						{
							add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 1));
						}
					}
				}
			}
		}

		// generate knight moves
		if ((side == white) ? piece == N : piece == n)
		{
			while (bitboard)
			{
				to_square = get_lsb(bitboard);

				attacks = knight_attacks[from_square] & ((side == white) ? ~occupancy[white] : ~occupancy[black]);

				while (attacks)
				{
					to_square = get_lsb(attacks);

					// quiet moves
					if (!get_bit(((side == white) ? occupancy[black] : occupancy[white]), to_square))
						add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 0));
					// capture moves
					else
						add_move(moves, encode_move(from_square, to_square, piece, 0, 1, 0, 0, 0));

					pop_bit(attacks, to_square);
				}

				pop_bit(bitboard, from_square);
			}
		}

		// generate bishop moves
		if ((side == white) ? piece == B : piece == b)
		{
			while (bitboard)
			{
				to_square = get_lsb(bitboard);

				attacks = get_bishop_attacks(from_square, occupancy[both]) & ((side == white) ? ~occupancy[white] : ~occupancy[black]);

				while (attacks)
				{
					to_square = get_lsb(attacks);

					// quiet moves
					if (!get_bit(((side == white) ? occupancy[black] : occupancy[white]), to_square))
						add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 0));
					// capture moves
					else
						add_move(moves, encode_move(from_square, to_square, piece, 0, 1, 0, 0, 0));

					pop_bit(attacks, to_square);
				}

				pop_bit(bitboard, from_square);
			}
		}
		
		// generate rook moves
		if ((side == white) ? piece == R : piece == r)
		{
			while (bitboard)
			{
				from_square = get_lsb(bitboard);

				attacks = get_rook_attacks(from_square, occupancy[both]) & ((side == white) ? ~occupancy[white] : ~occupancy[black]);

				while (attacks)
				{
					to_square = get_lsb(attacks);

					// quiet moves
					if (!get_bit(((side == white) ? occupancy[black] : occupancy[white]), to_square))
						add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 0));
					// capture moves
					else
						add_move(moves, encode_move(from_square, to_square, piece, 0, 1, 0, 0, 0));

					pop_bit(attacks, to_square);
				}

				pop_bit(bitboard, from_square);
			}
		}

		// generate queen moves
		if ((side == white) ? piece == Q : piece == q)
		{
			while (bitboard)
			{
				to_square = get_lsb(bitboard);

				attacks = get_queen_attacks(from_square, occupancy[both]) & ((side == white) ? ~occupancy[white] : ~occupancy[black]);

				while (attacks)
				{
					to_square = get_lsb(attacks);

					// quiet moves
					if (!get_bit(((side == white) ? occupancy[black] : occupancy[white]), to_square))
						add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 0));
					// capture moves
					else
						add_move(moves, encode_move(from_square, to_square, piece, 0, 1, 0, 0, 0));

					pop_bit(attacks, to_square);
				}

				pop_bit(bitboard, from_square);
			}
		}

		// generate king moves
		if ((side == white) ? piece == K : piece == k)
		{
			while (bitboard)
			{
				to_square = get_lsb(bitboard);

				attacks = king_attacks[from_square] & ((side == white) ? ~occupancy[white] : ~occupancy[black]);

				while (attacks)
				{
					to_square = get_lsb(attacks);

					// quiet moves
					if (!get_bit(((side == white) ? occupancy[black] : occupancy[white]), to_square))
						add_move(moves, encode_move(from_square, to_square, piece, 0, 0, 0, 0, 0));
					// capture moves
					else
						add_move(moves, encode_move(from_square, to_square, piece, 0, 1, 0, 0, 0));

					pop_bit(attacks, to_square);
				}

				pop_bit(bitboard, from_square);
			}
		}
	}
}

unsigned int state = 1804289383;

unsigned int get_random_number()
{
	unsigned int num = state;

	num ^= num << 13;
	num ^= num >> 17;
	num ^= num << 5;

	return num;
}

void init_all()
{
	init_leaper_attacks();
	init_slider_attacks(bishop);
	init_slider_attacks(rook);
}

int main()
{
	init_all();

	parse_fen(tricky_position);
	print_board();

	move_list moves[1];

	generate_moves(moves);

	for (int i = 0; i < moves->count; i++)
	{
		int move = moves->moves[i];

		copy_board();

		make_move(move, all_moves);
		print_board();
		getchar();

		take_back();
		print_board();
		getchar();
	}

	return 0;
}

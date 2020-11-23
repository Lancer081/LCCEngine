#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define U64 unsigned long long

#define get_bit(bitboard, bit) (bitboard & (1ULL << bit))
#define set_bit(bitboard, bit) (bitboard |= (1ULL << bit))
#define pop_bit(bitboard, bit) (get_bit(bitboard, bit) ? bitboard ^= (1ULL << bit) : 0)

#define start_position "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1 "
#define tricky_position "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1 "
#define killer_position "rnbqkb1r/pp1p1pPp/8/2p1pP2/1P1P4/3P3P/P1P1P3/RNBQKBNR w KQkq e6 0 1"
#define cmk_position "r2q1rk1/ppp2ppp/2n1bn2/2b1p3/3pP3/3P1NPP/PPP1NPB1/R1BQ1RK1 b - - 0 9 "

#define hash_size 0x400000
#define no_hash_entry 100000

#define hash_flag_exact 0
#define hash_flag_alpha 1
#define hash_flag_beta 2

#define mate_value 49000
#define mate_score 48000

#define max_ply 64

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
    U64 hash_key_copy = hash_key;									      \

// restore board state
#define take_back()                                                       \
    memcpy(board, board_copy, 96);										  \
    memcpy(occupancy, occupancy_copy, 24);                                \
    side = side_copy, enpassant = enpassant_copy, castle = castle_copy;   \
    hash_key = hash_key_copy											  \

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

typedef struct {
	U64 hash_key;
	int depth;
	int flag;
	int score;
} tt;

tt transpos_table[hash_size];

const int INF = 50000;

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

int material_score[12] = {
	100,
	300,
	350,
	500,
	1000,
	10000,
	-100,
	-300,
	-350,
	-500,
	-1000,
	-10000
};


// pawn positional score
const int pawn_score[64] =
{
	90,  90,  90,  90,  90,  90,  90,  90,
	30,  30,  30,  40,  40,  30,  30,  30,
	20,  20,  20,  30,  30,  30,  20,  20,
	10,  10,  10,  20,  20,  10,  10,  10,
	 5,   5,  10,  20,  20,   5,   5,   5,
	 0,   0,   0,   5,   5,   0,   0,   0,
	 0,   0,   0, -10, -10,   0,   0,   0,
	 0,   0,   0,   0,   0,   0,   0,   0
};

// knight positional score
const int knight_score[64] =
{
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5,   0,   0,  10,  10,   0,   0,  -5,
	-5,   5,  20,  20,  20,  20,   5,  -5,
	-5,  10,  20,  30,  30,  20,  10,  -5,
	-5,  10,  20,  30,  30,  20,  10,  -5,
	-5,   5,  20,  10,  10,  20,   5,  -5,
	-5,   0,   0,   0,   0,   0,   0,  -5,
	-5, -10,   0,   0,   0,   0, -10,  -5
};

// bishop positional score
const int bishop_score[64] =
{
	 0,   0,   0,   0,   0,   0,   0,   0,
	 0,   0,   0,   0,   0,   0,   0,   0,
	 0,   0,   0,  10,  10,   0,   0,   0,
	 0,   0,  10,  20,  20,  10,   0,   0,
	 0,   0,  10,  20,  20,  10,   0,   0,
	 0,  10,   0,   0,   0,   0,  10,   0,
	 0,  30,   0,   0,   0,   0,  30,   0,
	 0,   0, -10,   0,   0, -10,   0,   0

};

// rook positional score
const int rook_score[64] =
{
	50,  50,  50,  50,  50,  50,  50,  50,
	50,  50,  50,  50,  50,  50,  50,  50,
	 0,   0,  10,  20,  20,  10,   0,   0,
	 0,   0,  10,  20,  20,  10,   0,   0,
	 0,   0,  10,  20,  20,  10,   0,   0,
	 0,   0,  10,  20,  20,  10,   0,   0,
	 0,   0,  10,  20,  20,  10,   0,   0,
	 0,   0,   0,  20,  20,   0,   0,   0

};

// king positional score
const int king_score[64] =
{
	 0,   0,   0,   0,   0,   0,   0,   0,
	 0,   0,   5,   5,   5,   5,   0,   0,
	 0,   5,   5,  10,  10,   5,   5,   0,
	 0,   5,  10,  20,  20,  10,   5,   0,
	 0,   5,  10,  20,  20,  10,   5,   0,
	 0,   0,   5,  10,  10,   5,   0,   0,
	 0,   5,   5,  -5,  -5,   0,   5,   0,
	 0,   0,   5,   0, -15,   0,  10,   0
};

// mirror positional score tables for opposite side
const int mirror_score[128] =
{
	a1, b1, c1, d1, e1, f1, g1, h1,
	a2, b2, c2, d2, e2, f2, g2, h2,
	a3, b3, c3, d3, e3, f3, g3, h3,
	a4, b4, c4, d4, e4, f4, g4, h4,
	a5, b5, c5, d5, e5, f5, g5, h5,
	a6, b6, c6, d6, e6, f6, g6, h6,
	a7, b7, c7, d7, e7, f7, g7, h7,
	a8, b8, c8, d8, e8, f8, g8, h8
};

// MVV LVA [attacker][victim]
static int mvv_lva[12][12] = {
	105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
	104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
	103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
	102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
	101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
	100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600,

	105, 205, 305, 405, 505, 605,  105, 205, 305, 405, 505, 605,
	104, 204, 304, 404, 504, 604,  104, 204, 304, 404, 504, 604,
	103, 203, 303, 403, 503, 603,  103, 203, 303, 403, 503, 603,
	102, 202, 302, 402, 502, 602,  102, 202, 302, 402, 502, 602,
	101, 201, 301, 401, 501, 601,  101, 201, 301, 401, 501, 601,
	100, 200, 300, 400, 500, 600,  100, 200, 300, 400, 500, 600
};

const U64 not_A_file = 18374403900871474942ULL;
const U64 not_H_file = 9187201950435737471ULL;
const U64 not_HG_file = 4557430888798830399ULL;
const U64 not_AB_file = 18229723555195321596ULL;

// castling rights update constants
const int castling_rights[64] = {
	 7, 15, 15, 15,  3, 15, 15, 11,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	15, 15, 15, 15, 15, 15, 15, 15,
	13, 15, 15, 15, 12, 15, 15, 14
};

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

U64 hash_key;
U64 piece_keys[12][64];
U64 enpassant_keys[64];
U64 castle_keys[16];
U64 side_key;

// repetition table
U64 repetition_table[1000];	

int rep_index = 0;

int killer_moves[2][64];
int history_moves[12][64];

int side = -1;
int enpassant = no_sqr;
int castle;

int ply;

int pv_length[64];
int pv_table[64][64];

int score_pv, follow_pv;

const int full_depth_moves = 4;
const int reduction_limit = 2;

// exit from engine flag
int quit = 0;

// UCI "movestogo" command moves counter
int movestogo = 30;

// UCI "movetime" command time counter
int movetime = -1;

// UCI "time" command holder (ms)
int ttime = -1;

// UCI "inc" command's time increment holder
int inc = 0;

// UCI "starttime" command time holder
int starttime = 0;

// UCI "stoptime" command time holder
int stoptime = 0;

// variable to flag time control availability
int timeset = 0;

// variable to flag when the time is up
int stopped = 0;

int get_time_ms()
{
	return GetTickCount();
}


int input_waiting()
{
	static int init = 0, pipe;
	static HANDLE inh;
	DWORD dw;

	if (!init)
	{
		init = 1;
		inh = GetStdHandle(STD_INPUT_HANDLE);
		pipe = !GetConsoleMode(inh, &dw);
		if (!pipe)
		{
			SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT));
			FlushConsoleInputBuffer(inh);
		}
	}

	if (pipe)
	{
		if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) return 1;
		return dw;
	}

	else
	{
		GetNumberOfConsoleInputEvents(inh, &dw);
		return dw <= 1 ? 0 : dw;
	}
}


// read GUI/user input
void read_input()
{
	// bytes to read holder
	int bytes;

	// GUI/user input
	char input[256] = "", * endc;

	// "listen" to STDIN
	if (input_waiting())
	{
		// tell engine to stop calculating
		stopped = 1;

		// loop to read bytes from STDIN
		do
		{
			// read bytes from STDIN
			bytes = read(fileno(stdin), input, 256);
		}

		// until bytes available
		while (bytes < 0);

		// searches for the first occurrence of '\n'
		endc = strchr(input, '\n');

		// if found new line set value at pointer to 0
		if (endc) *endc = 0;

		// if input is available
		if (strlen(input) > 0)
		{
			// match UCI "quit" command
			if (!strncmp(input, "quit", 4))
			{
				// tell engine to terminate exacution    
				quit = 1;
			}

			// // match UCI "stop" command
			else if (!strncmp(input, "stop", 4)) {
				// tell engine to terminate exacution
				quit = 1;
			}
		}
	}
}

// a bridge function to interact between search and GUI input
static void communicate() {
	// if time is up break here
	if (timeset == 1 && get_time_ms() > stoptime) {
		// tell engine to stop calculating
		stopped = 1;
	}

	// read GUI input
	read_input();
}

unsigned int seed = 1804289383;

unsigned int get_random_number()
{
	unsigned int num = seed;

	num ^= num << 13;
	num ^= num >> 17;
	num ^= num << 5;

	return num;
}

unsigned int get_random_U32_number()
{
	unsigned int number = seed;

	// XOR shift algorithm
	number ^= number << 13;
	number ^= number >> 17;
	number ^= number << 5;

	seed = number;

	return number;
}

U64 get_random_U64_number()
{
	U64 n1, n2, n3, n4;

	// init random numbers slicing 16 bits from MS1B side
	n1 = (U64)(get_random_U32_number()) & 0xFFFF;
	n2 = (U64)(get_random_U32_number()) & 0xFFFF;
	n3 = (U64)(get_random_U32_number()) & 0xFFFF;
	n4 = (U64)(get_random_U32_number()) & 0xFFFF;

	return n1 | (n2 << 16) | (n3 << 32) | (n4 << 48);
}

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
	printf("    Hash key: %llx\n\n", hash_key);
}

U64 generate_hash_key()
{
	U64 final_key = 0ULL;

	U64 bitboard;

	for (int piece = P; piece <= k; piece++)
	{
		bitboard = board[piece];

		while (bitboard)
		{
			int square = get_lsb(bitboard);

			final_key ^= piece_keys[piece][square];

			pop_bit(bitboard, square);
		}
	}

	if (enpassant != no_sqr)
		final_key ^= enpassant_keys[enpassant];

	final_key ^= castle_keys[castle];

	if (side == black) final_key ^= side_key;

	return final_key;
}

void parse_fen(char* fen)
{
	memset(board, 0ULL, sizeof(board));
	memset(occupancy, 0ULL, sizeof(occupancy));

	side = 0;
	enpassant = no_sqr;
	castle = 0;

	hash_key = 0ULL;

	rep_index = 0;

	memset(repetition_table, 0, sizeof(repetition_table));

	ply = 0;

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

	if (*fen != '-')
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

	hash_key = generate_hash_key();
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

void init_random_keys()
{
	seed = 1804289383;

	for (int piece = P; piece <= k; piece++)
		for (int square = 0; square < 64; square++)
			piece_keys[piece][square] = get_random_U64_number();

	for (int square = 0; square < 64; square++)
		enpassant_keys[square] = get_random_U64_number();

	for (int i = 0; i < 16; i++)
		castle_keys[i] = get_random_U64_number();

	side_key = get_random_U64_number();
}

void clear_transpos_table()
{
	for (int i = 0; i < hash_size; i++)
	{
		transpos_table[i].hash_key = 0;
		transpos_table[i].depth = 0;
		transpos_table[i].flag = 0;
		transpos_table[i].score = 0;
	}
}

static inline int read_tt_entry(int depth, int alpha, int beta)
{
	tt* hash_entry = &transpos_table[hash_key % hash_size];

	if (hash_entry->hash_key == hash_key)
	{
		if (hash_entry->depth >= depth)
		{
			int score = hash_entry->score;

			if (score < -48000) score += ply;
			if (score > 48000) score -= ply;

			if (hash_entry->flag == hash_flag_exact)
				return score;

			if (hash_entry->flag == hash_flag_alpha && score <= alpha)
				return alpha;

			if (hash_entry->flag == hash_flag_beta && score >= beta)
				return beta;
		}
	}

	return no_hash_entry;
}

static inline void write_tt_entry(int depth, int score, int hash_flag)
{
	tt* hash_entry = &transpos_table[hash_key % hash_size];

	if (score < -48000) score -= ply;
	if (score > 48000) score += ply;

	hash_entry->hash_key = hash_key;
	hash_entry->score = score;
	hash_entry->flag = hash_flag;
	hash_entry->depth = depth;
}

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

		hash_key ^= piece_keys[piece][from_square];
		hash_key ^= piece_keys[piece][to_square];

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
			for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
			{
				if (get_bit(board[bb_piece], to_square))
				{
					// removes captured piece
					pop_bit(board[bb_piece], to_square);

					hash_key = piece_keys[bb_piece][to_square];
					break;
				}
			}
		}

		if (promoted)
		{
			pop_bit(board[(side == white) ? P : p], to_square);

			if (side == white)
			{
				pop_bit(board[P], to_square);
				hash_key ^= piece_keys[P][to_square];
			}
			else
			{
				pop_bit(board[p], to_square);
				hash_key ^= piece_keys[p][to_square];
			}

			set_bit(board[promoted], to_square);
			hash_key ^= piece_keys[promoted][to_square];
		}

		if (enpass)
		{
			if (side == white)
			{
				pop_bit(board[p], to_square + 8);
				hash_key ^= piece_keys[p][to_square + 8];
			}
			else
			{
				pop_bit(board[P], to_square - 8);
				hash_key ^= piece_keys[P][to_square - 8];
			}
		}
		if (enpassant != no_sqr)
			hash_key ^= enpassant_keys[enpassant];

		enpassant = no_sqr;

		if (doublePush)
		{
			(side == white) ? (enpassant = to_square + 8) : (enpassant = to_square - 8);

			if (side == white)
			{
				enpassant = to_square + 8;
				hash_key ^= enpassant_keys[to_square + 8];
			}
			else
			{
				enpassant = to_square - 8;
				hash_key ^= enpassant_keys[to_square - 8];
			}
		}

		if (castling)
		{
			switch (to_square)
			{
			case g1:
				pop_bit(board[R], h1);
				set_bit(board[R], f1);

				hash_key ^= piece_keys[R][h1];
				hash_key ^= piece_keys[R][f1];
				break;
			case c1:
				pop_bit(board[R], a1);
				set_bit(board[R], d1);

				hash_key ^= piece_keys[R][a1];
				hash_key ^= piece_keys[R][d1];
				break;
			case g8:
				pop_bit(board[r], h8);
				set_bit(board[r], f8);

				hash_key ^= piece_keys[r][h8];
				hash_key ^= piece_keys[r][f8];
				break;
			case c8:
				pop_bit(board[r], a8);
				set_bit(board[r], d8);

				hash_key ^= piece_keys[r][a8];
				hash_key ^= piece_keys[r][d8];
				break;
			}
		}

		hash_key ^= castle_keys[castle];

		// update castling rights
		castle &= castling_rights[from_square];
		castle &= castling_rights[to_square];

		hash_key ^= castle_keys[castle];

		// zero out occupancy
		memset(occupancy, 0ULL, 24);

		// reset occupancy
		for (int bb_piece = P; bb_piece <= K; bb_piece++)
			occupancy[white] |= board[bb_piece];
		for (int bb_piece = p; bb_piece <= k; bb_piece++)
			occupancy[black] |= board[bb_piece];

		occupancy[both] |= occupancy[white];
		occupancy[both] |= occupancy[black];

		side ^= 1;

		hash_key ^= side_key;

		if (is_square_attacked((side == white) ? get_lsb(board[k]) : get_lsb(board[K]), side))
		{
			take_back();
			return 0;
		}
		else
			return 1;
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

						if (from_square >= a7 && from_square <= h7)
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
							add_move(moves, encode_move(e1, g1, piece, 0, 0, 0, 0, 1));
					}
				}
				else if (castle & wq)
				{
					if (!get_bit(occupancy[both], d1) && !get_bit(occupancy[both], c1) && !get_bit(occupancy[both], b1))
					{
						if (!is_square_attacked(e1, black) && !is_square_attacked(d1, black))
							add_move(moves, encode_move(e1, c1, piece, 0, 0, 0, 0, 1));
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
								add_move(moves, encode_move(from_square, to_square + 8, piece, 0, 0, 1, 0, 0));
						}
					}

					attacks = pawn_attacks[side][from_square] & occupancy[white];

					// generate pawn captures
					while (attacks)
					{
						to_square = get_lsb(attacks);

						if (from_square >= a2 && from_square <= h2)
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
							add_move(moves, encode_move(e8, g8, piece, 0, 0, 0, 0, 1));
					}
				}
				else if (castle & bq)
				{
					if (!get_bit(occupancy[both], d8) && !get_bit(occupancy[both], c8) && !get_bit(occupancy[both], b8))
					{
						if (!is_square_attacked(e8, black) && !is_square_attacked(d8, black))
						{
							add_move(moves, encode_move(e8, c8, piece, 0, 0, 0, 0, 1));
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
				from_square = get_lsb(bitboard);

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
				from_square = get_lsb(bitboard);

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
				from_square = get_lsb(bitboard);

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
				from_square = get_lsb(bitboard);

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

long nodes;

static inline void perft(int depth)
{
	if (depth == 0)
	{
		nodes++;
		return;
	}

	move_list moves[1];

	generate_moves(moves);

	for (int i = 0; i < moves->count; i++)
	{
		int move = moves->moves[i];

		copy_board();

		if (!make_move(move, all_moves))
			continue;

		perft(depth - 1);

		take_back();
	}
}

static inline void enable_pv_scoring(move_list* moves)
{
	follow_pv = 0;

	for (int i = 0; i < moves->count; i++)
	{
		// make sure we hit a pv move
		if (pv_table[0][ply] == moves->moves[i])
		{
			score_pv = 1;
			follow_pv = 1;
		}
	}
}

static inline int score_move(int move)
{
	if (score_pv)
	{
		if (pv_table[0][ply] == move)
		{
			score_pv = 0;
			return 20000;
		}
	}

	if (get_move_capture(move))
	{
		int target_piece = P;
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

		for (int bb_piece = start_piece; bb_piece <= end_piece; bb_piece++)
		{
			if (get_bit(board[bb_piece], get_move_target(move)))
			{
				target_piece = bb_piece;
				break;
			}
		}

		return mvv_lva[get_move_piece(move)][target_piece] + 10000;
	}
	else
	{
		if (killer_moves[0][ply] == move)
			return 9000;
		else if (killer_moves[1][ply] == move)
			return 8000;
		else
			return history_moves[get_move_piece(move)][get_move_target(move)];
	}
}

static inline int sort_moves(move_list* moves)
{
	int move_scores[moves->count];

	for (int i = 0; i < moves->count; i++)
		move_scores[i] = score_move(moves->moves[i]);

	for (int current = 0; current < moves->count; current++)
	{
		for (int next = current + 1; next < moves->count; next++)
		{
			if (move_scores[current] < move_scores[next])
			{
				int temp_score = move_scores[current];
				move_scores[current] = move_scores[next];
				move_scores[next] = temp_score;

				int temp_move = moves->moves[current];
				moves->moves[current] = moves->moves[next];
				moves->moves[next] = temp_move;
			}
		}
	}
}

static inline int evaluate()
{
	int score = 0;

	U64 bitboard;

	int piece, square;

	for (int bb_piece = P; bb_piece <= k; bb_piece++)
	{
		bitboard = board[bb_piece];

		while (bitboard)
		{
			piece = bb_piece;
			square = get_lsb(bitboard);

			score += material_score[piece];

			switch (piece)
			{
			case P:
				score += pawn_score[square];
				break;
			case N:
				score += knight_score[square];
				break;
			case B:
				score += bishop_score[square];
				break;
			case R:
				score += rook_score[square];
				break;
			case K:
				score += king_score[square];
				break;

			case p:
				score -= pawn_score[mirror_score[square]];
				break;
			case n:
				score -= knight_score[mirror_score[square]];
				break;
			case b:
				score -= bishop_score[mirror_score[square]];
				break;
			case r:
				score -= rook_score[mirror_score[square]];
				break;
			case k:
				score -= king_score[mirror_score[square]];
				break;
			}

			pop_bit(bitboard, square);
		}
	}

	return (side == white) ? score : -score;
}

static inline int is_repetition()
{
	for (int i = 0; i < rep_index; i++)
	{
		if (repetition_table[i] == hash_key)
			return 1;
	}
	
	return 0;
}

static inline int quiesce(int alpha, int beta)
{
	if ((nodes & 2047) == 0)
		communicate();

	nodes++;

	int eval = evaluate();

	if (eval >= beta)
		return beta;

	if (eval > alpha)
		alpha = eval;

	move_list moves[1];

	generate_moves(moves);

	sort_moves(moves);

	for (int i = 0; i < moves->count; i++)
	{
		copy_board();
		ply++;

		rep_index++;
		repetition_table[rep_index] = hash_key;

		if (!make_move(moves->moves[i], only_captures))
		{
			ply--;
			rep_index--;
			continue;
		}

		int score = -quiesce(-beta, -alpha);

		ply--;

		rep_index--;

		take_back();

		if (stopped) return 0;

		if (score >= beta)
			return beta;

		if (score > alpha)
			alpha = score;
	}

	return alpha;
}

static inline int negamax(int depth, int alpha, int beta)
{
	int hash_flag = hash_flag_alpha;

	int score;

	int pv_node = beta - alpha > 1;

	if (ply && is_repetition())
		return 0;

	if (ply && (score = read_tt_entry(depth, alpha, beta)) != no_hash_entry && !pv_node)
		return score;

	if ((nodes & 2047) == 0)
		communicate();

	pv_length[ply] = ply;

	if (depth == 0)
		return quiesce(alpha, beta);

	if (ply > max_ply - 1)
		return evaluate();

	nodes++;

	int in_check = is_square_attacked((side == white) ? get_lsb(board[K]) : get_lsb(board[k]), side ^ 1);

	if (in_check)
		depth++;

	int legal_moves = 0;

	// null move pruning
	if (depth >= 3 && in_check == 0 && ply)
	{
		copy_board();
		ply++;

		rep_index++;
		repetition_table[rep_index] = hash_key;

		if (enpassant != no_sqr) hash_key ^= enpassant_keys[enpassant];

		// give black another move for more beta cutoffs
		side ^= 1;

		hash_key ^= side_key;

		enpassant = no_sqr;

		// search moves with a reduced depth
		score = -negamax(depth - 1 - 2, -beta, -beta + 1);

		ply--;
		rep_index--;

		take_back();

		if (stopped)
			return 0;

		if (score >= beta)
			return beta;
	}

	move_list moves[1];

	generate_moves(moves);

	if (follow_pv)
		enable_pv_scoring(moves);

	sort_moves(moves);

	int moves_searched = 0;

	for (int i = 0; i < moves->count; i++)
	{
		copy_board();
		ply++;

		rep_index++;
		repetition_table[rep_index] = hash_key;

		if (!make_move(moves->moves[i], all_moves))
		{
			ply--;
			rep_index--;
			continue;
		}

		legal_moves++;

		// PVS or principal variation search
		if (moves_searched == 0)
			score = -negamax(depth - 1, -beta, -alpha);
		else
		{
			// LMR or late move reduction
			if (moves_searched >= full_depth_moves && depth >= reduction_limit && in_check == 0 && !get_move_capture(moves->moves[i]) && !get_move_promoted(moves->moves[i]))
				score = -negamax(depth - 2, -alpha - 1, -alpha);
			else
				score = alpha + 1;

			if (score > alpha)
			{
				score = -negamax(depth - 1, -alpha - 1, -alpha);

				if ((score > alpha) && (score < beta))
					// if LMR fails research at full depth
					score = -negamax(depth - 1, -beta, -alpha);
			}
		}

		ply--;
		rep_index--;

		take_back();

		if (stopped)
			return 0;

		moves_searched++;

		if (score >= beta)
		{
			write_tt_entry(depth, beta, hash_flag_beta);

			if (!get_move_capture(moves->moves[i]))
			{
				killer_moves[1][ply] = killer_moves[0][ply];
				killer_moves[0][ply] = moves->moves[i];
			}

			return beta;
		}

		if (score > alpha)
		{
			hash_flag = hash_flag_exact;

			if (!get_move_capture(moves->moves[i]))
				history_moves[get_move_piece(moves->moves[i])][get_move_target(moves->moves[i])] += depth;

			alpha = score;

			pv_table[ply][ply] = moves->moves[i];

			for (int next_ply = ply + 1; next_ply < pv_length[ply + 1]; next_ply++)
				pv_table[ply][next_ply] = pv_table[ply + 1][next_ply];

			pv_length[ply] = pv_length[ply + 1];
		}
	}

	if (legal_moves == 0)
	{
		if (in_check)
			return -49000 + ply;
		else
			return 0;
	}

	write_tt_entry(depth, alpha, hash_flag);

	return alpha;
}

void select_move(int depth)
{
	nodes = 0;

	follow_pv = 0;
	score_pv = 0;

	memset(killer_moves, 0, sizeof(killer_moves));
	memset(history_moves, 0, sizeof(history_moves));
	memset(pv_table, 0, sizeof(pv_table));
	memset(pv_length, 0, sizeof(pv_length));

	int alpha = -INF;
	int beta = INF;
	int score = 0;

	for (int current_depth = 1; current_depth <= depth; current_depth++)
	{
		if (stopped)
			break;

		follow_pv = 1;

		score = negamax(current_depth, alpha, beta);

		// we went outside the window 
		if ((score <= alpha) && (score >= beta))
		{
			// research with normal alpha and beta
			alpha = -INF;
			beta = INF;
			continue;
		}

		// setup aspiration window for next iteration, narrowing the scope for each ply
		alpha = score - 50;
		beta = score + 50;

		if (score > -mate_value && score < -mate_score)
			printf("info score mate %d depth %d nodes %ld time %d pv ", -(score + mate_score), current_depth, nodes, get_time_ms() - starttime);
		else if (score > mate_score && score < mate_value)
			printf("info score mate %d depth %d nodes %ld time %d pv ", (mate_score - score), current_depth, nodes, get_time_ms() - starttime);
		else
			printf("info score cp %d depth %d nodes %ld time %d pv ", score, current_depth, nodes, get_time_ms() - starttime);

		for (int i = 0; i < pv_length[0]; i++)
		{
			print_move(pv_table[0][i]);
			printf(" ");
		}

		printf("\n");
	}

	printf("bestmove ");
	print_move(pv_table[0][0]);
	printf("\n");
}

int parse_move(char* move_string)
{
	move_list moves[1];

	generate_moves(moves);

	int from_square = (move_string[0] - 'a') + (8 - (move_string[1] - '0')) * 8;
	int to_square = (move_string[2] - 'a') + (8 - (move_string[3] - '0')) * 8;

	for (int i = 0; i < moves->count; i++)
	{
		int move = moves->moves[i];

		if (from_square == get_move_source(move) && to_square == get_move_target(move))
		{
			int promoted_piece = get_move_promoted(move);

			if (promoted_piece)
			{
				if ((promoted_piece == Q || promoted_piece == q) && move_string[4] == q)
					return move;
				else if ((promoted_piece == R || promoted_piece == r) && move_string[4] == r)
					return move;
				else if ((promoted_piece == N || promoted_piece == n) && move_string[4] == n)
					return move;
				else if ((promoted_piece == B || promoted_piece == b) && move_string[4] == b)
					return move;

				continue;
			}

			return move;
		}
	}

	return 0;
}

void parse_position(char* command)
{
	command += 9;

	char* current_char = command;

	// found startpos
	if (strncmp(command, "startpos", 8) == 0)
		parse_fen(start_position);
	else
	{
		current_char = strstr(command, "fen");

		if (current_char == NULL)
			parse_fen(start_position);
		else
		{
			current_char += 4;
			parse_fen(current_char);
		}
	}

	current_char = strstr(command, "moves");

	if (current_char != NULL)
	{
		current_char += 6;
		printf("%s\n", current_char);

		while (*current_char)
		{
			int move = parse_move(current_char);

			if (move == 0)
				break;

			rep_index++;

			repetition_table[rep_index] = hash_key;

			make_move(move, all_moves);

			while (*current_char && *current_char != ' ')
				current_char++;

			current_char++;
		}
	}
}

// parse UCI command "go"
void parse_go(char* command)
{
	// init parameters
	int depth = -1;

	// init argument
	char* argument = NULL;

	// infinite search
	if ((argument = strstr(command, "infinite"))) {}

	// match UCI "binc" command
	if ((argument = strstr(command, "binc")) && side == black)
		// parse black time increment
		inc = atoi(argument + 5);

	// match UCI "winc" command
	if ((argument = strstr(command, "winc")) && side == white)
		// parse white time increment
		inc = atoi(argument + 5);

	// match UCI "wtime" command
	if ((argument = strstr(command, "wtime")) && side == white)
		// parse white time limit
		ttime = atoi(argument + 6);

	// match UCI "btime" command
	if ((argument = strstr(command, "btime")) && side == black)
		// parse black time limit
		ttime = atoi(argument + 6);

	// match UCI "movestogo" command
	if ((argument = strstr(command, "movestogo")))
		// parse number of moves to go
		movestogo = atoi(argument + 10);

	// match UCI "movetime" command
	if ((argument = strstr(command, "movetime")))
		// parse amount of time allowed to spend to make a move
		movetime = atoi(argument + 9);

	// match UCI "depth" command
	if ((argument = strstr(command, "depth")))
		// parse search depth
		depth = atoi(argument + 6);

	// if move time is not available
	if (movetime != -1)
	{
		// set time equal to move time
		ttime = movetime;

		// set moves to go to 1
		movestogo = 1;
	}

	// init start time
	starttime = get_time_ms();

	// init search depth
	depth = depth;

	// if time control is available
	if (ttime != -1)
	{
		// flag we're playing with time control
		timeset = 1;

		// set up timing
		ttime /= movestogo;
		ttime -= 50;
		stoptime = starttime + ttime + inc;
	}

	// if depth is not available
	if (depth == -1)
		// set depth to 64 plies (takes ages to complete...)
		depth = max_ply;

	// print debug info
	printf("time:%d start:%d stop:%d depth:%d timeset:%d\n",
		ttime, starttime, stoptime, depth, timeset);

	// search position
	select_move(depth);
}
void uci_loop()
{
	setbuf(stdin, NULL);
	setbuf(stdout, NULL);

	char input[2000];

	printf("id name LCCEngine\n");
	printf("id name Lancer\n");
	printf("uciok\n");

	while (1)
	{
		memset(input, 0, sizeof(input));

		fflush(stdout);

		if (!fgets(input, 2000, stdin))
			continue;

		if (input[0] == '\n')
			continue;

		if (strncmp(input, "isready", 7) == 0)
		{
			printf("readyok\n");
			continue;
		}
		else if (strncmp(input, "position", 8) == 0)
		{
			parse_position(input);
			clear_transpos_table();
		}
		else if (strncmp(input, "ucinewgame", 10) == 0)
		{
			parse_position("position startpos");
			clear_transpos_table();
		}
		else if (strncmp(input, "go", 2) == 0)
			parse_go(input);
		else if (strncmp(input, "quit", 4) == 0)
			break;
		else if (strncmp(input, "uci", 3) == 0)
		{
			printf("id name LCCEngine\n");
			printf("id name Lancer\n");
			printf("uciok\n");
		}
	}
}

void init_all()
{
	init_leaper_attacks();

	init_slider_attacks(bishop);
	init_slider_attacks(rook);

	init_random_keys();

	clear_transpos_table();
}

int main()
{
	init_all();

	//uci_loop();

	//return 0;

	parse_fen(start_position);

	while (1)
	{
		print_board();

		int start = get_time_ms();
		select_move(11);
		int end = get_time_ms();

		make_move(pv_table[0][0], all_moves);
	}

	return 0;
}

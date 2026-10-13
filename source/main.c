#include <gba.h>
#include <stdbool.h>
#include <string.h>
#include "song.h"

/*
    ===========================================================================
    FRIDAY NIGHT FUNKIN' GBA DEMAKE - VISUAL OVERHAUL & CHORD ENGINE
    ===========================================================================
    A complete cartridge-grade demake of Friday Night Funkin' for Game Boy Advance.

    Visual & Engine Features:
    - 4-Lane Strumline with distinct Directional Arrow Receptor Outlines
    - Iconic FNF Neon Arrow Notes (Purple Left, Cyan Down, Lime Up, Red Right)
    - 3-Color Sprites with Dark Outlines, Saturated Cores, and Inner Highlights
    - Animated 32x32 Characters:
        * Boyfriend (Right): Bops to beat, poses Left/Down/Up/Right on hits, Miss face!
        * Opponent / Daddy Dearest (Left): Bops to beat, sings with microphone!
    - Authentic FNF Health Bar with Sliding Opponent and Boyfriend Head Icons:
        * Dynamic Red/Cyan split based on health percentage
        * Sliding icons with rhythmic beat-hop bounce
        * Distressed / Losing face when Boyfriend health drops below 20%
    - 32x16 Comic Judgement Popups (SICK!!, GOOD, BAD, MISS) with float animation
    - Dynamic Stage Atmosphere:
        * Dark purple brick wall with spotlight cones
        * Left & Right Speaker stacks with subwoofer cones that thump to BPM
        * Girlfriend silhouette sitting on center boombox on Title screen
    - Full Freeplay Song Select with Difficulty Star ratings, BPM, and High Scores
    - Results screen with Rank Badges (S, A, B, C, D, F) and Accuracy breakdown
    - 100% Concurrent Chord Support:
        * Spread Mode: Left hand (DPAD_LEFT, DPAD_DOWN), Right hand (KEY_B, KEY_A)
        * Single-hand D-pad: All 4 directions (LEFT, DOWN, UP, RIGHT)
        * Simultaneous Chord Assist & Held-Key Tolerance
        * FNF Ghost Tapping (no penalty for empty lane presses)
*/

#define SCREEN_WIDTH      240
#define SCREEN_HEIGHT     160

#define LANE_TARGET_Y     128
#define NOTE_SPEED        2
#define SPAWN_LEAD_FRAMES 56   /* (LANE_TARGET_Y - 16) / NOTE_SPEED */

/* Timing Windows (frames @ 60 FPS) */
#define SICK_WINDOW       3
#define GOOD_WINDOW       6
#define BAD_WINDOW        9
#define MISS_WINDOW       12

#define HIT_FLASH_TIME    10
#define JUDGEMENT_TIME    24

/* Hardware Helpers */
#define ATTR0_Y(y)        ((y) & 0x00FF)
#define ATTR1_X(x)        ((x) & 0x01FF)
#define ATTR2_TILE(n)     ((n) & 0x03FF)

#define BG0_SBB           31
#define BG0_CBB           0

/* Ergonomic Key Bindings for 100% Concurrent Chords */
/* Spread Mode: Left hand = D-pad (LEFT, DOWN), Right hand = Face buttons (B, A) */
/* Single Hand: D-pad (LEFT, DOWN, UP, RIGHT) all function naturally */
#define KEY_LANE_LEFT     (KEY_LEFT  | KEY_L)
#define KEY_LANE_DOWN     (KEY_DOWN)
#define KEY_LANE_UP       (KEY_UP    | KEY_B)
#define KEY_LANE_RIGHT    (KEY_RIGHT | KEY_A | KEY_R)

static const u16 lane_key_masks[LANE_COUNT] = {
    KEY_LANE_LEFT, KEY_LANE_DOWN, KEY_LANE_UP, KEY_LANE_RIGHT
};

/* Lane X positions: centered 4 lanes (each 16px wide inside 24px columns) */
static const int lane_x[LANE_COUNT] = { 72, 96, 120, 144 };

/* Game States */
typedef enum {
    STATE_TITLE = 0,
    STATE_SONG_SELECT,
    STATE_GAMEPLAY,
    STATE_RESULTS
} GameState;

static GameState current_state = STATE_TITLE;

/* Judgement Types */
typedef enum {
    JUDGE_NONE = 0,
    JUDGE_SICK,
    JUDGE_GOOD,
    JUDGE_BAD,
    JUDGE_MISS
} JudgementType;

/* Active Runtime Note */
typedef struct {
    u16 target_tick;
    u8 lane;
    bool active;
    bool hit;
    bool missed;
} ActiveNote;

#define MAX_ACTIVE_NOTES 64
static ActiveNote active_notes[MAX_ACTIVE_NOTES];
static int next_chart_idx = 0;

/* Hardware OAM Shadow Buffer */
static OBJATTR shadow_oam[128] ALIGN(4);

/* Sprite OAM Slot Layout */
#define OAM_RECEPTOR_BASE 0   /* 0..3: 4 Directional Receptors (16x16) */
#define OAM_BURST_BASE    4   /* 4..7: 4 Hit Bursts (16x16) */
#define OAM_JUDGEMENT     8   /* 1: Judgement Badge (32x16) */
#define OAM_ICON_OPP      9   /* 1: Opponent Health Icon (16x16) */
#define OAM_ICON_BF       10  /* 1: Boyfriend Health Icon (16x16) */
#define OAM_CHAR_OPP      11  /* 1: Opponent Character (32x32) */
#define OAM_CHAR_BF       12  /* 1: Boyfriend Character (32x32) */
#define OAM_CURSOR        13  /* 1: Song Select Cursor (16x16) */
#define OAM_NOTES_BASE    16  /* 16..127: Falling Notes */

/* Sprite Tile Offsets (Charblock 4, 4bpp in 1D mode) */
#define SPR_TILE_LEFT          0   /* 16x16 = 4 tiles (0..3) */
#define SPR_TILE_DOWN          4   /* 16x16 = 4 tiles (4..7) */
#define SPR_TILE_UP            8   /* 16x16 = 4 tiles (8..11) */
#define SPR_TILE_RIGHT         12  /* 16x16 = 4 tiles (12..15) */

#define SPR_TILE_REC_LEFT      16  /* 16x16 = 4 tiles (16..19) */
#define SPR_TILE_REC_DOWN      20  /* 16x16 = 4 tiles (20..23) */
#define SPR_TILE_REC_UP        24  /* 16x16 = 4 tiles (24..27) */
#define SPR_TILE_REC_RIGHT     28  /* 16x16 = 4 tiles (28..31) */

#define SPR_TILE_BURST         32  /* 16x16 = 4 tiles (32..35) */

#define SPR_TILE_JUDGE_SICK    36  /* 32x16 = 8 tiles (36..43) */
#define SPR_TILE_JUDGE_GOOD    44  /* 32x16 = 8 tiles (44..51) */
#define SPR_TILE_JUDGE_BAD     52  /* 32x16 = 8 tiles (52..59) */
#define SPR_TILE_JUDGE_MISS    60  /* 32x16 = 8 tiles (60..67) */

#define SPR_TILE_ICON_OPP      68  /* 16x16 = 4 tiles (68..71) */
#define SPR_TILE_ICON_BF       72  /* 16x16 = 4 tiles (72..75) */
#define SPR_TILE_ICON_BF_LOSE  76  /* 16x16 = 4 tiles (76..79) */

#define SPR_TILE_CURSOR        80  /* 16x16 = 4 tiles (80..83) */

#define SPR_TILE_BF_IDLE       84  /* 32x32 = 16 tiles (84..99) */
#define SPR_TILE_BF_LEFT       100 /* 32x32 = 16 tiles (100..115) */
#define SPR_TILE_BF_DOWN       116 /* 32x32 = 16 tiles (116..131) */
#define SPR_TILE_BF_UP         132 /* 32x32 = 16 tiles (132..147) */
#define SPR_TILE_BF_RIGHT      148 /* 32x32 = 16 tiles (148..163) */
#define SPR_TILE_BF_MISS       164 /* 32x32 = 16 tiles (164..179) */

#define SPR_TILE_OPP_IDLE      180 /* 32x32 = 16 tiles (180..195) */
#define SPR_TILE_OPP_SING      196 /* 32x32 = 16 tiles (196..211) */

/* Sprite Palette Indices */
#define PAL_SPR_LEFT           0   /* FNF Purple/Violet */
#define PAL_SPR_DOWN           1   /* FNF Cyan */
#define PAL_SPR_UP             2   /* FNF Lime/Green */
#define PAL_SPR_RIGHT          3   /* FNF Red/Scarlet */
#define PAL_SPR_REC_IDLE       4   /* Dim ghostly outline */
#define PAL_SPR_BURST          5   /* White/Yellow spark flash */
#define PAL_SPR_SICK           6   /* Gold/Rainbow */
#define PAL_SPR_GOOD           7   /* Emerald/Mint */
#define PAL_SPR_BAD            8   /* Orange/Gold */
#define PAL_SPR_MISS           9   /* Crimson Red */
#define PAL_SPR_BF             10  /* Boyfriend 16-color palette */
#define PAL_SPR_OPP            11  /* Opponent 16-color palette */
#define PAL_SPR_CURSOR         12  /* Neon Yellow */

/* Global Engine Variables */
static volatile u32 global_ticks = 0;
static u32 song_tick = 0;

static int selected_song_idx = 0;
static const Song* current_song = NULL;

/* Performance Stats */
static int score = 0;
static int combo = 0;
static int max_combo = 0;
static int count_sick = 0;
static int count_good = 0;
static int count_bad = 0;
static int count_miss = 0;
static int total_song_notes = 0;
static int health = 50; /* 0..100, 50=neutral, <=0=game over danger */

/* Character Animation States */
static int bf_pose = 0;       /* 0=Idle, 1=Left, 2=Down, 3=Up, 4=Right, 5=Miss */
static int bf_pose_timer = 0;
static int opp_sing_timer = 0;

/* High Scores Table */
#define MAX_SONGS_TRACKED 8
static int high_scores[MAX_SONGS_TRACKED] = { 0 };

static int lane_flash_timer[LANE_COUNT] = { 0, 0, 0, 0 };
static JudgementType current_judgement = JUDGE_NONE;
static int judgement_timer = 0;

/* Title & Menu Animation */
static u16 anim_frame = 0;

/* VBlank ISR */
static void vblank_intr(void) { global_ticks++; }

/* -------------------------------------------------------------------------- */
/* Font Glyphs (8x8)                                                          */
/* -------------------------------------------------------------------------- */
static const u8 font_8x8_data[46][8] = {
    /* 0..9 (0..9) */
    { 0x3C, 0x66, 0x6E, 0x76, 0x66, 0x66, 0x3C, 0x00 },
    { 0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00 },
    { 0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00 },
    { 0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00 },
    { 0x0C, 0x1C, 0x34, 0x64, 0x7E, 0x04, 0x04, 0x00 },
    { 0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00 },
    { 0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00 },
    { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x00 },
    { 0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00 },
    { 0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00 },
    /* A..Z (10..35) */
    { 0x18, 0x3C, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 },
    { 0x7C, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x7C, 0x00 },
    { 0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00 },
    { 0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00 },
    { 0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x7E, 0x00 },
    { 0x7E, 0x60, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x00 },
    { 0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3E, 0x00 },
    { 0x66, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00 },
    { 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00 },
    { 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00 },
    { 0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00 },
    { 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00 },
    { 0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00 },
    { 0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00 },
    { 0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 },
    { 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x00 },
    { 0x3C, 0x66, 0x66, 0x66, 0x6E, 0x3C, 0x06, 0x00 },
    { 0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00 },
    { 0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00 },
    { 0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00 },
    { 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00 },
    { 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x00 },
    { 0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00 },
    { 0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00 },
    { 0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x00 },
    { 0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00 },
    /* 36..45: Symbols */
    { 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00 }, /* : (36) */
    { 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00 }, /* - (37) */
    { 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00 }, /* ! (38) */
    { 0x60, 0x30, 0x18, 0x0C, 0x18, 0x30, 0x60, 0x00 }, /* > (39) */
    { 0x62, 0x64, 0x08, 0x10, 0x20, 0x26, 0x46, 0x00 }, /* % (40) */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00 }, /* . (41) */
    { 0x02, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00 }, /* / (42) */
    { 0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00 }, /* [ (43) */
    { 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00 }, /* ] (44) */
    { 0x10, 0x38, 0xFE, 0x7C, 0x38, 0x6C, 0xC6, 0x00 }  /* * (45) Star */
};

static u16 get_char_tile(char c) {
    if (c >= '0' && c <= '9') return 16 + (c - '0');
    if (c >= 'A' && c <= 'Z') return 16 + 10 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 16 + 10 + (c - 'a');
    if (c == ':') return 16 + 36;
    if (c == '-') return 16 + 37;
    if (c == '!') return 16 + 38;
    if (c == '>') return 16 + 39;
    if (c == '%') return 16 + 40;
    if (c == '.') return 16 + 41;
    if (c == '/') return 16 + 42;
    if (c == '[') return 16 + 43;
    if (c == ']') return 16 + 44;
    if (c == '*') return 16 + 45;
    if (c == '\'') return 16 + 37;
    return 0;
}

static void draw_text(int col, int row, const char* str, u8 pal_bank) {
    u16* bg_map = (u16*)SCREEN_BASE_BLOCK(BG0_SBB);
    int c = col;
    while (*str && c < 30) {
        u16 tile = get_char_tile(*str);
        bg_map[row * 32 + c] = tile | ((u16)(pal_bank & 0x0F) << 12);
        c++;
        str++;
    }
}

static void draw_number(int col, int row, int val, int digits, u8 pal_bank) {
    char buf[12];
    int div = 1;
    for (int i = 1; i < digits; i++) div *= 10;
    for (int i = 0; i < digits; i++) {
        buf[i] = '0' + ((val / div) % 10);
        div /= 10;
    }
    buf[digits] = '\0';
    draw_text(col, row, buf, pal_bank);
}

static void fill_row(int row, int col_start, int col_end, u16 tile_pal) {
    u16* bg_map = (u16*)SCREEN_BASE_BLOCK(BG0_SBB);
    for (int c = col_start; c < col_end; c++) {
        bg_map[row * 32 + c] = tile_pal;
    }
}

/* -------------------------------------------------------------------------- */
/* Sprite Pixel Helpers (1D Mapping Mode)                                     */
/* -------------------------------------------------------------------------- */
static void set_sprite_pixel(u32* spr_base, int base_tile, int w_tiles, int x, int y, u8 color) {
    if (x < 0 || y < 0) return;
    int tile_col = x / 8;
    int tile_row = y / 8;
    if (tile_col >= w_tiles) return;
    int tile_idx = base_tile + (tile_row * w_tiles + tile_col);
    int lx = x % 8;
    int ly = y % 8;
    int shift = lx * 4;
    u32* words = &spr_base[tile_idx * 8];
    words[ly] = (words[ly] & ~(0xFU << shift)) | (((u32)(color & 0xF)) << shift);
}

static void draw_spr_rect(u32* spr_base, int base_tile, int w_tiles,
                          int x0, int y0, int w, int h, u8 color) {
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            set_sprite_pixel(spr_base, base_tile, w_tiles, x0 + dx, y0 + dy, color);
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Arrow & Receptor Bitmasks (16x16)                                          */
/* -------------------------------------------------------------------------- */

/* Solid Arrows */
static const u16 mask_up[16] = {
    0b0000001100000000,
    0b0000011110000000,
    0b0000111111000000,
    0b0001111111100000,
    0b0011111111110000,
    0b0111111111111000,
    0b1111111111111100,
    0b1111111111111110,
    0b0000111111000000,
    0b0000111111000000,
    0b0000111111000000,
    0b0000111111000000,
    0b0000111111000000,
    0b0000011110000000,
    0b0000000000000000,
    0b0000000000000000
};

static const u16 inner_up[16] = {
    0b0000000000000000,
    0b0000001100000000,
    0b0000011110000000,
    0b0000111111000000,
    0b0001111111100000,
    0b0011111111110000,
    0b0000011110000000,
    0b0000000000000000,
    0b0000011110000000,
    0b0000011110000000,
    0b0000011110000000,
    0b0000011110000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};

static const u16 mask_down[16] = {
    0b0000000000000000,
    0b0000011110000000,
    0b0000111111000000,
    0b0000111111000000,
    0b0000111111000000,
    0b0000111111000000,
    0b0000111111000000,
    0b1111111111111110,
    0b1111111111111100,
    0b0111111111111000,
    0b0011111111110000,
    0b0001111111100000,
    0b0000111111000000,
    0b0000011110000000,
    0b0000001100000000,
    0b0000000000000000
};

static const u16 inner_down[16] = {
    0b0000000000000000,
    0b0000000000000000,
    0b0000011110000000,
    0b0000011110000000,
    0b0000011110000000,
    0b0000011110000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000011110000000,
    0b0011111111110000,
    0b0001111111100000,
    0b0000111111000000,
    0b0000011110000000,
    0b0000001100000000,
    0b0000000000000000,
    0b0000000000000000
};

static const u16 mask_left[16] = {
    0b0000001000000000,
    0b0000011000000000,
    0b0000111000000000,
    0b0001111000000000,
    0b0011111111111110,
    0b0111111111111110,
    0b1111111111111110,
    0b1111111111111110,
    0b1111111111111110,
    0b0111111111111110,
    0b0011111111111110,
    0b0001111000000000,
    0b0000111000000000,
    0b0000011000000000,
    0b0000001000000000,
    0b0000000000000000
};

static const u16 inner_left[16] = {
    0b0000000000000000,
    0b0000001000000000,
    0b0000011000000000,
    0b0000111000000000,
    0b0001110000000000,
    0b0011111111111100,
    0b0111111111111100,
    0b0011111111111100,
    0b0001110000000000,
    0b0000111000000000,
    0b0000011000000000,
    0b0000001000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};

static const u16 mask_right[16] = {
    0b0000000001000000,
    0b0000000001100000,
    0b0000000001110000,
    0b0000000001111000,
    0b0111111111111100,
    0b0111111111111110,
    0b0111111111111111,
    0b0111111111111111,
    0b0111111111111111,
    0b0111111111111110,
    0b0111111111111100,
    0b0000000001111000,
    0b0000000001110000,
    0b0000000001100000,
    0b0000000001000000,
    0b0000000000000000
};

static const u16 inner_right[16] = {
    0b0000000000000000,
    0b0000000001000000,
    0b0000000001100000,
    0b0000000001110000,
    0b0000000001110000,
    0b0011111111111100,
    0b0011111111111110,
    0b0011111111111100,
    0b0000000001110000,
    0b0000000001110000,
    0b0000000001100000,
    0b0000000001000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000,
    0b0000000000000000
};

/* Hollow Directional Receptor Outlines */
static const u16 mask_rec_up[16] = {
    0b0000001100000000,
    0b0000011110000000,
    0b0000110011000000,
    0b0001100001100000,
    0b0011000000110000,
    0b0110000000011000,
    0b1111110011111100,
    0b1100000000000110,
    0b0011000000110000,
    0b0011000000110000,
    0b0011000000110000,
    0b0011000000110000,
    0b0011000000110000,
    0b0001111111100000,
    0b0000000000000000,
    0b0000000000000000
};

static const u16 mask_rec_down[16] = {
    0b0000000000000000,
    0b0001111111100000,
    0b0011000000110000,
    0b0011000000110000,
    0b0011000000110000,
    0b0011000000110000,
    0b0011000000110000,
    0b1100000000000110,
    0b1111110011111100,
    0b0110000000011000,
    0b0011000000110000,
    0b0001100001100000,
    0b0000110011000000,
    0b0000011110000000,
    0b0000001100000000,
    0b0000000000000000
};

static const u16 mask_rec_left[16] = {
    0b0000001000000000,
    0b0000011000000000,
    0b0000110000000000,
    0b0001100000000000,
    0b0011001111111110,
    0b0110001000000010,
    0b1100001000000010,
    0b1100000000000010,
    0b1100001000000010,
    0b0110001000000010,
    0b0011001111111110,
    0b0001100000000000,
    0b0000110000000000,
    0b0000011000000000,
    0b0000001000000000,
    0b0000000000000000
};

static const u16 mask_rec_right[16] = {
    0b0000000001000000,
    0b0000000001100000,
    0b0000000000110000,
    0b0000000000011000,
    0b0111111111001100,
    0b0100000001000110,
    0b0100000001000011,
    0b0100000000000011,
    0b0100000001000011,
    0b0100000001000110,
    0b0111111111001100,
    0b0000000000011000,
    0b0000000000110000,
    0b0000000001100000,
    0b0000000001000000,
    0b0000000000000000
};

/* Hit Burst (16x16) */
static const u16 mask_burst[16] = {
    0b0000000110000000,
    0b0010000110000100,
    0b0001001111001000,
    0b0000111111110000,
    0b0001111111111000,
    0b0011111111111100,
    0b0111111111111110,
    0b1111111111111111,
    0b1111111111111111,
    0b0111111111111110,
    0b0011111111111100,
    0b0001111111111000,
    0b0000111111110000,
    0b0001001111001000,
    0b0010000110000100,
    0b0000000110000000
};

/* Menu Cursor (16x16) */
static const u16 mask_cursor[16] = {
    0b1110000000000000,
    0b1111100000000000,
    0b1111111000000000,
    0b1111111110000000,
    0b1111111111100000,
    0b1111111111111000,
    0b1111111111111100,
    0b1111111111111110,
    0b1111111111111110,
    0b1111111111111100,
    0b1111111111111000,
    0b1111111111100000,
    0b1111111110000000,
    0b1111111000000000,
    0b1111100000000000,
    0b1110000000000000
};

/* -------------------------------------------------------------------------- */
/* Sprite Generation Functions                                                */
/* -------------------------------------------------------------------------- */

static void gen_sprite_3col(u32* dest, const u16 mask[16], const u16 inner[16],
                            u8 fill, u8 highlight, u8 outline) {
    u32 tiles[4][8];
    memset(tiles, 0, sizeof(tiles));

    for (int row = 0; row < 16; row++) {
        for (int col = 0; col < 16; col++) {
            bool is_set = (mask[row] & (1 << (15 - col))) != 0;
            if (is_set) {
                int ti = (row / 8) * 2 + (col / 8);
                bool is_inner = inner && (inner[row] & (1 << (15 - col))) != 0;
                u8 c = is_inner ? highlight : fill;
                int lx = col % 8, ly = row % 8;
                tiles[ti][ly] |= (((u32)(c & 0xF)) << (lx * 4));
            }
        }
    }

    /* Outline */
    for (int t = 0; t < 4; t++) {
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                u8 p = (tiles[t][row] >> (col * 4)) & 0xF;
                if (p == 0) {
                    bool adj = false;
                    for (int dy = -1; dy <= 1 && !adj; dy++) {
                        for (int dx = -1; dx <= 1 && !adj; dx++) {
                            if (dx == 0 && dy == 0) continue;
                            int nx = col + dx, ny = row + dy;
                            if (nx >= 0 && nx < 8 && ny >= 0 && ny < 8) {
                                if (((tiles[t][ny] >> (nx * 4)) & 0xF) != 0) adj = true;
                            }
                        }
                    }
                    if (adj) tiles[t][row] |= (((u32)(outline & 0xF)) << (col * 4));
                }
            }
        }
    }

    memcpy(dest, tiles, sizeof(tiles));
}

static void gen_sprite_2col(u32* dest, const u16 mask[16], u8 fill, u8 outline) {
    gen_sprite_3col(dest, mask, NULL, fill, fill, outline);
}

/* -------------------------------------------------------------------------- */
/* Judgement Badges (32x16)                                                   */
/* -------------------------------------------------------------------------- */
static void gen_judgement_badges(u32* spr_base) {
    /* 1. SICK!! (SPR_TILE_JUDGE_SICK) */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 0, 0, 32, 16, 0);
    /* 'S' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 3, 3, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 3, 5, 2, 3, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 3, 7, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 6, 9, 2, 3, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 3, 11, 5, 2, 2);
    /* 'I' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 10, 3, 2, 10, 2);
    /* 'C' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 14, 3, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 14, 5, 2, 6, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 14, 11, 5, 2, 2);
    /* 'K' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 21, 3, 2, 10, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 24, 4, 2, 3, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 23, 7, 2, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 24, 9, 2, 4, 2);
    /* '!' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 28, 3, 2, 7, 3);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_SICK, 4, 28, 12, 2, 2, 3);
    /* Sparkle highlights on top */
    set_sprite_pixel(spr_base, SPR_TILE_JUDGE_SICK, 4, 4, 3, 3);
    set_sprite_pixel(spr_base, SPR_TILE_JUDGE_SICK, 4, 10, 3, 3);
    set_sprite_pixel(spr_base, SPR_TILE_JUDGE_SICK, 4, 15, 3, 3);
    set_sprite_pixel(spr_base, SPR_TILE_JUDGE_SICK, 4, 21, 3, 3);

    /* 2. GOOD (SPR_TILE_JUDGE_GOOD) */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 0, 0, 32, 16, 0);
    /* 'G' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 4, 3, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 4, 5, 2, 6, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 4, 11, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 7, 7, 2, 4, 2);
    /* 'O' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 11, 3, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 11, 5, 2, 6, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 14, 5, 2, 6, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 11, 11, 5, 2, 2);
    /* 'O' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 18, 3, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 18, 5, 2, 6, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 21, 5, 2, 6, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 18, 11, 5, 2, 2);
    /* 'D' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 25, 3, 4, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 25, 5, 2, 6, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 28, 5, 2, 6, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_GOOD, 4, 25, 11, 4, 2, 2);

    /* 3. BAD (SPR_TILE_JUDGE_BAD) */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 0, 0, 32, 16, 0);
    /* 'B' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 6, 3, 2, 10, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 8, 3, 4, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 10, 5, 2, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 8, 7, 4, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 10, 9, 2, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 8, 11, 4, 2, 2);
    /* 'A' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 14, 5, 2, 8, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 19, 5, 2, 8, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 15, 3, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 15, 8, 5, 2, 2);
    /* 'D' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 23, 3, 2, 10, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 25, 3, 3, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 27, 5, 2, 6, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_BAD, 4, 25, 11, 3, 2, 2);

    /* 4. MISS (SPR_TILE_JUDGE_MISS) */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 0, 0, 32, 16, 0);
    /* 'M' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 4, 3, 2, 10, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 6, 5, 2, 3, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 8, 3, 2, 10, 2);
    /* 'I' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 12, 3, 2, 10, 2);
    /* 'S' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 16, 3, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 16, 5, 2, 3, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 16, 7, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 19, 9, 2, 3, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 16, 11, 5, 2, 2);
    /* 'S' */
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 23, 3, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 23, 5, 2, 3, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 23, 7, 5, 2, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 26, 9, 2, 3, 2);
    draw_spr_rect(spr_base, SPR_TILE_JUDGE_MISS, 4, 23, 11, 5, 2, 2);
}

/* -------------------------------------------------------------------------- */
/* Health Icons (16x16)                                                       */
/* -------------------------------------------------------------------------- */
static void gen_health_icons(u32* spr_base) {
    /* 1. Opponent Icon (SPR_TILE_ICON_OPP) */
    draw_spr_rect(spr_base, SPR_TILE_ICON_OPP, 2, 0, 0, 16, 16, 0);
    /* Spiky demon hair */
    draw_spr_rect(spr_base, SPR_TILE_ICON_OPP, 2, 2, 1, 12, 4, 3);
    set_sprite_pixel(spr_base, SPR_TILE_ICON_OPP, 2, 3, 0, 3);
    set_sprite_pixel(spr_base, SPR_TILE_ICON_OPP, 2, 8, 0, 3);
    set_sprite_pixel(spr_base, SPR_TILE_ICON_OPP, 2, 13, 0, 3);
    /* Face (Lilac) */
    draw_spr_rect(spr_base, SPR_TILE_ICON_OPP, 2, 3, 5, 10, 8, 2);
    /* Glowing yellow eyes */
    draw_spr_rect(spr_base, SPR_TILE_ICON_OPP, 2, 5, 7, 2, 2, 5);
    draw_spr_rect(spr_base, SPR_TILE_ICON_OPP, 2, 9, 7, 2, 2, 5);
    /* Smirk */
    draw_spr_rect(spr_base, SPR_TILE_ICON_OPP, 2, 6, 11, 5, 1, 1);
    set_sprite_pixel(spr_base, SPR_TILE_ICON_OPP, 2, 10, 10, 1);

    /* 2. Boyfriend Normal Icon (SPR_TILE_ICON_BF) */
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF, 2, 0, 0, 16, 16, 0);
    /* Red Cap */
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF, 2, 3, 2, 11, 4, 2);
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF, 2, 1, 5, 4, 2, 8); /* Visor */
    /* Cyan Hair puff */
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF, 2, 4, 6, 4, 3, 3);
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF, 2, 11, 5, 4, 4, 3);
    /* Peach Face */
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF, 2, 5, 8, 8, 6, 4);
    /* Determined Eye */
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF, 2, 8, 9, 3, 2, 5);
    set_sprite_pixel(spr_base, SPR_TILE_ICON_BF, 2, 9, 10, 1);
    /* Confident Grin */
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF, 2, 7, 12, 4, 1, 1);

    /* 3. Boyfriend Losing Icon (SPR_TILE_ICON_BF_LOSE) */
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF_LOSE, 2, 0, 0, 16, 16, 0);
    /* Red Cap */
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF_LOSE, 2, 3, 2, 11, 4, 2);
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF_LOSE, 2, 1, 5, 4, 2, 8);
    /* Distressed Face (Peach with blue sweat) */
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF_LOSE, 2, 4, 7, 10, 7, 4);
    /* Sweat drop (Cyan) */
    set_sprite_pixel(spr_base, SPR_TILE_ICON_BF_LOSE, 2, 3, 8, 3);
    set_sprite_pixel(spr_base, SPR_TILE_ICON_BF_LOSE, 2, 3, 9, 3);
    /* X Eye (> <) */
    set_sprite_pixel(spr_base, SPR_TILE_ICON_BF_LOSE, 2, 7, 9, 1);
    set_sprite_pixel(spr_base, SPR_TILE_ICON_BF_LOSE, 2, 9, 9, 1);
    set_sprite_pixel(spr_base, SPR_TILE_ICON_BF_LOSE, 2, 8, 10, 1);
    /* Jagged open mouth */
    draw_spr_rect(spr_base, SPR_TILE_ICON_BF_LOSE, 2, 6, 12, 5, 2, 1);
}

/* -------------------------------------------------------------------------- */
/* Animated Boyfriend Sprites (32x32, 16 tiles each)                          */
/* -------------------------------------------------------------------------- */
static void gen_bf_pose(u32* spr_base, int base_tile, int pose) {
    draw_spr_rect(spr_base, base_tile, 4, 0, 0, 32, 32, 0);

    int off_x = 0;
    int off_y = 0;
    int mic_x = 21, mic_y = 15;

    if (pose == 1) { /* LEFT */
        off_x = -3; off_y = 0;
        mic_x = 12; mic_y = 14;
    } else if (pose == 2) { /* DOWN */
        off_x = 0; off_y = 4;
        mic_x = 19; mic_y = 23;
    } else if (pose == 3) { /* UP */
        off_x = 0; off_y = -3;
        mic_x = 21; mic_y = 7;
    } else if (pose == 4) { /* RIGHT */
        off_x = 4; off_y = 0;
        mic_x = 27; mic_y = 14;
    } else if (pose == 5) { /* MISS */
        off_x = 2; off_y = 1;
        mic_x = 23; mic_y = 21;
    }

    int cx = 14 + off_x;
    int cy = 16 + off_y;

    /* Sneakers (Red) */
    draw_spr_rect(spr_base, base_tile, 4, cx - 6, 27 + off_y, 5, 4, 2);
    draw_spr_rect(spr_base, base_tile, 4, cx + 2, 27 + off_y, 6, 4, 2);
    /* Soles */
    draw_spr_rect(spr_base, base_tile, 4, cx - 6, 30 + off_y, 5, 1, 5);
    draw_spr_rect(spr_base, base_tile, 4, cx + 2, 30 + off_y, 6, 1, 5);

    /* Baggy Jeans (Dark Blue) */
    draw_spr_rect(spr_base, base_tile, 4, cx - 5, 21 + off_y, 11, 7, 6);

    /* T-Shirt (White) */
    draw_spr_rect(spr_base, base_tile, 4, cx - 4, cy - 2, 9, 8, 5);
    /* Forbidden sign red logo */
    draw_spr_rect(spr_base, base_tile, 4, cx - 1, cy + 1, 3, 3, 2);

    /* Peach Face */
    draw_spr_rect(spr_base, base_tile, 4, cx - 3, cy - 9, 8, 7, 4);

    if (pose == 5) {
        /* Miss: Sweat drop & shocked eyes */
        set_sprite_pixel(spr_base, base_tile, 4, cx - 5, cy - 9, 3);
        set_sprite_pixel(spr_base, base_tile, 4, cx - 5, cy - 8, 3);
        draw_spr_rect(spr_base, base_tile, 4, cx, cy - 7, 3, 2, 1);
        draw_spr_rect(spr_base, base_tile, 4, cx - 1, cy - 4, 4, 2, 1);
    } else {
        /* Confident Eye */
        draw_spr_rect(spr_base, base_tile, 4, cx + 1, cy - 8, 3, 3, 5);
        set_sprite_pixel(spr_base, base_tile, 4, cx + 2, cy - 7, 1);
        /* Smirk */
        draw_spr_rect(spr_base, base_tile, 4, cx, cy - 4, 4, 1, 1);
    }

    /* Red Backwards Cap */
    draw_spr_rect(spr_base, base_tile, 4, cx - 4, cy - 13, 10, 5, 2);
    draw_spr_rect(spr_base, base_tile, 4, cx - 7, cy - 10, 4, 2, 8); /* Visor */

    /* Spiky Cyan Hair */
    draw_spr_rect(spr_base, base_tile, 4, cx + 4, cy - 11, 4, 5, 3);
    draw_spr_rect(spr_base, base_tile, 4, cx - 2, cy - 8, 3, 3, 3);
    set_sprite_pixel(spr_base, base_tile, 4, cx + 5, cy - 10, 9); /* highlight */

    /* Silver Microphone */
    draw_spr_rect(spr_base, base_tile, 4, mic_x, mic_y, 4, 4, 7);
    draw_spr_rect(spr_base, base_tile, 4, mic_x + 1, mic_y + 4, 2, 3, 1); /* handle */
}

/* -------------------------------------------------------------------------- */
/* Animated Opponent Sprites (32x32, 16 tiles each)                           */
/* -------------------------------------------------------------------------- */
static void gen_opp_pose(u32* spr_base, int base_tile, bool is_singing) {
    draw_spr_rect(spr_base, base_tile, 4, 0, 0, 32, 32, 0);

    int off_y = is_singing ? -1 : 0;
    int cx = 15;

    /* Black Trench Coat */
    draw_spr_rect(spr_base, base_tile, 4, cx - 6, 14 + off_y, 13, 16, 4);
    /* Red Tie & Lavender Shirt */
    draw_spr_rect(spr_base, base_tile, 4, cx - 1, 13 + off_y, 3, 8, 6);

    /* Demon Face (Lilac) */
    draw_spr_rect(spr_base, base_tile, 4, cx - 4, 5 + off_y, 9, 8, 2);

    /* Yellow Slit Eyes */
    draw_spr_rect(spr_base, base_tile, 4, cx - 2, 7 + off_y, 2, 2, 5);
    draw_spr_rect(spr_base, base_tile, 4, cx + 2, 7 + off_y, 2, 2, 5);

    if (is_singing) {
        /* Open Mouth Singing */
        draw_spr_rect(spr_base, base_tile, 4, cx - 2, 10 + off_y, 5, 3, 1);
        set_sprite_pixel(spr_base, base_tile, 4, cx, 11 + off_y, 6);
    } else {
        /* Wicked Smile */
        draw_spr_rect(spr_base, base_tile, 4, cx - 2, 11 + off_y, 5, 1, 1);
    }

    /* Wild Spiky Demon Horns/Hair (Magenta) */
    draw_spr_rect(spr_base, base_tile, 4, cx - 6, 1 + off_y, 13, 5, 3);
    set_sprite_pixel(spr_base, base_tile, 4, cx - 5, 0 + off_y, 3);
    set_sprite_pixel(spr_base, base_tile, 4, cx, 0 + off_y, 9);
    set_sprite_pixel(spr_base, base_tile, 4, cx + 5, 0 + off_y, 3);

    /* Silver Microphone in hand */
    int mic_x = is_singing ? (cx + 8) : (cx + 6);
    int mic_y = is_singing ? (10 + off_y) : (14 + off_y);
    draw_spr_rect(spr_base, base_tile, 4, mic_x, mic_y, 4, 4, 7);
    draw_spr_rect(spr_base, base_tile, 4, mic_x + 1, mic_y + 4, 2, 3, 1);
}

/* -------------------------------------------------------------------------- */
/* Palette Setup                                                              */
/* -------------------------------------------------------------------------- */
static void init_palettes(void) {
    /* === BG Palette Bank 0: Stage Atmosphere === */
    BG_PALETTE[0]  = RGB5(1, 0, 3);     /* Deepest void purple */
    BG_PALETTE[1]  = RGB5(4, 2, 8);     /* Brick wall purple */
    BG_PALETTE[2]  = RGB5(7, 3, 14);    /* Brick mortar highlight */
    BG_PALETTE[3]  = RGB5(2, 1, 5);     /* Lane dark highway */
    BG_PALETTE[4]  = RGB5(12, 6, 22);   /* Lane highway neon purple border */
    BG_PALETTE[5]  = RGB5(6, 3, 12);    /* Lane divider line */
    BG_PALETTE[6]  = RGB5(18, 12, 30);  /* Strumline glow */
    BG_PALETTE[7]  = RGB5(31, 31, 31);  /* Pure white font */
    BG_PALETTE[8]  = RGB5(28, 4, 6);    /* Health bar Opponent Red */
    BG_PALETTE[9]  = RGB5(0, 24, 28);   /* Health bar BF Cyan */
    BG_PALETTE[10] = RGB5(3, 2, 6);     /* Speaker cone dark */
    BG_PALETTE[11] = RGB5(14, 8, 22);   /* Speaker rim light */
    BG_PALETTE[12] = RGB5(10, 5, 18);   /* Stage floor plank */
    BG_PALETTE[13] = RGB5(14, 8, 26);   /* Spotlight glow */
    BG_PALETTE[14] = RGB5(5, 3, 12);    /* Panel fill */
    BG_PALETTE[15] = RGB5(18, 8, 30);   /* Panel border neon */

    /* === BG Palette Bank 1: Neon Magenta (Title & Highlights) === */
    BG_PALETTE[1 * 16 + 0] = RGB5(1, 0, 3);
    BG_PALETTE[1 * 16 + 7] = RGB5(31, 4, 18);   /* Hot pink neon font */
    BG_PALETTE[1 * 16 + 2] = RGB5(31, 14, 24);

    /* === BG Palette Bank 2: Electric Cyan === */
    BG_PALETTE[2 * 16 + 0] = RGB5(1, 0, 3);
    BG_PALETTE[2 * 16 + 7] = RGB5(0, 28, 31);   /* Cyan font */
    BG_PALETTE[2 * 16 + 2] = RGB5(14, 31, 31);

    /* === BG Palette Bank 3: Gold / Yellow === */
    BG_PALETTE[3 * 16 + 0] = RGB5(1, 0, 3);
    BG_PALETTE[3 * 16 + 7] = RGB5(31, 28, 0);   /* FNF Gold font */

    /* === BG Palette Bank 4: Lime Green === */
    BG_PALETTE[4 * 16 + 0] = RGB5(1, 0, 3);
    BG_PALETTE[4 * 16 + 7] = RGB5(6, 31, 8);    /* Lime green font */

    /* === BG Palette Bank 5: Crimson Red === */
    BG_PALETTE[5 * 16 + 0] = RGB5(1, 0, 3);
    BG_PALETTE[5 * 16 + 7] = RGB5(31, 6, 6);    /* Crimson red font */

    /* === BG Palette Bank 6: Dim Lavender === */
    BG_PALETTE[6 * 16 + 0] = RGB5(1, 0, 3);
    BG_PALETTE[6 * 16 + 7] = RGB5(14, 10, 20);  /* Muted secondary font */

    /* === BG Palette Bank 7: Orange === */
    BG_PALETTE[7 * 16 + 0] = RGB5(1, 0, 3);
    BG_PALETTE[7 * 16 + 7] = RGB5(31, 18, 2);   /* Orange font */

    /* ================================================================ */
    /* Sprite Palettes                                                  */
    /* ================================================================ */

    /* Bank 0: Left Arrow & Pressed Receptor (FNF Purple) */
    SPRITE_PALETTE[0 * 16 + 0] = 0;
    SPRITE_PALETTE[0 * 16 + 1] = RGB5(3, 0, 7);
    SPRITE_PALETTE[0 * 16 + 2] = RGB5(20, 3, 28);
    SPRITE_PALETTE[0 * 16 + 3] = RGB5(30, 14, 31);

    /* Bank 1: Down Arrow & Pressed Receptor (FNF Cyan) */
    SPRITE_PALETTE[1 * 16 + 1] = RGB5(0, 3, 7);
    SPRITE_PALETTE[1 * 16 + 2] = RGB5(0, 22, 30);
    SPRITE_PALETTE[1 * 16 + 3] = RGB5(18, 31, 31);

    /* Bank 2: Up Arrow & Pressed Receptor (FNF Lime Green) */
    SPRITE_PALETTE[2 * 16 + 1] = RGB5(0, 4, 0);
    SPRITE_PALETTE[2 * 16 + 2] = RGB5(4, 28, 6);
    SPRITE_PALETTE[2 * 16 + 3] = RGB5(20, 31, 20);

    /* Bank 3: Right Arrow & Pressed Receptor (FNF Scarlet Red) */
    SPRITE_PALETTE[3 * 16 + 1] = RGB5(6, 0, 0);
    SPRITE_PALETTE[3 * 16 + 2] = RGB5(30, 4, 6);
    SPRITE_PALETTE[3 * 16 + 3] = RGB5(31, 20, 18);

    /* Bank 4: Receptor Idle (Ghostly translucent outline) */
    SPRITE_PALETTE[4 * 16 + 1] = RGB5(2, 1, 4);
    SPRITE_PALETTE[4 * 16 + 2] = RGB5(8, 5, 14);
    SPRITE_PALETTE[4 * 16 + 3] = RGB5(16, 12, 24);

    /* Bank 5: Hit Burst & Flash */
    SPRITE_PALETTE[5 * 16 + 1] = RGB5(14, 12, 0);
    SPRITE_PALETTE[5 * 16 + 2] = RGB5(31, 28, 8);
    SPRITE_PALETTE[5 * 16 + 3] = RGB5(31, 31, 31);

    /* Bank 6: SICK Judgement (Gold/Rainbow) */
    SPRITE_PALETTE[6 * 16 + 1] = RGB5(4, 2, 8);
    SPRITE_PALETTE[6 * 16 + 2] = RGB5(31, 26, 0);
    SPRITE_PALETTE[6 * 16 + 3] = RGB5(31, 31, 31);

    /* Bank 7: GOOD Judgement (Emerald/Mint) */
    SPRITE_PALETTE[7 * 16 + 1] = RGB5(0, 4, 2);
    SPRITE_PALETTE[7 * 16 + 2] = RGB5(2, 28, 18);
    SPRITE_PALETTE[7 * 16 + 3] = RGB5(20, 31, 28);

    /* Bank 8: BAD Judgement (Orange) */
    SPRITE_PALETTE[8 * 16 + 1] = RGB5(4, 2, 0);
    SPRITE_PALETTE[8 * 16 + 2] = RGB5(31, 16, 2);
    SPRITE_PALETTE[8 * 16 + 3] = RGB5(31, 28, 12);

    /* Bank 9: MISS Judgement (Crimson) */
    SPRITE_PALETTE[9 * 16 + 1] = RGB5(4, 0, 0);
    SPRITE_PALETTE[9 * 16 + 2] = RGB5(28, 2, 4);
    SPRITE_PALETTE[9 * 16 + 3] = RGB5(31, 16, 16);

    /* Bank 10: Boyfriend Character & Icon */
    SPRITE_PALETTE[10 * 16 + 1] = RGB5(2, 2, 8);    /* Dark outline */
    SPRITE_PALETTE[10 * 16 + 2] = RGB5(28, 4, 6);   /* Cap / Shoes Red */
    SPRITE_PALETTE[10 * 16 + 3] = RGB5(0, 24, 30);  /* Hair Cyan */
    SPRITE_PALETTE[10 * 16 + 4] = RGB5(31, 23, 19); /* Skin Peach */
    SPRITE_PALETTE[10 * 16 + 5] = RGB5(31, 31, 31); /* Shirt White */
    SPRITE_PALETTE[10 * 16 + 6] = RGB5(4, 8, 20);   /* Pants Blue */
    SPRITE_PALETTE[10 * 16 + 7] = RGB5(22, 22, 24); /* Mic Silver */
    SPRITE_PALETTE[10 * 16 + 8] = RGB5(16, 2, 4);   /* Visor Dark Red */
    SPRITE_PALETTE[10 * 16 + 9] = RGB5(16, 29, 31); /* Hair Highlight */

    /* Bank 11: Opponent Character & Icon */
    SPRITE_PALETTE[11 * 16 + 1] = RGB5(2, 1, 3);    /* Dark outline */
    SPRITE_PALETTE[11 * 16 + 2] = RGB5(22, 16, 26); /* Skin Lilac */
    SPRITE_PALETTE[11 * 16 + 3] = RGB5(28, 4, 20);  /* Hair Magenta */
    SPRITE_PALETTE[11 * 16 + 4] = RGB5(4, 3, 7);    /* Coat Black */
    SPRITE_PALETTE[11 * 16 + 5] = RGB5(31, 28, 0);  /* Eyes Yellow */
    SPRITE_PALETTE[11 * 16 + 6] = RGB5(28, 4, 6);   /* Tie Red */
    SPRITE_PALETTE[11 * 16 + 7] = RGB5(20, 20, 22); /* Mic Silver */
    SPRITE_PALETTE[11 * 16 + 8] = RGB5(27, 21, 30); /* Highlight */
    SPRITE_PALETTE[11 * 16 + 9] = RGB5(31, 14, 26); /* Hair Glow */

    /* Bank 12: Cursor (Gold) */
    SPRITE_PALETTE[12 * 16 + 1] = RGB5(4, 4, 0);
    SPRITE_PALETTE[12 * 16 + 2] = RGB5(31, 28, 0);
    SPRITE_PALETTE[12 * 16 + 3] = RGB5(31, 31, 24);
}

/* -------------------------------------------------------------------------- */
/* Background Tile Init                                                       */
/* -------------------------------------------------------------------------- */
static void init_background_tiles(void) {
    u32* bg_tiles = (u32*)CHAR_BASE_ADR(BG0_CBB);

    /* Tile 0: Void */
    memset(&bg_tiles[0 * 8], 0, 32);

    /* Tile 1: Brick wall fill (Color 1) */
    for (int y = 0; y < 8; y++) bg_tiles[1 * 8 + y] = 0x11111111;

    /* Tile 2: Brick wall mortar line (Color 2 stripe) */
    for (int y = 0; y < 8; y++) {
        if (y == 7) bg_tiles[2 * 8 + y] = 0x22222222;
        else bg_tiles[2 * 8 + y] = 0x11111111;
    }

    /* Tile 3: Lane highway dark (Color 3) */
    for (int y = 0; y < 8; y++) bg_tiles[3 * 8 + y] = 0x33333333;

    /* Tile 4: Lane left border glow */
    for (int y = 0; y < 8; y++) bg_tiles[4 * 8 + y] = 0x33333354;

    /* Tile 5: Lane right border glow */
    for (int y = 0; y < 8; y++) bg_tiles[5 * 8 + y] = 0x45333333;

    /* Tile 6: Strumline target line glow (Color 6) */
    for (int y = 0; y < 8; y++) {
        if (y == 3 || y == 4) bg_tiles[6 * 8 + y] = 0x66666666;
        else bg_tiles[6 * 8 + y] = 0x33333333;
    }

    /* Tile 7: Health Bar Outer Frame */
    for (int y = 0; y < 8; y++) {
        if (y == 0 || y == 7) bg_tiles[7 * 8 + y] = 0x44444444;
        else bg_tiles[7 * 8 + y] = 0x00000000;
    }

    /* Tile 8: Health Bar Opponent Red Fill */
    for (int y = 0; y < 8; y++) {
        if (y == 0 || y == 7) bg_tiles[8 * 8 + y] = 0x44444444;
        else bg_tiles[8 * 8 + y] = 0x88888888;
    }

    /* Tile 9: Health Bar BF Cyan Fill */
    for (int y = 0; y < 8; y++) {
        if (y == 0 || y == 7) bg_tiles[9 * 8 + y] = 0x44444444;
        else bg_tiles[9 * 8 + y] = 0x99999999;
    }

    /* Tile 10: Subwoofer speaker rim */
    for (int y = 0; y < 8; y++) bg_tiles[10 * 8 + y] = 0xBBBBBBBB;

    /* Tile 11: Subwoofer speaker cone (dark) */
    for (int y = 0; y < 8; y++) {
        if (y == 3 || y == 4) bg_tiles[11 * 8 + y] = 0xAAAAAAAA;
        else bg_tiles[11 * 8 + y] = 0x00000000;
    }

    /* Tile 12: Subwoofer speaker bump (expanded light) */
    for (int y = 0; y < 8; y++) {
        if (y >= 2 && y <= 5) bg_tiles[12 * 8 + y] = 0xBBBBBBBB;
        else bg_tiles[12 * 8 + y] = 0xAAAAAAAA;
    }

    /* Tile 13: Stage wooden floor plank (Color 12) */
    for (int y = 0; y < 8; y++) {
        if (y == 0) bg_tiles[13 * 8 + y] = 0x44444444;
        else bg_tiles[13 * 8 + y] = 0xCCCCCCCC;
    }

    /* Tile 14: Panel fill (Color 14) */
    for (int y = 0; y < 8; y++) bg_tiles[14 * 8 + y] = 0xEEEEEEEE;

    /* Tile 15: Panel border neon (Color 15) */
    for (int y = 0; y < 8; y++) {
        if (y == 0 || y == 7) bg_tiles[15 * 8 + y] = 0xFFFFFFFF;
        else bg_tiles[15 * 8 + y] = 0xEEEEEEEE;
    }

    /* Tiles 16..61: Font Glyphs */
    for (int c = 0; c < 46; c++) {
        u32* font_tile = &bg_tiles[(16 + c) * 8];
        for (int y = 0; y < 8; y++) {
            u8 bits = font_8x8_data[c][y];
            u32 row_val = 0;
            for (int x = 0; x < 8; x++) {
                if (bits & (1 << (7 - x))) {
                    row_val |= (7U << (x * 4));
                }
            }
            font_tile[y] = row_val;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Sprite Tile Initialization                                                 */
/* -------------------------------------------------------------------------- */
static void init_sprite_tiles(void) {
    u32* spr = (u32*)OBJ_BASE_ADR;

    /* 1. Solid Neon Arrows */
    gen_sprite_3col(&spr[SPR_TILE_LEFT * 8],  mask_left,  inner_left,  2, 3, 1);
    gen_sprite_3col(&spr[SPR_TILE_DOWN * 8],  mask_down,  inner_down,  2, 3, 1);
    gen_sprite_3col(&spr[SPR_TILE_UP * 8],    mask_up,    inner_up,    2, 3, 1);
    gen_sprite_3col(&spr[SPR_TILE_RIGHT * 8], mask_right, inner_right, 2, 3, 1);

    /* 2. Directional Hollow Receptors */
    gen_sprite_2col(&spr[SPR_TILE_REC_LEFT * 8],  mask_rec_left,  2, 1);
    gen_sprite_2col(&spr[SPR_TILE_REC_DOWN * 8],  mask_rec_down,  2, 1);
    gen_sprite_2col(&spr[SPR_TILE_REC_UP * 8],    mask_rec_up,    2, 1);
    gen_sprite_2col(&spr[SPR_TILE_REC_RIGHT * 8], mask_rec_right, 2, 1);

    /* 3. Hit Burst */
    gen_sprite_2col(&spr[SPR_TILE_BURST * 8], mask_burst, 3, 2);

    /* 4. Menu Cursor */
    gen_sprite_2col(&spr[SPR_TILE_CURSOR * 8], mask_cursor, 2, 1);

    /* 5. Judgement Badges (32x16) */
    gen_judgement_badges(spr);

    /* 6. Health Icons (16x16) */
    gen_health_icons(spr);

    /* 7. Boyfriend Character Poses (32x32) */
    gen_bf_pose(spr, SPR_TILE_BF_IDLE,  0);
    gen_bf_pose(spr, SPR_TILE_BF_LEFT,  1);
    gen_bf_pose(spr, SPR_TILE_BF_DOWN,  2);
    gen_bf_pose(spr, SPR_TILE_BF_UP,    3);
    gen_bf_pose(spr, SPR_TILE_BF_RIGHT, 4);
    gen_bf_pose(spr, SPR_TILE_BF_MISS,  5);

    /* 8. Opponent Character Poses (32x32) */
    gen_opp_pose(spr, SPR_TILE_OPP_IDLE, false);
    gen_opp_pose(spr, SPR_TILE_OPP_SING, true);
}

/* -------------------------------------------------------------------------- */
/* Screen Clear & Hardware Sprites Clear                                      */
/* -------------------------------------------------------------------------- */
static void clear_screen_map(void) {
    u16* bg_map = (u16*)SCREEN_BASE_BLOCK(BG0_SBB);
    for (int i = 0; i < 32 * 32; i++) bg_map[i] = 0;
}

static void clear_all_sprites(void) {
    for (int i = 0; i < 128; i++) {
        shadow_oam[i].attr0 = ATTR0_DISABLED;
        shadow_oam[i].attr1 = 0;
        shadow_oam[i].attr2 = 0;
        shadow_oam[i].dummy = 0;
    }
}

/* -------------------------------------------------------------------------- */
/* Screen Renderers                                                           */
/* -------------------------------------------------------------------------- */

static void render_title_screen(void) {
    clear_screen_map();
    clear_all_sprites();
    u16* bg_map = (u16*)SCREEN_BASE_BLOCK(BG0_SBB);

    /* Dark purple brick background */
    for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 30; col++) {
            bg_map[row * 32 + col] = (row == 4 || row == 10 || row == 16) ? 2 : 1;
        }
    }

    /* Left & Right Speaker stacks with bump animation */
    bool bump = ((anim_frame >> 3) & 1);
    for (int row = 5; row < 15; row++) {
        bg_map[row * 32 + 1] = bump ? 12 : 11;
        bg_map[row * 32 + 2] = bump ? 12 : 11;
        bg_map[row * 32 + 27] = bump ? 12 : 11;
        bg_map[row * 32 + 28] = bump ? 12 : 11;
    }

    /* Stylized FNF Demake Header */
    draw_text(3, 2, "FRIDAY NIGHT FUNKIN-", 1); /* Neon Pink */
    draw_text(7, 4, "GBA DEMAKE", 3);            /* Gold */

    /* Girlfriend Silhouette / Boombox in Center */
    fill_row(8, 10, 20, 10);
    fill_row(9, 10, 20, bump ? 12 : 11);
    fill_row(10, 10, 20, bump ? 12 : 11);
    draw_text(11, 7, "* GF *", 1);

    /* Pulsing Press Start */
    if ((anim_frame & 32) < 20) {
        draw_text(5, 12, ">> PRESS START <<", ((anim_frame >> 4) & 1) ? 1 : 2);
    }

    /* Controls Guide */
    draw_text(2, 15, "CONTROLS:", 2);
    draw_text(2, 16, "D-PAD: LEFT/DOWN/UP/RIGHT", 6);
    draw_text(2, 17, "CHORDS: D-PAD + B/A", 3);
    draw_text(4, 18, "GHOST TAPPING: ON", 4);
}

static void render_song_select_screen(void) {
    clear_screen_map();
    clear_all_sprites();
    u16* bg_map = (u16*)SCREEN_BASE_BLOCK(BG0_SBB);

    for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 30; col++) {
            bg_map[row * 32 + col] = 1;
        }
    }

    /* Freeplay Header */
    fill_row(0, 0, 30, 15);
    draw_text(5, 0, "FREEPLAY - TRACK LIST", 3);

    int total_songs = get_song_count();
    for (int i = 0; i < total_songs && i < 4; i++) {
        const Song* s = get_song(i);
        int row = 3 + i * 2;

        if (i == selected_song_idx) {
            /* Active Song Highlight Capsule */
            fill_row(row, 2, 28, 14);

            /* Animated Cursor Sprite */
            shadow_oam[OAM_CURSOR].attr0 = ATTR0_Y(row * 8) | ATTR0_COLOR_16 | ATTR0_SQUARE;
            shadow_oam[OAM_CURSOR].attr1 = ATTR1_X(12) | ATTR1_SIZE_16;
            shadow_oam[OAM_CURSOR].attr2 = ATTR2_TILE(SPR_TILE_CURSOR) |
                                          ATTR2_PALETTE(PAL_SPR_CURSOR) | ATTR2_PRIORITY(0);

            draw_text(5, row, s->title, 1); /* Neon Pink */
        } else {
            draw_text(5, row, s->title, 6); /* Dim Lavender */
        }
    }

    /* Info Panel */
    const Song* cur = get_song(selected_song_idx);
    fill_row(12, 2, 28, 15);
    fill_row(17, 2, 28, 15);
    for (int r = 13; r < 17; r++) fill_row(r, 2, 28, 14);

    draw_text(3, 13, "ARTIST:", 2);
    draw_text(11, 13, cur->artist, 7);

    draw_text(3, 14, "DIFF  :", 2);
    draw_text(11, 14, cur->difficulty, (selected_song_idx == 0) ? 4 : (selected_song_idx == 1) ? 7 : 5);

    /* Star difficulty rating */
    if (selected_song_idx == 0)      draw_text(19, 14, "*..", 3);
    else if (selected_song_idx == 1) draw_text(19, 14, "**.", 3);
    else                             draw_text(19, 14, "***", 3);

    draw_text(3, 15, "BPM   :", 2);
    draw_number(11, 15, cur->bpm, 3, 0);

    draw_text(17, 15, "NOTES:", 2);
    draw_number(24, 15, get_song_total_notes(cur), 3, 0);

    draw_text(3, 16, "BEST  :", 2);
    draw_number(11, 16, high_scores[selected_song_idx], 6, 3);

    draw_text(3, 18, "A: PLAY   B: TITLE", 6);
}

static void render_gameplay_background(void) {
    clear_screen_map();
    u16* bg_map = (u16*)SCREEN_BASE_BLOCK(BG0_SBB);

    /* Stage brick backdrop */
    for (int row = 0; row < 17; row++) {
        for (int col = 0; col < 30; col++) {
            bg_map[row * 32 + col] = (row == 4 || row == 8) ? 2 : 1;
        }
    }

    /* Left Speaker Stack (Cols 1..3, Rows 6..14) */
    for (int row = 6; row < 14; row++) {
        bg_map[row * 32 + 1] = 11;
        bg_map[row * 32 + 2] = 11;
    }

    /* Right Speaker Stack (Cols 27..28, Rows 6..14) */
    for (int row = 6; row < 14; row++) {
        bg_map[row * 32 + 27] = 11;
        bg_map[row * 32 + 28] = 11;
    }

    /* 4 Highway Lanes (centered: cols 9..20) */
    const int lane_cols[LANE_COUNT] = { 9, 12, 15, 18 };
    for (int l = 0; l < LANE_COUNT; l++) {
        int st = lane_cols[l];
        for (int row = 1; row < 17; row++) {
            bg_map[row * 32 + st - 1] = 4;
            bg_map[row * 32 + st + 0] = (row == (LANE_TARGET_Y / 8)) ? 6 : 3;
            bg_map[row * 32 + st + 1] = (row == (LANE_TARGET_Y / 8)) ? 6 : 3;
            bg_map[row * 32 + st + 2] = 5;
        }
    }

    /* Stage Floor Plank (Row 17) */
    for (int col = 0; col < 30; col++) {
        bg_map[17 * 32 + col] = 13;
    }

    /* Health Bar Outer Frame (Row 18, Cols 6..23) */
    for (int col = 6; col < 24; col++) {
        bg_map[18 * 32 + col] = 7;
    }

    /* Static HUD Labels on Row 19 */
    draw_text(1, 19, "SCORE:", 2);
    draw_text(14, 19, "COMBO:", 2);
    draw_text(24, 19, "MS:", 5);
}

static void update_gameplay_hud(bool on_beat) {
    u16* bg_map = (u16*)SCREEN_BASE_BLOCK(BG0_SBB);

    /* Pulse Speaker Cones on beat */
    u16 spk_tile = on_beat ? 12 : 11;
    for (int row = 8; row < 12; row++) {
        bg_map[row * 32 + 1] = spk_tile;
        bg_map[row * 32 + 2] = spk_tile;
        bg_map[row * 32 + 27] = spk_tile;
        bg_map[row * 32 + 28] = spk_tile;
    }

    /* Health Bar: 18 tiles total across cols 6..23 */
    int split_idx = (health * 18) / 100;
    if (split_idx < 0) split_idx = 0;
    if (split_idx > 18) split_idx = 18;

    for (int i = 0; i < 18; i++) {
        bg_map[18 * 32 + 6 + i] = (i < split_idx) ? 9 : 8;
    }

    /* Score: Row 19, Cols 7..12 */
    draw_number(7, 19, score, 6, 0);

    /* Combo: Row 19, Cols 20..22 */
    draw_number(20, 19, combo, 3, (combo >= 10) ? 3 : 0);

    /* Misses: Row 19, Cols 27..28 */
    draw_number(27, 19, (count_miss > 99) ? 99 : count_miss, 2, 5);
}

static void render_results_screen(void) {
    clear_screen_map();
    clear_all_sprites();
    u16* bg_map = (u16*)SCREEN_BASE_BLOCK(BG0_SBB);

    for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 30; col++) {
            bg_map[row * 32 + col] = 1;
        }
    }

    fill_row(1, 2, 28, 15);
    draw_text(6, 1, "TRACK CLEARED!", 3);

    if (current_song) {
        draw_text(5, 3, current_song->title, 1);
    }

    /* Stats Panel */
    for (int r = 5; r < 14; r++) fill_row(r, 2, 20, 14);

    draw_text(3, 6, "SCORE   :", 2);
    draw_number(13, 6, score, 6, 3);

    draw_text(3, 7, "MAX CMBO:", 2);
    draw_number(13, 7, max_combo, 4, 0);

    draw_text(3, 8, "SICK!   :", 3);
    draw_number(13, 8, count_sick, 4, 3);

    draw_text(3, 9, "GOOD    :", 4);
    draw_number(13, 9, count_good, 4, 4);

    draw_text(3, 10, "BAD     :", 7);
    draw_number(13, 10, count_bad, 4, 7);

    draw_text(3, 11, "MISS    :", 5);
    draw_number(13, 11, count_miss, 4, 5);

    /* Accuracy % */
    int total_hits = count_sick + count_good + count_bad + count_miss;
    int accuracy = 100;
    if (total_hits > 0) {
        accuracy = (count_sick * 300 + count_good * 200 + count_bad * 100) * 100 /
                   (total_hits * 300);
    }
    draw_text(3, 12, "ACCURACY:", 2);
    draw_number(13, 12, accuracy, 3, 0);
    draw_text(16, 12, "%", 0);

    /* Rank Box */
    fill_row(5, 22, 28, 15);
    fill_row(9, 22, 28, 15);
    for (int r = 6; r < 9; r++) fill_row(r, 22, 28, 14);

    draw_text(23, 6, "RANK", 2);

    const char* rank = "S";
    u8 rank_pal = 3;
    if (count_miss == 0 && count_bad == 0 && count_good == 0) { rank = "P"; rank_pal = 3; }
    else if (accuracy >= 95) { rank = "S"; rank_pal = 3; }
    else if (accuracy >= 90) { rank = "A"; rank_pal = 4; }
    else if (accuracy >= 80) { rank = "B"; rank_pal = 2; }
    else if (accuracy >= 70) { rank = "C"; rank_pal = 7; }
    else if (accuracy >= 60) { rank = "D"; rank_pal = 1; }
    else { rank = "F"; rank_pal = 5; }

    draw_text(24, 8, rank, rank_pal);

    draw_text(4, 16, ">> PRESS A TO RETURN <<", 6);
}

/* -------------------------------------------------------------------------- */
/* Gameplay Engine & Sprite Update                                            */
/* -------------------------------------------------------------------------- */
static void start_gameplay(int song_idx) {
    selected_song_idx = song_idx;
    current_song = get_song(song_idx);

    score = 0;
    combo = 0;
    max_combo = 0;
    count_sick = 0;
    count_good = 0;
    count_bad = 0;
    count_miss = 0;
    health = 50;
    song_tick = 0;
    next_chart_idx = 0;
    current_judgement = JUDGE_NONE;
    judgement_timer = 0;
    bf_pose = 0;
    bf_pose_timer = 0;
    opp_sing_timer = 0;
    total_song_notes = get_song_total_notes(current_song);

    for (int i = 0; i < LANE_COUNT; i++) lane_flash_timer[i] = 0;
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        active_notes[i].active = false;
        active_notes[i].hit = false;
        active_notes[i].missed = false;
    }

    clear_all_sprites();
    render_gameplay_background();
    update_gameplay_hud(false);
    current_state = STATE_GAMEPLAY;
}

static void spawn_song_notes(void) {
    if (!current_song) return;
    while (next_chart_idx < (int)current_song->event_count) {
        const ChartEvent* ev = &current_song->events[next_chart_idx];
        if (song_tick + SPAWN_LEAD_FRAMES >= ev->tick) {
            for (int l = 0; l < LANE_COUNT; l++) {
                if (ev->lane_mask & (1 << l)) {
                    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
                        if (!active_notes[i].active) {
                            active_notes[i].active = true;
                            active_notes[i].lane = l;
                            active_notes[i].target_tick = ev->tick;
                            active_notes[i].hit = false;
                            active_notes[i].missed = false;
                            break;
                        }
                    }
                }
            }
            next_chart_idx++;
        } else break;
    }
}

static void register_hit(int lane, int note_idx, int diff_abs) {
    active_notes[note_idx].hit = true;
    active_notes[note_idx].active = false;
    lane_flash_timer[lane] = HIT_FLASH_TIME;

    /* BF strikes lane pose */
    bf_pose = lane + 1;
    bf_pose_timer = 10;

    if (diff_abs <= SICK_WINDOW) {
        score += 300; combo++; count_sick++;
        current_judgement = JUDGE_SICK;
        health += 4;
    } else if (diff_abs <= GOOD_WINDOW) {
        score += 200; combo++; count_good++;
        current_judgement = JUDGE_GOOD;
        health += 2;
    } else if (diff_abs <= BAD_WINDOW) {
        score += 100; combo++; count_bad++;
        current_judgement = JUDGE_BAD;
        health += 1;
    } else {
        score += 50; combo++; count_bad++;
        current_judgement = JUDGE_BAD;
    }

    if (health > 100) health = 100;
    judgement_timer = JUDGEMENT_TIME;
    if (combo > max_combo) max_combo = combo;
}

static void try_hit_lane(int lane, bool is_held_check) {
    int closest_idx = -1;
    int min_diff = 9999;

    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (active_notes[i].active && active_notes[i].lane == lane &&
            !active_notes[i].hit && !active_notes[i].missed) {
            int diff = (int)song_tick - (int)active_notes[i].target_tick;
            int diff_abs = (diff < 0) ? -diff : diff;
            if (diff_abs < min_diff) {
                min_diff = diff_abs;
                closest_idx = i;
            }
        }
    }

    if (closest_idx >= 0) {
        int max_window = is_held_check ? GOOD_WINDOW : MISS_WINDOW;
        if (min_diff <= max_window) {
            u16 chord_tick = active_notes[closest_idx].target_tick;
            register_hit(lane, closest_idx, min_diff);

            /* Concurrent Chord Assist:
               If other notes exist at the exact same target tick, and their key is
               currently down or held, immediately register them too! */
            u16 down_keys = keysDown();
            u16 held_keys = keysHeld();
            for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
                if (active_notes[i].active && !active_notes[i].hit && !active_notes[i].missed &&
                    active_notes[i].target_tick == chord_tick && active_notes[i].lane != lane) {
                    int other_lane = active_notes[i].lane;
                    u16 mask = lane_key_masks[other_lane];
                    if ((down_keys & mask) || (held_keys & mask)) {
                        register_hit(other_lane, i, min_diff);
                    }
                }
            }
            return;
        }
    }

    if (!is_held_check) {
        /* Ghost tapping: visual flash only, no penalty */
        lane_flash_timer[lane] = 3;
    }
}

static void check_misses(void) {
    for (int i = 0; i < MAX_ACTIVE_NOTES; i++) {
        if (active_notes[i].active && !active_notes[i].hit && !active_notes[i].missed) {
            if (song_tick > active_notes[i].target_tick + MISS_WINDOW) {
                active_notes[i].missed = true;
                active_notes[i].active = false;
                count_miss++;
                combo = 0;
                health -= 6;
                if (health < 0) health = 0;
                current_judgement = JUDGE_MISS;
                judgement_timer = JUDGEMENT_TIME;

                /* BF Miss reaction */
                bf_pose = 5;
                bf_pose_timer = 14;
            }
        }
    }
}

static void handle_gameplay_input(u16 down, u16 held) {
    if (down & KEY_START) {
        current_state = STATE_SONG_SELECT;
        render_song_select_screen();
        return;
    }

    /* 1. Fresh key transitions (keysDown) */
    for (int l = 0; l < LANE_COUNT; l++) {
        if (down & lane_key_masks[l]) {
            try_hit_lane(l, false);
        }
    }

    /* 2. Held keys reaching target window (chord lenience) */
    for (int l = 0; l < LANE_COUNT; l++) {
        if (held & lane_key_masks[l]) {
            try_hit_lane(l, true);
        }
    }
}

static void update_gameplay_sprites(bool on_beat) {
    /* 1. Receptors (Slots 0..3) */
    const int rec_tiles[LANE_COUNT] = {
        SPR_TILE_REC_LEFT, SPR_TILE_REC_DOWN, SPR_TILE_REC_UP, SPR_TILE_REC_RIGHT
    };
    const int arrow_tiles[LANE_COUNT] = {
        SPR_TILE_LEFT, SPR_TILE_DOWN, SPR_TILE_UP, SPR_TILE_RIGHT
    };

    for (int l = 0; l < LANE_COUNT; l++) {
        bool hit = (lane_flash_timer[l] > 0);
        shadow_oam[OAM_RECEPTOR_BASE + l].attr0 = ATTR0_Y(LANE_TARGET_Y) | ATTR0_COLOR_16 | ATTR0_SQUARE;
        shadow_oam[OAM_RECEPTOR_BASE + l].attr1 = ATTR1_X(lane_x[l]) | ATTR1_SIZE_16;
        /* If hit, switch to full solid arrow and lane color */
        shadow_oam[OAM_RECEPTOR_BASE + l].attr2 = ATTR2_TILE(hit ? arrow_tiles[l] : rec_tiles[l]) |
                                                 ATTR2_PALETTE(hit ? l : PAL_SPR_REC_IDLE) |
                                                 ATTR2_PRIORITY(1);
    }

    /* 2. Hit Bursts (Slots 4..7) */
    for (int l = 0; l < LANE_COUNT; l++) {
        if (lane_flash_timer[l] > 0) {
            shadow_oam[OAM_BURST_BASE + l].attr0 = ATTR0_Y(LANE_TARGET_Y) | ATTR0_COLOR_16 | ATTR0_SQUARE;
            shadow_oam[OAM_BURST_BASE + l].attr1 = ATTR1_X(lane_x[l]) | ATTR1_SIZE_16;
            shadow_oam[OAM_BURST_BASE + l].attr2 = ATTR2_TILE(SPR_TILE_BURST) |
                                                  ATTR2_PALETTE(PAL_SPR_BURST) |
                                                  ATTR2_PRIORITY(0);
        } else {
            shadow_oam[OAM_BURST_BASE + l].attr0 = ATTR0_DISABLED;
        }
    }

    /* 3. Judgement Badge (Slot 8, 32x16 wide) */
    if (current_judgement != JUDGE_NONE) {
        int tile = SPR_TILE_JUDGE_SICK;
        int pal = PAL_SPR_SICK;
        if (current_judgement == JUDGE_GOOD)      { tile = SPR_TILE_JUDGE_GOOD; pal = PAL_SPR_GOOD; }
        else if (current_judgement == JUDGE_BAD)  { tile = SPR_TILE_JUDGE_BAD;  pal = PAL_SPR_BAD; }
        else if (current_judgement == JUDGE_MISS) { tile = SPR_TILE_JUDGE_MISS; pal = PAL_SPR_MISS; }

        int float_y = LANE_TARGET_Y - 28 - (JUDGEMENT_TIME - judgement_timer);
        if (float_y < 16) float_y = 16;

        shadow_oam[OAM_JUDGEMENT].attr0 = ATTR0_Y(float_y) | ATTR0_COLOR_16 | ATTR0_WIDE;
        shadow_oam[OAM_JUDGEMENT].attr1 = ATTR1_X(104) | ATTR1_SIZE_32;
        shadow_oam[OAM_JUDGEMENT].attr2 = ATTR2_TILE(tile) | ATTR2_PALETTE(pal) | ATTR2_PRIORITY(0);
    } else {
        shadow_oam[OAM_JUDGEMENT].attr0 = ATTR0_DISABLED;
    }

    /* 4. Sliding Health Bar Icons (Slots 9 & 10) */
    int divider_x = 48 + (health * 144) / 100;
    int hop_y = on_beat ? 2 : 0;
    int icon_y = 138 - hop_y;

    /* Opponent Icon (Facing right) */
    shadow_oam[OAM_ICON_OPP].attr0 = ATTR0_Y(icon_y) | ATTR0_COLOR_16 | ATTR0_SQUARE;
    shadow_oam[OAM_ICON_OPP].attr1 = ATTR1_X(divider_x - 14) | ATTR1_SIZE_16;
    shadow_oam[OAM_ICON_OPP].attr2 = ATTR2_TILE(SPR_TILE_ICON_OPP) |
                                     ATTR2_PALETTE(PAL_SPR_OPP) | ATTR2_PRIORITY(0);

    /* BF Icon (Facing left) */
    int bf_icon_tile = (health < 20) ? SPR_TILE_ICON_BF_LOSE : SPR_TILE_ICON_BF;
    shadow_oam[OAM_ICON_BF].attr0 = ATTR0_Y(icon_y) | ATTR0_COLOR_16 | ATTR0_SQUARE;
    shadow_oam[OAM_ICON_BF].attr1 = ATTR1_X(divider_x - 2) | ATTR1_SIZE_16;
    shadow_oam[OAM_ICON_BF].attr2 = ATTR2_TILE(bf_icon_tile) |
                                    ATTR2_PALETTE(PAL_SPR_BF) | ATTR2_PRIORITY(0);

    /* 5. Opponent Character (Slot 11, 32x32) */
    int opp_tile = (opp_sing_timer > 0) ? SPR_TILE_OPP_SING : SPR_TILE_OPP_IDLE;
    int opp_y = 86 + (on_beat ? 1 : 0);
    shadow_oam[OAM_CHAR_OPP].attr0 = ATTR0_Y(opp_y) | ATTR0_COLOR_16 | ATTR0_SQUARE;
    shadow_oam[OAM_CHAR_OPP].attr1 = ATTR1_X(18) | ATTR1_SIZE_32;
    shadow_oam[OAM_CHAR_OPP].attr2 = ATTR2_TILE(opp_tile) |
                                     ATTR2_PALETTE(PAL_SPR_OPP) | ATTR2_PRIORITY(1);

    /* 6. Boyfriend Character (Slot 12, 32x32) */
    int bf_tile = SPR_TILE_BF_IDLE;
    if (bf_pose == 1) bf_tile = SPR_TILE_BF_LEFT;
    else if (bf_pose == 2) bf_tile = SPR_TILE_BF_DOWN;
    else if (bf_pose == 3) bf_tile = SPR_TILE_BF_UP;
    else if (bf_pose == 4) bf_tile = SPR_TILE_BF_RIGHT;
    else if (bf_pose == 5) bf_tile = SPR_TILE_BF_MISS;

    int bf_y = 86 + ((bf_pose == 0 && on_beat) ? 1 : 0);
    shadow_oam[OAM_CHAR_BF].attr0 = ATTR0_Y(bf_y) | ATTR0_COLOR_16 | ATTR0_SQUARE;
    shadow_oam[OAM_CHAR_BF].attr1 = ATTR1_X(190) | ATTR1_SIZE_32;
    shadow_oam[OAM_CHAR_BF].attr2 = ATTR2_TILE(bf_tile) |
                                    ATTR2_PALETTE(PAL_SPR_BF) | ATTR2_PRIORITY(1);

    /* Clear slots 13..15 */
    shadow_oam[13].attr0 = ATTR0_DISABLED;
    shadow_oam[14].attr0 = ATTR0_DISABLED;
    shadow_oam[15].attr0 = ATTR0_DISABLED;

    /* 7. Falling Notes (Slots 16..127) */
    int oam_idx = OAM_NOTES_BASE;
    for (int i = 0; i < MAX_ACTIVE_NOTES && oam_idx < 128; i++) {
        if (active_notes[i].active && !active_notes[i].hit && !active_notes[i].missed) {
            int y = LANE_TARGET_Y - (int)((active_notes[i].target_tick - song_tick) * NOTE_SPEED);
            if (y >= 10 && y <= 160) {
                int l = active_notes[i].lane;
                shadow_oam[oam_idx].attr0 = ATTR0_Y(y) | ATTR0_COLOR_16 | ATTR0_SQUARE;
                shadow_oam[oam_idx].attr1 = ATTR1_X(lane_x[l]) | ATTR1_SIZE_16;
                shadow_oam[oam_idx].attr2 = ATTR2_TILE(arrow_tiles[l]) |
                                            ATTR2_PALETTE(l) | ATTR2_PRIORITY(1);
                oam_idx++;
            }
        }
    }

    while (oam_idx < 128) {
        shadow_oam[oam_idx++].attr0 = ATTR0_DISABLED;
    }
}

/* -------------------------------------------------------------------------- */
/* Main Function                                                              */
/* -------------------------------------------------------------------------- */
int main(void) {
    irqInit();
    irqSet(IRQ_VBLANK, vblank_intr);
    irqEnable(IRQ_VBLANK);

    REG_DISPCNT = MODE_0 | BG0_ON | OBJ_ON | OBJ_1D_MAP;
    REG_BG0CNT = BG_16_COLOR | BG_SIZE_0 | CHAR_BASE(BG0_CBB) |
                 SCREEN_BASE(BG0_SBB) | BG_PRIORITY(2);

    init_palettes();
    init_background_tiles();
    init_sprite_tiles();
    clear_all_sprites();

    current_state = STATE_TITLE;
    render_title_screen();

    u32 last_tick = global_ticks;

    while (1) {
        VBlankIntrWait();
        dmaCopy(shadow_oam, OAM, sizeof(shadow_oam));

        scanKeys();
        u16 down = keysDown();
        u16 held = keysHeld();

        u32 current_ticks = global_ticks;
        u32 delta = current_ticks - last_tick;
        if (delta == 0) delta = 1;
        last_tick = current_ticks;

        switch (current_state) {
            case STATE_TITLE:
                anim_frame++;
                if ((anim_frame & 7) == 0) render_title_screen();
                if (down & (KEY_START | KEY_A)) {
                    current_state = STATE_SONG_SELECT;
                    render_song_select_screen();
                }
                break;

            case STATE_SONG_SELECT:
                anim_frame++;
                if (down & KEY_UP) {
                    selected_song_idx = (selected_song_idx - 1 + get_song_count()) % get_song_count();
                    render_song_select_screen();
                } else if (down & KEY_DOWN) {
                    selected_song_idx = (selected_song_idx + 1) % get_song_count();
                    render_song_select_screen();
                } else if (down & (KEY_A | KEY_START)) {
                    start_gameplay(selected_song_idx);
                } else if (down & KEY_B) {
                    current_state = STATE_TITLE;
                    render_title_screen();
                }
                break;

            case STATE_GAMEPLAY: {
                int bpm = current_song ? current_song->bpm : 120;
                int frames_per_beat = (60 * 60) / bpm;
                if (frames_per_beat <= 0) frames_per_beat = 30;

                for (u32 d = 0; d < delta; d++) {
                    song_tick++;
                    spawn_song_notes();
                    check_misses();

                    for (int l = 0; l < LANE_COUNT; l++) {
                        if (lane_flash_timer[l] > 0) lane_flash_timer[l]--;
                    }
                    if (judgement_timer > 0) {
                        judgement_timer--;
                        if (judgement_timer == 0) current_judgement = JUDGE_NONE;
                    }
                    if (bf_pose_timer > 0) {
                        bf_pose_timer--;
                        if (bf_pose_timer == 0) bf_pose = 0;
                    }
                    if (opp_sing_timer > 0) {
                        opp_sing_timer--;
                    } else if ((song_tick % (frames_per_beat * 2)) == 0) {
                        /* Opponent periodically sings */
                        opp_sing_timer = 12;
                    }
                }

                bool on_beat = ((song_tick % frames_per_beat) < 4);

                handle_gameplay_input(down, held);
                update_gameplay_hud(on_beat);
                update_gameplay_sprites(on_beat);

                if (current_song && song_tick >= current_song->total_ticks) {
                    if (score > high_scores[selected_song_idx])
                        high_scores[selected_song_idx] = score;
                    clear_all_sprites();
                    render_results_screen();
                    current_state = STATE_RESULTS;
                }
                break;
            }

            case STATE_RESULTS:
                if (down & (KEY_A | KEY_START | KEY_B)) {
                    current_state = STATE_SONG_SELECT;
                    render_song_select_screen();
                }
                break;
        }
    }

    return 0;
}

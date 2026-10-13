#ifndef SONG_H
#define SONG_H

#include <gba.h>
#include <stdbool.h>

/*
    ===========================================================================
    Modular & Reusable Rhythm Game Charting Engine
    ===========================================================================
    This module defines the song, chart, and event structures.
    Authoring a new song is as simple as defining a const ChartEvent array
    and registering it into the song catalog.
*/

/* 4-Lane Rhythm Game Identifiers */
typedef enum {
    LANE_LEFT  = 0,
    LANE_DOWN  = 1,
    LANE_UP    = 2,
    LANE_RIGHT = 3
} Lane;

#define LANE_COUNT 4

/* Lane Bitmasks for Simultaneous Chords & Jumps */
#define LANE_M_LEFT   (1 << LANE_LEFT)   /* 0b0001 */
#define LANE_M_DOWN   (1 << LANE_DOWN)   /* 0b0010 */
#define LANE_M_UP     (1 << LANE_UP)     /* 0b0100 */
#define LANE_M_RIGHT  (1 << LANE_RIGHT)  /* 0b1000 */

/* Convenient Short-Hand Note Constants */
#define N_L           LANE_M_LEFT
#define N_D           LANE_M_DOWN
#define N_U           LANE_M_UP
#define N_R           LANE_M_RIGHT

/* Double Chords */
#define N_LD          (N_L | N_D)
#define N_LU          (N_L | N_U)
#define N_LR          (N_L | N_R)
#define N_DU          (N_D | N_U)
#define N_DR          (N_D | N_R)
#define N_UR          (N_U | N_R)

/* Triple Chords */
#define N_LDU         (N_L | N_D | N_U)
#define N_LDR         (N_L | N_D | N_R)
#define N_LUR         (N_L | N_U | N_R)
#define N_DUR         (N_D | N_U | N_R)

/* Quadruple Chord */
#define N_ALL         (N_L | N_D | N_U | N_R)

/*
    Chart Event:
    - tick: Frame timestamp @ 60 FPS (VBlank synced)
    - lane_mask: Bitmask of lanes active at this tick (supports simultaneous notes)
*/
typedef struct {
    u16 tick;
    u8 lane_mask;
} ChartEvent;

/*
    Song Definition Structure:
    - title: Track title
    - artist: Composer / Week
    - difficulty: "EASY", "NORMAL", "HARD"
    - bpm: Beats Per Minute
    - total_ticks: Full song duration in frames
    - event_count: Number of chart events
    - events: Pointer to ROM chart event array
*/
typedef struct {
    const char* title;
    const char* artist;
    const char* difficulty;
    u16 bpm;
    u16 total_ticks;
    u16 event_count;
    const ChartEvent* events;
} Song;

/* Helper Macro to Calculate Frame Ticks from Musical Measure, Beat, and 16th */
/* At 60 FPS, 1 beat (quarter note) = (60 * 60) / BPM = 3600 / BPM frames */
#define MEASURE_TICK(bpm, measure, beat, sub16) \
    ((u16)((((measure) * 4 + (beat)) * 3600 + (sub16) * 900) / (bpm)))

/* Song Catalog Accessors */
int get_song_count(void);
const Song* get_song(int index);
u16 get_song_total_notes(const Song* song);

#endif /* SONG_H */

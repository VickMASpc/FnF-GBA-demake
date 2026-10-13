#include "song.h"

/*
    ===========================================================================
    Friday Night Funkin' Song Charts
    ===========================================================================
    All chords are charted with natural, ergonomic pairings:
    - N_LD (Left + Down)
    - N_UR (Up + Right)
    - N_LU (Left + Up)
    - N_DR (Down + Right)
    - N_LR (Left + Right, via Spread controls: DPAD_LEFT + KEY_A)
    - N_DU (Down + Up, via Spread controls: DPAD_DOWN + KEY_B)

    Supports both single D-pad and Two-Handed (DPAD + B/A) playstyles.
*/

/* -------------------------------------------------------------------------- */
/* Track 1: "BOPEEBO" (BPM 100 - Relaxed groove, easy timing, gentle chords)  */
/* -------------------------------------------------------------------------- */
static const ChartEvent bopeebo_events[] = {
    /* Measure 1: Slow quarter note intro */
    {  60, N_L },
    {  96, N_D },
    { 132, N_U },
    { 168, N_R },

    /* Measure 2: Groove back */
    { 204, N_U },
    { 240, N_D },
    { 276, N_L },
    { 312, N_R },

    /* Measure 3: First double chords (Left+Down, Up+Right) */
    { 348, N_LD },
    { 384, N_UR },
    { 420, N_LD },
    { 456, N_UR },

    /* Measure 4: 8th note run */
    { 492, N_L },
    { 510, N_D },
    { 528, N_U },
    { 546, N_R },
    { 564, N_U },
    { 582, N_D },
    { 600, N_L },
    { 618, N_R },

    /* Measure 5: Syncopated beats */
    { 654, N_L },
    { 672, N_U },
    { 690, N_D },
    { 726, N_R },
    { 744, N_UR },
    { 780, N_LD },

    /* Measure 6: Ending cadence */
    { 816, N_D },
    { 834, N_U },
    { 852, N_L },
    { 870, N_R },
    { 906, N_UR }
};

/* -------------------------------------------------------------------------- */
/* Track 2: "FRESH" (BPM 120 - Upbeat dance tempo, syncopation, double jumps) */
/* -------------------------------------------------------------------------- */
static const ChartEvent fresh_events[] = {
    /* Measure 1: Funk intro */
    {  60, N_L },
    {  90, N_D },
    { 105, N_U },
    { 120, N_R },
    { 150, N_U },
    { 165, N_D },
    { 180, N_L },

    /* Measure 2: Double jumps (Chords) */
    { 210, N_LD },
    { 240, N_UR },
    { 270, N_LD },
    { 300, N_UR },

    /* Measure 3: 8th note syncopation */
    { 330, N_L },
    { 345, N_D },
    { 360, N_U },
    { 375, N_R },
    { 390, N_U },
    { 405, N_D },
    { 420, N_LU },

    /* Measure 4: Rhythm rolls */
    { 450, N_D },
    { 465, N_D },
    { 480, N_U },
    { 495, N_U },
    { 510, N_L },
    { 525, N_R },
    { 540, N_DR },

    /* Measure 5: Syncopated bounce */
    { 570, N_L },
    { 577, N_D },
    { 585, N_U },
    { 600, N_R },
    { 615, N_D },
    { 630, N_LD },

    /* Measure 6: Climax chords */
    { 660, N_LD },
    { 690, N_UR },
    { 720, N_LU },
    { 750, N_DR },
    { 780, N_UR }
};

/* -------------------------------------------------------------------------- */
/* Track 3: "DAD BATTLE" (BPM 150 - Fast, 16th bursts, chords, high intensity)*/
/* -------------------------------------------------------------------------- */
static const ChartEvent dad_battle_events[] = {
    /* Measure 1: High speed intro */
    {  60, N_L },
    {  72, N_D },
    {  84, N_U },
    {  96, N_R },
    { 108, N_U },
    { 120, N_D },
    { 132, N_LD },

    /* Measure 2: Fast chord alternating */
    { 156, N_UR },
    { 180, N_LD },
    { 204, N_LU },
    { 228, N_DR },

    /* Measure 3: 16th note staircase roll */
    { 252, N_L },
    { 258, N_D },
    { 264, N_U },
    { 270, N_R },
    { 276, N_U },
    { 282, N_D },
    { 288, N_L },
    { 300, N_LD },

    /* Measure 4: Triplet syncopation */
    { 324, N_U },
    { 336, N_D },
    { 348, N_L },
    { 360, N_R },
    { 372, N_UR },
    { 396, N_LD },

    /* Measure 5: Intense rhythmic bursts */
    { 420, N_L },
    { 426, N_D },
    { 432, N_U },
    { 438, N_R },
    { 444, N_LD },
    { 456, N_UR },
    { 468, N_LU },
    { 480, N_DR },

    /* Measure 6: Climax finale */
    { 504, N_L },
    { 510, N_D },
    { 516, N_U },
    { 522, N_R },
    { 528, N_LD },
    { 540, N_UR },
    { 552, N_UR }
};

/* -------------------------------------------------------------------------- */
/* Song Catalog Registry                                                      */
/* -------------------------------------------------------------------------- */
static const Song song_catalog[] = {
    {
        .title = "BOPEEBO",
        .artist = "WEEK 1 - DADDY DEAREST",
        .difficulty = "EASY",
        .bpm = 100,
        .total_ticks = 980,
        .event_count = sizeof(bopeebo_events) / sizeof(bopeebo_events[0]),
        .events = bopeebo_events
    },
    {
        .title = "FRESH",
        .artist = "WEEK 1 - DADDY DEAREST",
        .difficulty = "NORMAL",
        .bpm = 120,
        .total_ticks = 860,
        .event_count = sizeof(fresh_events) / sizeof(fresh_events[0]),
        .events = fresh_events
    },
    {
        .title = "DAD BATTLE",
        .artist = "WEEK 1 - DADDY DEAREST",
        .difficulty = "HARD",
        .bpm = 150,
        .total_ticks = 620,
        .event_count = sizeof(dad_battle_events) / sizeof(dad_battle_events[0]),
        .events = dad_battle_events
    }
};

static const int CATALOG_COUNT = sizeof(song_catalog) / sizeof(song_catalog[0]);

int get_song_count(void) {
    return CATALOG_COUNT;
}

const Song* get_song(int index) {
    if (index < 0 || index >= CATALOG_COUNT) return &song_catalog[0];
    return &song_catalog[index];
}

u16 get_song_total_notes(const Song* song) {
    u16 count = 0;
    for (u16 i = 0; i < song->event_count; i++) {
        u8 mask = song->events[i].lane_mask;
        for (int l = 0; l < LANE_COUNT; l++) {
            if (mask & (1 << l)) count++;
        }
    }
    return count;
}

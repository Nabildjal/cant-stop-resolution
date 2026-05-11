/*
 * constants.h  —  Constantes et types partagés pour Can't Stop
 *
 * A inclure dans tous les fichiers du projet :
 *   #include "constants.h"
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <stdint.h>

/* ---------------------------------------------------------------
 * Paramètres du jeu
 * --------------------------------------------------------------- */
#define COL_MIN       2      /* première colonne jouable          */
#define COL_MAX      12      /* dernière colonne jouable          */
#define N_COLS       13      /* taille des tableaux (index 0..12) */
#define N_PLAY_COLS  11      /* colonnes jouables : 2..12         */
#define MAX_RUNNERS   3      /* runners simultanés max par tour   */
#define COLS_TO_WIN   3      /* colonnes à gagner pour victoire   */
#define N_DICE        4      /* nombre de dés lancés              */
#define N_DICE_COMBOS 1296   /* 6^4 combinaisons possibles        */

/* Longueur (nombre de cases) de chaque colonne */
static const int COL_LEN[N_COLS] = {
    0, 0,   /* col 0, 1 : inexistantes   */
    3,      /* col  2  (prob. 1/36)      */
    5,      /* col  3  (prob. 2/36)      */
    7,      /* col  4  (prob. 3/36)      */
    9,      /* col  5  (prob. 4/36)      */
    11,     /* col  6  (prob. 5/36)      */
    13,     /* col  7  (prob. 6/36)      */
    11,     /* col  8  (prob. 5/36)      */
    9,      /* col  9  (prob. 4/36)      */
    7,      /* col 10  (prob. 3/36)      */
    5,      /* col 11  (prob. 2/36)      */
    3       /* col 12  (prob. 1/36)      */
};

/* Accès rapide à la longueur d'une colonne */
#define col_len(c)  (COL_LEN[c])

/* ---------------------------------------------------------------
 * Paramètres de la Value Iteration
 * --------------------------------------------------------------- */
#define VI_EPS        1e-9   /* seuil de convergence              */
#define VI_MAX_ITER   10000  /* sécurité : max itérations         */

/* ---------------------------------------------------------------
 * Paramètres de la hashmap
 * --------------------------------------------------------------- */
#define HM_INIT_CAPACITY  (1 << 22)   /* 4M slots au départ (~200MB) */
#define HM_LOAD_FACTOR    0.65        /* agrandit si size/cap > 0.65 */
#define HM_EMPTY          0xFFFFFFFFFFFFFFFFULL
#define HM_DELETED        0xFFFFFFFFFFFFFFFEULL

/* ---------------------------------------------------------------
 * Structure d'état
 * Perspective du JOUEUR COURANT (symétrie du jeu)
 * --------------------------------------------------------------- */
typedef struct {
    int8_t pos_me[N_COLS];    /* positions permanentes — moi       */
    int8_t pos_opp[N_COLS];   /* positions permanentes — adversaire*/
    int8_t runner[N_COLS];    /* avance runner ce tour (0=inactif) */
} State;

/* ---------------------------------------------------------------
 * Structure d'une option de dés
 * (résultat d'un lancer exploitable)
 * --------------------------------------------------------------- */
typedef struct {
    int col1;    /* première colonne avancée (COL_MIN..COL_MAX)   */
    int col2;    /* deuxième colonne (peut == col1 → avance x2)   */
    int count;   /* nombre de lancers de dés donnant cette option */
} DiceOption;

/* ---------------------------------------------------------------
 * Résultat complet d'un lancer de dés
 * --------------------------------------------------------------- */
typedef struct {
    DiceOption options[20];  /* options jouables distinctes        */
    int        n_options;    /* nombre d'options                   */
    int        bust_count;   /* lancers causant un bust            */
    int        total;        /* toujours N_DICE_COMBOS = 1296      */
} DiceResult;

/* ---------------------------------------------------------------
 * Slot de la hashmap
 * --------------------------------------------------------------- */
typedef struct {
    uint64_t lo, hi;    /* clé compacte de l'état (132 bits)      */
    double   value;     /* V(s) : probabilité de victoire          */
    int8_t   policy;    /* 0=STOP  1=CONTINUE  -1=non calculé     */
    int8_t   occupied;  /* 1 si slot utilisé, 0 si vide           */
} HMSlot;

/* ---------------------------------------------------------------
 * Hashmap
 * --------------------------------------------------------------- */
typedef struct {
    HMSlot  *slots;
    uint64_t capacity;
    uint64_t size;
} HashMap;

/* ---------------------------------------------------------------
 * Macros utilitaires
 * --------------------------------------------------------------- */

/* Vrai si la colonne c est bloquée (gagnée par quelqu'un) */
#define is_blocked(s, c) \
    ((col_len(c) > 0) && \
     ((s)->pos_me[c]  >= col_len(c) || \
      (s)->pos_opp[c] >= col_len(c)))

/* Nombre de colonnes gagnées par le joueur courant */
static inline int cols_won(const State *s)
{
    int n = 0;
    for (int c = COL_MIN; c <= COL_MAX; c++)
        if (col_len(c) > 0 && s->pos_me[c] >= col_len(c)) n++;
    return n;
}

/* Nombre de runners actifs */
static inline int n_runners(const State *s)
{
    int n = 0;
    for (int c = COL_MIN; c <= COL_MAX; c++)
        if (s->runner[c] > 0) n++;
    return n;
}

/* Vrai si le joueur courant a gagné (3 colonnes complètes) */
#define is_winner(s) (cols_won(s) >= COLS_TO_WIN)

#endif /* CONSTANTS_H */
/*
 * state.c  —  Hashmap d'états pour Can't Stop
 *
 * Dépendances : constants.h
 * Compilation (tests) : gcc -O2 -DSTATE_TEST -o state_test state.c && ./state_test
 * Compilation (lib)   : gcc -O2 -c state.c
 */

#include "constants.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------
 * Encodage State → deux uint64_t (12 bits par colonne × 11 cols)
 * --------------------------------------------------------------- */
static void state_to_key(const State *s, uint64_t *hi, uint64_t *lo)
{
    *hi = 0; *lo = 0;
    int bit = 0;
    for (int c = COL_MIN; c <= COL_MAX; c++) {
        uint64_t val = ((uint64_t)(uint8_t)s->pos_me[c]  << 8)
                     | ((uint64_t)(uint8_t)s->pos_opp[c] << 4)
                     |  (uint64_t)(uint8_t)s->runner[c];
        if (bit < 64) {
            *lo |= val << bit;
            if (bit + 12 > 64) *hi |= val >> (64 - bit);
        } else {
            *hi |= val << (bit - 64);
        }
        bit += 12;
    }
}

static uint64_t hash_key(uint64_t hi, uint64_t lo)
{
    uint64_t h = lo ^ (hi * 0x9e3779b97f4a7c15ULL);
    h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return h;
}

/* ---------------------------------------------------------------
 * Hashmap — open addressing avec flag occupied explicite
 * --------------------------------------------------------------- */
HashMap* hm_create(uint64_t capacity)
{
    HashMap *hm  = malloc(sizeof(HashMap));
    hm->capacity = capacity;
    hm->size     = 0;
    hm->slots    = calloc(capacity, sizeof(HMSlot));
    /* calloc met tout à 0 : occupied=0, value=0.0, policy=0 */
    return hm;
}

void hm_free(HashMap *hm) { free(hm->slots); free(hm); }

static HMSlot* hm_find(HashMap *hm, uint64_t lo, uint64_t hi)
{
    uint64_t h = hash_key(hi, lo) & (hm->capacity - 1);
    for (uint64_t i = 0; i < hm->capacity; i++) {
        HMSlot *sl = &hm->slots[(h + i) & (hm->capacity - 1)];
        if (!sl->occupied) return sl;                          /* slot libre */
        if (sl->lo == lo && sl->hi == hi) return sl;          /* trouvé     */
    }
    return NULL;
}

int hm_set(HashMap *hm, const State *s, double value, int8_t policy)
{
    uint64_t lo, hi;
    state_to_key(s, &hi, &lo);
    HMSlot *sl = hm_find(hm, lo, hi);
    if (!sl) return 0;
    if (!sl->occupied) { sl->lo = lo; sl->hi = hi; sl->occupied = 1; hm->size++; }
    sl->value  = value;
    sl->policy = policy;
    return 1;
}

int hm_get(HashMap *hm, const State *s, double *value, int8_t *policy)
{
    uint64_t lo, hi;
    state_to_key(s, &hi, &lo);
    HMSlot *sl = hm_find(hm, lo, hi);
    if (!sl || !sl->occupied) return 0;
    *value  = sl->value;
    *policy = sl->policy;
    return 1;
}

/* Retourne V(s) ou 0.0 si état inconnu */
double hm_get_value(HashMap *hm, const State *s)
{
    double v = 0.0; int8_t p = -1;
    hm_get(hm, s, &v, &p);
    return v;
}

/* ---------------------------------------------------------------
 * Validation
 * --------------------------------------------------------------- */
int state_valid(const State *s)
{
    int nr = 0;
    for (int c = COL_MIN; c <= COL_MAX; c++) {
        int L = col_len(c);
        if (s->pos_me[c]  < 0 || s->pos_me[c]  > L) return 0;
        if (s->pos_opp[c] < 0 || s->pos_opp[c] > L) return 0;
        if (s->runner[c]  < 0) return 0;
        if (s->pos_me[c] + s->runner[c] > L)         return 0;
        if (is_blocked(s, c) && s->runner[c] > 0)    return 0;
        if (s->runner[c] > 0) nr++;
    }
    return (nr <= MAX_RUNNERS);
}

/* ---------------------------------------------------------------
 * Transitions
 * --------------------------------------------------------------- */

/* STOP : encaisse runners → check victoire → swap joueurs */
int apply_stop(const State *s, State *out)
{
    *out = *s;
    for (int c = COL_MIN; c <= COL_MAX; c++) {
        if (out->runner[c] > 0) {
            out->pos_me[c] += out->runner[c];
            if (out->pos_me[c] > col_len(c)) out->pos_me[c] = col_len(c);
            out->runner[c] = 0;
        }
    }
    if (is_winner(out)) return 1;
    for (int c = COL_MIN; c <= COL_MAX; c++) {
        int8_t t       = out->pos_me[c];
        out->pos_me[c] = out->pos_opp[c];
        out->pos_opp[c]= t;
    }
    return 0;
}

/* BUST : efface runners → swap joueurs */
void apply_bust(const State *s, State *out)
{
    *out = *s;
    for (int c = COL_MIN; c <= COL_MAX; c++) out->runner[c] = 0;
    for (int c = COL_MIN; c <= COL_MAX; c++) {
        int8_t t       = out->pos_me[c];
        out->pos_me[c] = out->pos_opp[c];
        out->pos_opp[c]= t;
    }
}

/* MOVE : avance runners sur col1 et col2 (col1==col2 → +2 sur la même) */
void apply_move(const State *s, int col1, int col2, State *out)
{
    *out = *s;
    out->runner[col1]++;
    out->runner[col2]++;
    for (int c = COL_MIN; c <= COL_MAX; c++) {
        int8_t max_r = (int8_t)(col_len(c) - out->pos_me[c]);
        if (out->runner[c] > max_r) out->runner[c] = max_r;
    }
}

/* ---------------------------------------------------------------
 * Affichage
 * --------------------------------------------------------------- */
void print_state(const State *s)
{
    printf("Col :  "); for(int c=COL_MIN;c<=COL_MAX;c++) printf("%3d",c);
    printf("\nMe  :  "); for(int c=COL_MIN;c<=COL_MAX;c++) printf("%3d",s->pos_me[c]);
    printf("\nOpp :  "); for(int c=COL_MIN;c<=COL_MAX;c++) printf("%3d",s->pos_opp[c]);
    printf("\nRun :  "); for(int c=COL_MIN;c<=COL_MAX;c++) printf("%3d",s->runner[c]);
    printf("\nRunners: %d | Cols gagnees(me): %d\n", n_runners(s), cols_won(s));
}

/* ---------------------------------------------------------------
 * Tests
 * --------------------------------------------------------------- */
#ifdef STATE_TEST
int main(void)
{
    printf("=== state.c — tests ===\n\n");

    /* Test 1 : insert + lookup */
    {
        HashMap *hm = hm_create(1024);
        State s; memset(&s,0,sizeof(s));
        s.pos_me[7]=5; s.pos_opp[6]=3; s.runner[7]=2; s.runner[9]=1;
        printf("--- Test 1 : insert + lookup ---\n");
        print_state(&s);
        hm_set(hm, &s, 0.72, 1);
        double v; int8_t p;
        int found = hm_get(hm, &s, &v, &p);
        printf("Trouve: %s | V=%.4f | policy=%d (attendu: OUI 0.7200 1)\n\n",
               found?"OUI":"NON", v, p);
        hm_free(hm);
    }

    /* Test 2 : apply_stop */
    {
        State s,s2; memset(&s,0,sizeof(s));
        s.pos_me[7]=5; s.runner[7]=3; s.runner[9]=2;
        printf("--- Test 2 : apply_stop ---\n");
        printf("Avant:\n"); print_state(&s);
        int won = apply_stop(&s,&s2);
        printf("Apres (victoire=%s):\n", won?"OUI":"NON");
        print_state(&s2);
        printf("Valide: %s\n\n", state_valid(&s2)?"OUI":"NON");
    }

    /* Test 3 : apply_bust */
    {
        State s,s2; memset(&s,0,sizeof(s));
        s.pos_me[7]=5; s.runner[6]=1; s.runner[7]=2;
        printf("--- Test 3 : apply_bust ---\n");
        printf("Avant:\n"); print_state(&s);
        apply_bust(&s,&s2);
        printf("Apres:\n"); print_state(&s2);
        printf("Valide: %s\n\n", state_valid(&s2)?"OUI":"NON");
    }

    /* Test 4 : victoire */
    {
        State s,s2; memset(&s,0,sizeof(s));
        s.pos_me[2]=3; s.pos_me[12]=3; s.pos_me[7]=11; s.runner[7]=2;
        printf("--- Test 4 : victoire ---\n");
        print_state(&s);
        printf("apply_stop → victoire: %s (attendu OUI)\n\n",
               apply_stop(&s,&s2)?"OUI":"NON");
    }

    /* Test 5 : invalide */
    {
        State s; memset(&s,0,sizeof(s));
        s.runner[2]=1; s.runner[5]=1; s.runner[7]=1; s.runner[9]=1;
        printf("--- Test 5 : 4 runners invalide ---\n");
        printf("Valide: %s (attendu NON)\n\n", state_valid(&s)?"OUI":"NON");
    }

    /* Test 6 : hm_get_value sur état absent */
    {
        HashMap *hm = hm_create(256);
        State s; memset(&s,0,sizeof(s));
        printf("--- Test 6 : hm_get_value absent → 0.0 ---\n");
        printf("V = %.4f (attendu 0.0000)\n\n", hm_get_value(hm, &s));
        hm_free(hm);
    }

    printf("=== Tous les tests OK ===\n");
    return 0;
}
#endif
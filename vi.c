/*
 * vi_fast.c — VI Can't Stop, col 7+8, avec table pré-calculée
 *
 * Optimisation clé : on pré-calcule une fois pour toutes
 * la table de transitions pour toutes les configurations
 * de runners possibles sur {7,8}.
 *
 * gcc -O2 -o vi_fast vi_fast.c dice.c state.c -lm && ./vi_fast
 */

#include "constants.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

void get_outcomes(const int[], int, const int[], DiceResult*);
int  apply_stop(const State*, State*);
void apply_bust(const State*, State*);
void apply_move(const State*, int, int, State*);

#define CA 7
#define CB 8
#define LA (col_len(CA)+1)   /* 14 */
#define LB (col_len(CB)+1)   /* 12 */
#define TOTAL (LA*LB*LA*LB*LA*LB)

static double *V;
static int8_t *P;

/* ---------------------------------------------------------------
 * Table pré-calculée des transitions
 *
 * Clé : (r7, r8, blocked7, blocked8) → 2+2+2 = 4 bits → 16 configs
 * Pour chaque config : DiceResult pré-calculé
 * --------------------------------------------------------------- */
#define N_CONFIGS 16   /* 2^4 : r7>0, r8>0, blk7, blk8 */
static DiceResult TRANS[N_CONFIGS];

static int config_key(int r7, int r8, int blk7, int blk8) {
    return (r7>0)*8 + (r8>0)*4 + blk7*2 + blk8;
}

static void precompute_transitions(void) {
    int blocked[N_COLS];
    for (int k=0; k<N_CONFIGS; k++) {
        int has_r7 = (k>>3)&1, has_r8 = (k>>2)&1;
        int blk7   = (k>>1)&1, blk8   = k&1;
        memset(blocked, 0, sizeof(blocked));
        blocked[CA] = blk7; blocked[CB] = blk8;
        int runners[2], n_run=0;
        if (has_r7 && !blk7) runners[n_run++]=CA;
        if (has_r8 && !blk8) runners[n_run++]=CB;
        get_outcomes(runners, n_run, blocked, &TRANS[k]);
    }
    printf("Table de transitions pré-calculée (%d configs)\n", N_CONFIGS);
}

/* ---------------------------------------------------------------
 * Encodage / décodage
 * --------------------------------------------------------------- */
static int encode6(int pm7,int pm8,int po7,int po8,int r7,int r8) {
    return pm7 + LA*(pm8 + LB*(po7 + LA*(po8 + LB*(r7 + LA*r8))));
}

static void decode6(int idx, int *pm7,int *pm8,int *po7,int *po8,
                    int *r7,int *r8) {
    *pm7=idx%LA; idx/=LA;
    *pm8=idx%LB; idx/=LB;
    *po7=idx%LA; idx/=LA;
    *po8=idx%LB; idx/=LB;
    *r7 =idx%LA; idx/=LA;
    *r8 =idx%LB;
}

static int ok6(int pm7,int pm8,int po7,int po8,int r7,int r8) {
    if (pm7>col_len(CA)||pm8>col_len(CB)) return 0;
    if (po7>col_len(CA)||po8>col_len(CB)) return 0;
    if (pm7+r7>col_len(CA)||pm8+r8>col_len(CB)) return 0;
    int blk7=(pm7>=col_len(CA)||po7>=col_len(CA));
    int blk8=(pm8>=col_len(CB)||po8>=col_len(CB));
    if (blk7&&r7>0) return 0;
    if (blk8&&r8>0) return 0;
    return ((r7>0)+(r8>0))<=MAX_RUNNERS;
}

/* ---------------------------------------------------------------
 * Bellman update — utilise la table pré-calculée
 * --------------------------------------------------------------- */
static double bellman6(int pm7,int pm8,int po7,int po8,int r7,int r8) {
    int blk7=(pm7>=col_len(CA)||po7>=col_len(CA));
    int blk8=(pm8>=col_len(CB)||po8>=col_len(CB));

    /* V_stop : encaisse runners, swap */
    int new_pm7=pm7+r7, new_pm8=pm8+r8;
    if (new_pm7>col_len(CA)) new_pm7=col_len(CA);
    if (new_pm8>col_len(CB)) new_pm8=col_len(CB);
    /* Victoire si 2 colonnes gagnées */
    int won = (new_pm7>=col_len(CA)) + (new_pm8>=col_len(CB)) >= 2;
    double vs;
    if (won) {
        vs = 1.0;
    } else {
        /* Après STOP : swap → adversaire joue depuis (po7,po8,new_pm7,new_pm8,0,0) */
        int idx_stop = encode6(po7,po8,new_pm7,new_pm8,0,0);
        vs = 1.0 - V[idx_stop];
    }

    /* V_continue : utilise table pré-calculée */
    int ck = config_key(r7,r8,blk7,blk8);
    const DiceResult *dr = &TRANS[ck];

    double vc = 0.0;

    /* Bust : perd runners, swap */
    int idx_bust = encode6(po7,po8,pm7,pm8,0,0);
    vc += (double)dr->bust_count * (1.0 - V[idx_bust]);

    /* Options */
    for (int i=0; i<dr->n_options; i++) {
        int c1=dr->options[i].col1, c2=dr->options[i].col2;
        /* Filtre : garde seulement CA et CB */
        if (c1!=CA&&c1!=CB&&c2!=CA&&c2!=CB) continue;
        if (c1!=CA&&c1!=CB) c1=c2;
        if (c2!=CA&&c2!=CB) c2=c1;

        /* Avance runner */
        int nr7=r7+(c1==CA)+(c2==CA);
        int nr8=r8+(c1==CB)+(c2==CB);
        /* Plafonne */
        int max7=col_len(CA)-pm7, max8=col_len(CB)-pm8;
        if (nr7>max7) nr7=max7;
        if (nr8>max8) nr8=max8;

        if (!ok6(pm7,pm8,po7,po8,nr7,nr8)) continue;
        int idx_move = encode6(pm7,pm8,po7,po8,nr7,nr8);
        vc += (double)dr->options[i].count * V[idx_move];
    }
    vc /= (double)N_DICE_COMBOS;

    return (vc>vs) ? vc : vs;
}

int main(void) {
    printf("=== Can't Stop — VI rapide col %d+%d ===\n\n", CA, CB);
    printf("TOTAL=%d (~%.0f MB)\n", TOTAL, TOTAL*8.0/1e6);

    precompute_transitions();

    V = calloc(TOTAL, sizeof(double));
    P = calloc(TOTAL, sizeof(int8_t));

    /* Terminaux : 2 colonnes gagnées */
    int n_terminal=0, n_valid=0;
    for (int idx=0; idx<TOTAL; idx++) {
        int pm7,pm8,po7,po8,r7,r8;
        decode6(idx,&pm7,&pm8,&po7,&po8,&r7,&r8);
        if (!ok6(pm7,pm8,po7,po8,r7,r8)) continue;
        n_valid++;
        if ((pm7>=col_len(CA))+(pm8>=col_len(CB))>=2) {
            V[idx]=1.0; n_terminal++;
        }
    }
    printf("États valides: %d | Terminaux: %d\n\n", n_valid, n_terminal);

    /* Value Iteration */
    int iter=0; double delta;
    printf("Value Iteration...\n");
    do {
        delta=0.0;
        for (int idx=0; idx<TOTAL; idx++) {
            int pm7,pm8,po7,po8,r7,r8;
            decode6(idx,&pm7,&pm8,&po7,&po8,&r7,&r8);
            if (!ok6(pm7,pm8,po7,po8,r7,r8)) continue;
            if ((pm7>=col_len(CA))+(pm8>=col_len(CB))>=2) continue;

            double old_v=V[idx];
            double new_v=bellman6(pm7,pm8,po7,po8,r7,r8);
            V[idx]=new_v;
            double d=fabs(new_v-old_v);
            if (d>delta) delta=d;
        }
        iter++;
        if (iter<=5||iter%20==0)
            printf("  Iter %3d — delta=%.4e\n",iter,delta);
    } while (delta>VI_EPS && iter<VI_MAX_ITER);

    printf("Convergé en %d itérations\n\n", iter);

    /* Policy */
    for (int idx=0; idx<TOTAL; idx++) {
        int pm7,pm8,po7,po8,r7,r8;
        decode6(idx,&pm7,&pm8,&po7,&po8,&r7,&r8);
        if (!ok6(pm7,pm8,po7,po8,r7,r8)) continue;
        if ((pm7>=col_len(CA))+(pm8>=col_len(CB))>=2) continue;

        int blk7=(pm7>=col_len(CA)||po7>=col_len(CA));
        int blk8=(pm8>=col_len(CB)||po8>=col_len(CB));
        int ck=config_key(r7,r8,blk7,blk8);
        const DiceResult *dr=&TRANS[ck];

        int new_pm7=pm7+r7,new_pm8=pm8+r8;
        if (new_pm7>col_len(CA)) new_pm7=col_len(CA);
        if (new_pm8>col_len(CB)) new_pm8=col_len(CB);
        int won=(new_pm7>=col_len(CA))+(new_pm8>=col_len(CB))>=2;
        double vs=won?1.0:(1.0-V[encode6(po7,po8,new_pm7,new_pm8,0,0)]);

        double vc=0.0;
        vc+=(double)dr->bust_count*(1.0-V[encode6(po7,po8,pm7,pm8,0,0)]);
        for (int i=0;i<dr->n_options;i++){
            int c1=dr->options[i].col1,c2=dr->options[i].col2;
            if(c1!=CA&&c1!=CB&&c2!=CA&&c2!=CB) continue;
            if(c1!=CA&&c1!=CB) c1=c2; if(c2!=CA&&c2!=CB) c2=c1;
            int nr7=r7+(c1==CA)+(c2==CA), nr8=r8+(c1==CB)+(c2==CB);
            int mx7=col_len(CA)-pm7, mx8=col_len(CB)-pm8;
            if(nr7>mx7)nr7=mx7; if(nr8>mx8)nr8=mx8;
            if(!ok6(pm7,pm8,po7,po8,nr7,nr8)) continue;
            vc+=(double)dr->options[i].count*V[encode6(pm7,pm8,po7,po8,nr7,nr8)];
        }
        vc/=N_DICE_COMBOS;
        P[idx]=(vc>vs)?1:0;
    }

    /* Résultats */
    printf("V(0,0,0,0,0,0) = %.6f  (état initial)\n\n", V[0]);

    printf("Policy : runner col7=r uniquement\n");
    printf("r  :"); for(int r=1;r<=col_len(CA);r++) printf(" %2d",r); printf("\n");
    printf("pol:"); for(int r=1;r<=col_len(CA);r++){
        if(!ok6(0,0,0,0,r,0)){printf("  -");continue;}
        printf("  %s",P[encode6(0,0,0,0,r,0)]?"C":"S");
    } printf("\n\n");

    printf("V(s) : runner7=5, pos_opp7 croissant\n");
    printf("po7  :"); for(int po=0;po<=13;po++) printf(" %2d",po); printf("\n");
    printf("V    :"); for(int po=0;po<=13;po++){
        if(!ok6(0,0,po,0,5,0)){printf("  - ");continue;}
        printf(" %.2f",V[encode6(0,0,po,0,5,0)]);
    } printf("\n\n");

    printf("V(s) : runner7=5, runner8=3, pos_opp=0\n");
    printf("V = %.4f\n", ok6(0,0,0,0,5,3)?V[encode6(0,0,0,0,5,3)]:0.0);

    free(V); free(P);
    return 0;
}

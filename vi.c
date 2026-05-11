/*
 * vi_corrected.c — VI Can't Stop, col 7+8, version corrigée
 *
 * Corrections :
 *   1. Bust correct : un lancer est un bust si AUCUNE des 3 paires
 *      ne contient CA ou CB (pas juste les busts "dés impossibles")
 *   2. V_continue : on somme sur les 1296 lancers directement,
 *      sans passer par get_outcomes qui filtre mal
 *   3. Table de transitions pré-calculée correctement
 *
 * gcc -O3 -o vi_corrected vi_corrected.c dice.c state.c -lm
 */

#include "constants.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* Déclarations externes */
int  apply_stop(const State*, State*);
void apply_bust(const State*, State*);

#define CA 7
#define CB 8
#define LA (col_len(CA)+1)   /* 14 */
#define LB (col_len(CB)+1)   /* 12 */
#define TOTAL (LA*LB*LA*LB*LA*LB)

static double *V;
static int8_t *P;

/* ---------------------------------------------------------------
 * Encodage / décodage
 * --------------------------------------------------------------- */
static inline int encode6(int pm7,int pm8,int po7,int po8,int r7,int r8) {
    return pm7 + LA*(pm8 + LB*(po7 + LA*(po8 + LB*(r7 + LA*r8))));
}

static inline void decode6(int idx,
                            int *pm7,int *pm8,int *po7,int *po8,
                            int *r7, int *r8) {
    *pm7=idx%LA; idx/=LA;
    *pm8=idx%LB; idx/=LB;
    *po7=idx%LA; idx/=LA;
    *po8=idx%LB; idx/=LB;
    *r7 =idx%LA; idx/=LA;
    *r8 =idx%LB;
}

static inline int ok6(int pm7,int pm8,int po7,int po8,int r7,int r8) {
    if (pm7>col_len(CA)||pm8>col_len(CB)) return 0;
    if (po7>col_len(CA)||po8>col_len(CB)) return 0;
    if (pm7+r7>col_len(CA)||pm8+r8>col_len(CB)) return 0;
    int blk7=(pm7>=col_len(CA)||po7>=col_len(CA));
    int blk8=(pm8>=col_len(CB)||po8>=col_len(CB));
    if (blk7&&r7>0) return 0;
    if (blk8&&r8>0) return 0;
    return ((r7>0)+(r8>0))<=MAX_RUNNERS;
}

static inline double getV(int pm7,int pm8,int po7,int po8,int r7,int r8) {
    if (!ok6(pm7,pm8,po7,po8,r7,r8)) return 0.0;
    return V[encode6(pm7,pm8,po7,po8,r7,r8)];
}

/* ---------------------------------------------------------------
 * Table pré-calculée des transitions
 *
 * Pour chaque état (r7, r8, blk7, blk8) et chaque lancer (d1..d4),
 * on stocke : pour chaque lancer, la meilleure option disponible
 * (delta_r7, delta_r8) = avance sur CA et CB.
 *
 * On pré-calcule les 1296 lancers une fois pour toutes.
 * Pour chaque lancer on stocke :
 *   - best_dr7, best_dr8 : avance optimale (à choisir par la VI)
 *   - is_bust : 1 si aucune paire ne touche CA ou CB
 *
 * En réalité on stocke TOUTES les options valides par lancer
 * pour que la VI puisse choisir le max.
 * --------------------------------------------------------------- */

/* Pour un lancer donné, liste des options (dr7, dr8) disponibles */
typedef struct {
    int dr7, dr8;   /* avance sur CA et CB (0 si colonne non touchée) */
} LancerOption;

typedef struct {
    LancerOption opts[3];  /* max 3 paires par lancer */
    int n_opts;             /* 0 = bust */
} LancerResult;

static LancerResult LANCERS[N_DICE_COMBOS];  /* pré-calculé */
static int LANCER_IDX = 0;

static void precompute_lancers(void) {
    LANCER_IDX = 0;
    for (int d1=1;d1<=6;d1++)
    for (int d2=1;d2<=6;d2++)
    for (int d3=1;d3<=6;d3++)
    for (int d4=1;d4<=6;d4++) {
        int pairs[3][2] = {
            {d1+d2, d3+d4},
            {d1+d3, d2+d4},
            {d1+d4, d2+d3}
        };
        LancerResult *lr = &LANCERS[LANCER_IDX++];
        lr->n_opts = 0;

        /* Collecte les paires distinctes qui touchent CA ou CB */
        for (int p=0; p<3; p++) {
            int c1=pairs[p][0], c2=pairs[p][1];
            int touches = (c1==CA||c1==CB||c2==CA||c2==CB);
            if (!touches) continue;

            /* Calcule dr7 et dr8 */
            int dr7 = (c1==CA) + (c2==CA);
            int dr8 = (c1==CB) + (c2==CB);

            /* Vérifie si cette option est déjà dans la liste */
            int found = 0;
            for (int i=0; i<lr->n_opts; i++)
                if (lr->opts[i].dr7==dr7 && lr->opts[i].dr8==dr8)
                    { found=1; break; }
            if (!found && lr->n_opts < 3) {
                lr->opts[lr->n_opts].dr7 = dr7;
                lr->opts[lr->n_opts].dr8 = dr8;
                lr->n_opts++;
            }
        }
        /* n_opts == 0 → bust pour ce lancer */
    }
    printf("Table lancers pre-calculee : %d lancers\n", LANCER_IDX);

    /* Vérification */
    int busts = 0;
    for (int i=0; i<N_DICE_COMBOS; i++)
        if (LANCERS[i].n_opts==0) busts++;
    printf("Lancers bust (aucune paire sur col %d ou %d) : %d/1296 = %.2f%%\n\n",
           CA, CB, busts, 100.0*busts/1296.0);
}

/* ---------------------------------------------------------------
 * Bellman update corrigé
 *
 * V_stop   : encaisse runners → check victoire → 1 - V(adversaire)
 * V_continue : pour chaque lancer, le joueur choisit la meilleure
 *              option disponible COMPATIBLE avec son état actuel
 * --------------------------------------------------------------- */
static double bellman6(int pm7,int pm8,int po7,int po8,int r7,int r8)
{
    int blk7 = (pm7>=col_len(CA) || po7>=col_len(CA));
    int blk8 = (pm8>=col_len(CB) || po8>=col_len(CB));

    /* --- V_stop --- */
    int new_pm7 = pm7+r7, new_pm8 = pm8+r8;
    if (new_pm7 > col_len(CA)) new_pm7 = col_len(CA);
    if (new_pm8 > col_len(CB)) new_pm8 = col_len(CB);
    int won = (new_pm7>=col_len(CA)) + (new_pm8>=col_len(CB)) >= 2;
    double vs;
    if (won) {
        vs = 1.0;
    } else {
        /* Après STOP : swap → adversaire joue depuis (po7,po8,new_pm7,new_pm8,0,0) */
        vs = 1.0 - getV(po7, po8, new_pm7, new_pm8, 0, 0);
    }

    /* --- V_continue --- */
    double vc = 0.0;
    int max_r7 = col_len(CA) - pm7;  /* runner max possible sur CA */
    int max_r8 = col_len(CB) - pm8;  /* runner max possible sur CB */

    for (int li = 0; li < N_DICE_COMBOS; li++) {
        LancerResult *lr = &LANCERS[li];

        if (lr->n_opts == 0) {
            /* Bust réel des dés (aucune paire sur CA ou CB) */
            /* + Bust fonctionnel : paires existent mais bloquées */
            double v_bust = 1.0 - getV(po7, po8, pm7, pm8, 0, 0);
            vc += v_bust;
            continue;
        }

        /* Parmi les options disponibles, garde celles compatibles */
        /* avec l'état actuel (runners max, colonnes bloquées) */
        double best = -1.0;
        int any_valid = 0;

        for (int oi=0; oi<lr->n_opts; oi++) {
            int dr7 = lr->opts[oi].dr7;
            int dr8 = lr->opts[oi].dr8;

            /* Vérifie compatibilité avec runners actifs et slots */
            /* Règle : on ne peut pas avancer sur une colonne bloquée */
            if (dr7 > 0 && blk7) { dr7 = 0; }
            if (dr8 > 0 && blk8) { dr8 = 0; }

            /* Vérifie slots runners disponibles */
            int nr = (r7>0) + (r8>0);
            int need_new7 = (r7==0 && dr7>0);
            int need_new8 = (r8==0 && dr8>0);
            if (nr + need_new7 + need_new8 > MAX_RUNNERS) {
                /* Essaie de n'avancer que sur une colonne */
                if (dr7>0 && nr + need_new7 <= MAX_RUNNERS) dr8=0;
                else if (dr8>0 && nr + need_new8 <= MAX_RUNNERS) dr7=0;
                else { /* Les deux impossibles → bust fonctionnel */
                    double v_bust = 1.0 - getV(po7,po8,pm7,pm8,0,0);
                    if (!any_valid || v_bust > best) best = v_bust;
                    any_valid = 1;
                    continue;
                }
            }

            if (dr7==0 && dr8==0) continue;

            /* Calcule nouveau runner */
            int nr7 = r7 + dr7;
            int nr8 = r8 + dr8;
            if (nr7 > max_r7) nr7 = max_r7;
            if (nr8 > max_r8) nr8 = max_r8;

            if (!ok6(pm7,pm8,po7,po8,nr7,nr8)) continue;

            double val = getV(pm7,pm8,po7,po8,nr7,nr8);
            if (!any_valid || val > best) best = val;
            any_valid = 1;
        }

        if (!any_valid) {
            /* Bust fonctionnel : aucune option valide */
            best = 1.0 - getV(po7, po8, pm7, pm8, 0, 0);
        }
        vc += best;
    }
    vc /= (double)N_DICE_COMBOS;

    return (vc > vs) ? vc : vs;
}

/* ---------------------------------------------------------------
 * main
 * --------------------------------------------------------------- */
int main(void)
{
    printf("=== Can't Stop - VI corrigee col %d+%d ===\n\n", CA, CB);
    printf("TOTAL=%d (~%.0f MB)\n\n", TOTAL, TOTAL*8.0/1e6);

    precompute_lancers();

    V = calloc(TOTAL, sizeof(double));
    P = calloc(TOTAL, sizeof(int8_t));
    if (!V || !P) { printf("Erreur allocation\n"); return 1; }

    /* Terminaux : 2 colonnes gagnées par le joueur courant */
    int n_valid=0, n_term=0;
    for (int idx=0; idx<TOTAL; idx++) {
        int pm7,pm8,po7,po8,r7,r8;
        decode6(idx,&pm7,&pm8,&po7,&po8,&r7,&r8);
        if (!ok6(pm7,pm8,po7,po8,r7,r8)) continue;
        n_valid++;
        if ((pm7>=col_len(CA))+(pm8>=col_len(CB))>=2)
            { V[idx]=1.0; n_term++; }
    }
    printf("Etats valides: %d | Terminaux: %d\n\n", n_valid, n_term);

    /* Value Iteration */
    int iter=0; double delta;
    printf("Value Iteration...\n");
    do {
        delta = 0.0;
        for (int idx=0; idx<TOTAL; idx++) {
            int pm7,pm8,po7,po8,r7,r8;
            decode6(idx,&pm7,&pm8,&po7,&po8,&r7,&r8);
            if (!ok6(pm7,pm8,po7,po8,r7,r8)) continue;
            if ((pm7>=col_len(CA))+(pm8>=col_len(CB))>=2) continue;

            double old_v = V[idx];
            double new_v = bellman6(pm7,pm8,po7,po8,r7,r8);
            V[idx] = new_v;
            double d = fabs(new_v - old_v);
            if (d > delta) delta = d;
        }
        iter++;
        if (iter<=5 || iter%50==0)
            printf("  Iter %4d - delta=%.4e\n", iter, delta);
    } while (delta > VI_EPS && iter < VI_MAX_ITER);

    printf("Converge en %d iterations (delta=%.2e)\n\n", iter, delta);

    /* Policy */
    for (int idx=0; idx<TOTAL; idx++) {
        int pm7,pm8,po7,po8,r7,r8;
        decode6(idx,&pm7,&pm8,&po7,&po8,&r7,&r8);
        if (!ok6(pm7,pm8,po7,po8,r7,r8)) continue;
        if ((pm7>=col_len(CA))+(pm8>=col_len(CB))>=2) continue;

        int new_pm7=pm7+r7, new_pm8=pm8+r8;
        if (new_pm7>col_len(CA)) new_pm7=col_len(CA);
        if (new_pm8>col_len(CB)) new_pm8=col_len(CB);
        int won=(new_pm7>=col_len(CA))+(new_pm8>=col_len(CB))>=2;
        double vs = won ? 1.0 : (1.0 - getV(po7,po8,new_pm7,new_pm8,0,0));

        double vc = 0.0;
        int max_r7=col_len(CA)-pm7, max_r8=col_len(CB)-pm8;
        int blk7=(pm7>=col_len(CA)||po7>=col_len(CA));
        int blk8=(pm8>=col_len(CB)||po8>=col_len(CB));

        for (int li=0; li<N_DICE_COMBOS; li++) {
            LancerResult *lr = &LANCERS[li];
            if (lr->n_opts==0) {
                vc += 1.0 - getV(po7,po8,pm7,pm8,0,0);
                continue;
            }
            double best=-1.0; int any=0;
            for (int oi=0;oi<lr->n_opts;oi++){
                int dr7=lr->opts[oi].dr7, dr8=lr->opts[oi].dr8;
                if(dr7>0&&blk7) dr7=0;
                if(dr8>0&&blk8) dr8=0;
                int nr=(r7>0)+(r8>0);
                int n7=(r7==0&&dr7>0), n8=(r8==0&&dr8>0);
                if(nr+n7+n8>MAX_RUNNERS){
                    if(dr7>0&&nr+n7<=MAX_RUNNERS) dr8=0;
                    else if(dr8>0&&nr+n8<=MAX_RUNNERS) dr7=0;
                    else { double vb=1.0-getV(po7,po8,pm7,pm8,0,0);
                           if(!any||vb>best) best=vb; any=1; continue; }
                }
                if(dr7==0&&dr8==0) continue;
                int nr7=r7+dr7, nr8=r8+dr8;
                if(nr7>max_r7)nr7=max_r7; if(nr8>max_r8)nr8=max_r8;
                if(!ok6(pm7,pm8,po7,po8,nr7,nr8)) continue;
                double val=getV(pm7,pm8,po7,po8,nr7,nr8);
                if(!any||val>best) best=val; any=1;
            }
            if(!any) best=1.0-getV(po7,po8,pm7,pm8,0,0);
            vc+=best;
        }
        vc/=N_DICE_COMBOS;
        P[idx]=(vc>vs)?1:0;
    }

    /* ---- Résultats ---- */
    printf("V(etat initial, tout=0) = %.6f\n\n", V[0]);

    printf("Policy : runner col%d=r uniquement (tout le reste=0)\n", CA);
    printf("r   :"); for(int r=1;r<=col_len(CA);r++) printf(" %2d",r); printf("\n");
    printf("pol :"); for(int r=1;r<=col_len(CA);r++){
        if(!ok6(0,0,0,0,r,0)){printf("  -");continue;}
        printf("  %s",P[encode6(0,0,0,0,r,0)]?"C":"S");
    } printf("\n\n");

    printf("Policy : runner col%d=r uniquement\n", CB);
    printf("r   :"); for(int r=1;r<=col_len(CB);r++) printf(" %2d",r); printf("\n");
    printf("pol :"); for(int r=1;r<=col_len(CB);r++){
        if(!ok6(0,0,0,0,0,r)){printf("  -");continue;}
        printf("  %s",P[encode6(0,0,0,0,0,r)]?"C":"S");
    } printf("\n\n");

    printf("V(s) : runner col%d=5, pos_opp%d croissant\n", CA, CA);
    printf("po7  :"); for(int po=0;po<=col_len(CA);po++) printf(" %2d",po); printf("\n");
    printf("V    :"); for(int po=0;po<=col_len(CA);po++){
        if(!ok6(0,0,po,0,5,0)){printf("   -");continue;}
        printf(" %.2f",getV(0,0,po,0,5,0));
    } printf("\n\n");

    printf("Policy runner col7=r, pos_me7 croissant (po=0)\n");
    printf("      "); for(int r=1;r<=8;r++) printf(" r=%d ",r); printf("\n");
    for(int pm=0;pm<=8;pm++){
        printf("pm7=%-2d",pm);
        for(int r=1;r<=8;r++){
            if(!ok6(pm,0,0,0,r,0)||pm+r>col_len(CA)){printf("  WIN");continue;}
            printf("  %s  ",P[encode6(pm,0,0,0,r,0)]?"C":"S");
        }
        printf("\n");
    }

    free(V); free(P);
    return 0;
}
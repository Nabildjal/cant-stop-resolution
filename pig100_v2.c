/*
 * pig100_v2.c  —  Pig GOAL=100, Value Iteration avec k*(i,j)
 *
 * Modification : affiche k*(i,j) pour plusieurs valeurs de j
 * (pas seulement j=0), comme demandé par le prof.
 *
 * Compilation : gcc -O2 -o pig100_v2 pig100_v2.c -lm
 * Exécution   : ./pig100_v2
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#define GOAL      100
#define EPS_LOCAL 1e-9

static double V[GOAL][GOAL][GOAL];
static char   policy[GOAL][GOAL][GOAL];

static inline double hold_value(int i, int j, int k)
{
    if (i + k >= GOAL) return 1.0;
    return 1.0 - V[j][i + k][0];
}

static inline double roll_value(int i, int j, int k)
{
    if (i + k >= GOAL) return 1.0;
    double exp = 0.0;
    exp += (1.0/6.0) * (1.0 - V[j][i][0]);
    for (int r = 2; r <= 6; r++) {
        if (i + k + r >= GOAL) exp += (1.0/6.0) * 1.0;
        else                   exp += (1.0/6.0) * V[i][j][k+r];
    }
    return exp;
}

static inline double bellman_update(int i, int j, int k) //comparewr entre roll et hold values et vhoisir le meilleur
{
    if (i + k >= GOAL) return 1.0;
    double h = hold_value(i, j, k);
    double r = roll_value(i, j, k);
    return (r > h) ? r : h;
}

static void local_iterate(int i, int j)
{
    double delta;
    do {
        delta = 0.0;
        for (int k = 0; k < GOAL - i; k++) {
            double old = V[i][j][k];
            double nw  = bellman_update(i, j, k);
            V[i][j][k] = nw;
            double d = fabs(nw - old);
            if (d > delta) delta = d;
        }
        if (i != j) {
            for (int k = 0; k < GOAL - j; k++) {
                double old = V[j][i][k];
                double nw  = bellman_update(j, i, k);
                V[j][i][k] = nw;
                double d = fabs(nw - old);
                if (d > delta) delta = d;
            }
        }
    } while (delta >= EPS_LOCAL);
}

int main(void)
{
    /* Init */
    memset(V,      0, sizeof(V));
    memset(policy, 0, sizeof(policy));
    for (int i = 0; i < GOAL; i++)
        for (int j = 0; j < GOAL; j++)
            for (int k = 0; k < GOAL; k++)
                if (i + k >= GOAL) V[i][j][k] = 1.0;

    /* Value Iteration avec ordering par somme décroissante */
    for (int S = 2*(GOAL-1); S >= 0; S--)
        for (int i = 0; i < GOAL; i++) {
            int j = S - i;
            if (j < 0 || j >= GOAL) continue;
            local_iterate(i, j);
        }

    /* Policy */
    for (int i = 0; i < GOAL; i++)
        for (int j = 0; j < GOAL; j++)
            for (int k = 0; k < GOAL; k++) {
                if (i + k >= GOAL) { policy[i][j][k] = 0; continue; }
                policy[i][j][k] = (roll_value(i,j,k) > hold_value(i,j,k)) ? 1 : 0;
            }

    printf("=== Pig (GOAL=%d) — Value Iteration ===\n\n", GOAL);
    printf("V(0,0,0) = %.8f  (attendu ~0.53056)\n\n", V[0][0][0]);

    /* -------------------------------------------------------
     * Table k*(i,j) pour plusieurs valeurs de j
     * k*(i,j) = plus petit k tel que policy[i][j][k] = HOLD
     * ------------------------------------------------------- */
    int j_vals[] = {0, 10, 20, 30, 50, 70, 90, 99};
    int nj = 8;

    printf("==== Seuil k*(i,j) - premier HOLD selon i ET j ====\n");
    printf("(Chaque colonne = un score d'adversaire j fixe)\n\n");

    /* En-tête */
    printf("%-6s", "i\\j");
    for (int jj = 0; jj < nj; jj++) printf("  j=%-3d", j_vals[jj]);
    printf("\n");
    printf("%-6s", "------");
    for (int jj = 0; jj < nj; jj++) printf("  -----");
    printf("\n");

    /* Lignes i = 0..77 (au-delà on gagne trop vite) */
    for (int i = 0; i < 78; i++) {
        printf("i=%-4d", i);
        for (int jj = 0; jj < nj; jj++) {
            int j = j_vals[jj];
            /* Cherche k* */
            int kstar = -1;
            for (int k = 0; k < GOAL - i; k++) {
                if (!policy[i][j][k]) { kstar = k; break; }
            }
            if (kstar >= 0) printf("  %-5d", kstar);
            else            printf("  ROLL ");
        }
        printf("\n");
    }

    /* -------------------------------------------------------
     * Analyse : impact de j sur k* pour i fixé
     * ------------------------------------------------------- */
    printf("\n==== Impact de j sur k* (i fixe) ====\n");
    printf("(Comment le score de l'adversaire change la strategie)\n\n");

    int i_vals[] = {0, 20, 40, 60, 70};
    int ni = 5;

    printf("%-6s", "j\\i");
    for (int ii = 0; ii < ni; ii++) printf("  i=%-3d", i_vals[ii]);
    printf("\n");
    printf("%-6s", "------");
    for (int ii = 0; ii < ni; ii++) printf("  -----");
    printf("\n");

    for (int j = 0; j < GOAL; j += 5) {
        printf("j=%-4d", j);
        for (int ii = 0; ii < ni; ii++) {
            int i = i_vals[ii];
            if (i >= GOAL) { printf("  -    "); continue; }
            int kstar = -1;
            for (int k = 0; k < GOAL - i; k++)
                if (!policy[i][j][k]) { kstar = k; break; }
            if (kstar >= 0) printf("  %-5d", kstar);
            else            printf("  ROLL ");
        }
        printf("\n");
    }

    /* Export CSV complet */
    FILE *f = fopen("pig_policy_ij.csv", "w");
    fprintf(f, "i,j,kstar\n");
    for (int i = 0; i < GOAL; i++)
        for (int j = 0; j < GOAL; j++) {
            int kstar = -1;
            for (int k = 0; k < GOAL - i; k++)
                if (!policy[i][j][k]) { kstar = k; break; }
            if (kstar >= 0) fprintf(f, "%d,%d,%d\n", i, j, kstar);
        }
    fclose(f);
    printf("\nExporte : pig_policy_ij.csv\n");

    return 0;
}

/*
 * dice.c  —  Table de transitions des 4 dés pour Can't Stop
 *
 * Dépendances : constants.h
 * Compilation : gcc -O2 -o dice_test dice.c && ./dice_test
 *
 * Fournit :
 *   get_outcomes(runners, n_runners, blocked, result)
 *   → remplit un DiceResult avec toutes les options jouables
 *     et le nombre de lancers causant un bust.
 */

#include "constants.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------
 * Interne : une paire (ca, cb) est compatible avec les runners si
 * au moins une des deux colonnes est jouable (runner existant ou
 * slot libre et colonne non bloquée).
 * --------------------------------------------------------------- */
static int pair_compatible(const int runners[], int n_run,
                           const int blocked[], int ca, int cb)
{
    int ca_runner = 0, cb_runner = 0;
    for (int r = 0; r < n_run; r++) {
        if (runners[r] == ca) ca_runner = 1;
        if (runners[r] == cb) cb_runner = 1;
    }
    /* ca jouable si déjà runner, ou slot libre et non bloqué */
    int ca_ok = ca_runner || (!blocked[ca] && n_run < MAX_RUNNERS);
    /* cb : si ca ouvre un nouveau slot, ça compte */
    int slots_after_ca = n_run + (ca_ok && !ca_runner ? 1 : 0);
    int cb_ok = cb_runner || (!blocked[cb] && slots_after_ca < MAX_RUNNERS);
    return ca_ok || cb_ok;
}

/* Ajoute ou fusionne (ca, cb) normalisé dans options[] */
static void add_option(DiceOption options[], int *n, int ca, int cb)
{
    if (ca > cb) { int t = ca; ca = cb; cb = t; }
    for (int i = 0; i < *n; i++)
        if (options[i].col1 == ca && options[i].col2 == cb)
            { options[i].count++; return; }
    options[*n].col1  = ca;
    options[*n].col2  = cb;
    options[*n].count = 1;
    (*n)++;
}

/* ---------------------------------------------------------------
 * get_outcomes() — fonction principale
 * --------------------------------------------------------------- */
void get_outcomes(const int runners[], int n_runners,
                  const int blocked[], DiceResult *result)
{
    memset(result, 0, sizeof(DiceResult));
    result->total = N_DICE_COMBOS;

    for (int d1=1;d1<=6;d1++)
    for (int d2=1;d2<=6;d2++)
    for (int d3=1;d3<=6;d3++)
    for (int d4=1;d4<=6;d4++) {
        int pairs[3][2] = {
            {d1+d2, d3+d4},
            {d1+d3, d2+d4},
            {d1+d4, d2+d3}
        };
        /* Collecte les paires compatibles distinctes pour ce lancer */
        int tmp_ca[3], tmp_cb[3], n_tmp = 0;
        for (int p = 0; p < 3; p++) {
            int ca = pairs[p][0], cb = pairs[p][1];
            if (!pair_compatible(runners, n_runners, blocked, ca, cb)) continue;
            int a = (ca<=cb)?ca:cb, b = (ca<=cb)?cb:ca;
            int found = 0;
            for (int t=0;t<n_tmp;t++)
                if (tmp_ca[t]==a && tmp_cb[t]==b) { found=1; break; }
            if (!found) { tmp_ca[n_tmp]=a; tmp_cb[n_tmp]=b; n_tmp++; }
        }
        if (n_tmp == 0) {
            result->bust_count++;
        } else {
            for (int t=0;t<n_tmp;t++)
                add_option(result->options, &result->n_options,
                           tmp_ca[t], tmp_cb[t]);
        }
    }
}

/* ---------------------------------------------------------------
 * Affichage
 * --------------------------------------------------------------- */
void print_dice_result(const DiceResult *r, const int runners[], int n_run)
{
    printf("Runners : ");
    for (int i=0;i<n_run;i++) printf("%d ",runners[i]);
    if (!n_run) printf("(aucun)");
    printf("\nBust : %d/1296 = %.2f%%\n",
           r->bust_count, 100.0*r->bust_count/1296.0);
    printf("Options (%d) :\n", r->n_options);
    for (int i=0;i<r->n_options;i++) {
        const DiceOption *o = &r->options[i];
        if (o->col1==o->col2)
            printf("  col %2d (x2) : %3d = %.2f%%\n",
                   o->col1, o->count, 100.0*o->count/1296.0);
        else
            printf("  col %2d+%2d  : %3d = %.2f%%\n",
                   o->col1,o->col2,o->count,100.0*o->count/1296.0);
    }
    printf("\n");
}

/* ---------------------------------------------------------------
 * Tests
 * --------------------------------------------------------------- */
#ifdef DICE_TEST
int main(void)
{
    int blocked[N_COLS]; memset(blocked, 0, sizeof(blocked));
    DiceResult r;

    printf("=== dice.c (refactorisé avec constants.h) ===\n\n");

    { int run[]={};    int n=0; get_outcomes(run,n,blocked,&r);
      printf("--- Aucun runner ---\n"); print_dice_result(&r,run,n); }

    { int run[]={7};   int n=1; get_outcomes(run,n,blocked,&r);
      printf("--- Runner sur col 7 ---\n"); print_dice_result(&r,run,n); }

    { int run[]={6,7,8}; int n=3; get_outcomes(run,n,blocked,&r);
      printf("--- Runners sur 6,7,8 ---\n"); print_dice_result(&r,run,n); }

    { int run[]={2,7,12}; int n=3; get_outcomes(run,n,blocked,&r);
      printf("--- Runners sur 2,7,12 (colonnes extremes) ---\n");
      print_dice_result(&r,run,n); }

    return 0;
}
#endif
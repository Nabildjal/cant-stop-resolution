from collections import defaultdict

GOAL = 2
epsilon = 1e-8

# Générer tous les états possibles
states = []
for i in range(GOAL):
    for j in range(GOAL):
        for k in range(GOAL+1):
            states.append((i, j, k))

V = defaultdict(float)

def is_terminal(i,j,k):
    return i + k >= GOAL

def hold_value(i,j,k):
    if i + k >= GOAL:
        return 1.0
    return 1 - V[(j, i+k, 0)]

def flip_value(i,j,k):
    if i + k >= GOAL:
        return 1.0
    lose = 1 - V[(j, i, 0)]
    win = V[(i, j, k+1)]
    return 0.5 * (lose + win)

# -------- VALUE ITERATION --------

iteration = 0
while True:
    delta = 0
    V_new = V.copy()
    
    for (i,j,k) in states:
        if is_terminal(i,j,k):
            V_new[(i,j,k)] = 1.0
        else:
            hold = hold_value(i,j,k)
            flip = flip_value(i,j,k)
            V_new[(i,j,k)] = max(hold, flip)
        
        delta = max(delta, abs(V_new[(i,j,k)] - V[(i,j,k)]))
    
    V = V_new
    iteration += 1
    
    if delta < epsilon:
        break

print("Converged after", iteration, "iterations\n")

# -------- EXTRACTION POLICY --------

policy = {}

for (i,j,k) in states:
    if is_terminal(i,j,k):
        policy[(i,j,k)] = "WIN"
    else:
        hold = hold_value(i,j,k)
        flip = flip_value(i,j,k)
        policy[(i,j,k)] = "FLIP" if flip > hold else "HOLD"

# -------- AFFICHAGE COMPLET --------

print("==== VALEURS V(i,j,k) ====\n")

for i in range(GOAL):
    for j in range(GOAL):
        print(f"--- i={i}, j={j} ---")
        for k in range(GOAL+1):
            value = V[(i,j,k)]
            action = policy[(i,j,k)]
            print(f"k={k}  ->  V={value:.6f}   |   Best Action = {action}")
        print()

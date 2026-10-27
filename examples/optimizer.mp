// Compare --phase tac with --phase optimized: seed folds to 14.
// Loop variables must remain dynamic across the back-edge.
int main() {
    int seed;
    int i;
    int total;
    seed = 2 + 3 * 4;
    i = 0;
    total = 0;
    while (i < 5) {
        total = total + i * seed;
        i = i + 1;
    }
    return total + seed;
}

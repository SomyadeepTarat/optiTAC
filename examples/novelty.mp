// Exercise all eight optimizer transformations; returns 100.
int work(int n) {
    int i;
    int total;
    int a;
    int b;
    int copy;
    int dead;
    i = 0;
    total = 0;
    dead = 2 + 3 * 4;
    if (0) total = 999;
    while (i < n) {
        a = i + n;
        b = n + i;
        copy = a;
        dead = copy + 0;
        dead = i * 2;
        total = total + (a - b) + 1;
        i = i + 1;
    }
    return total;
}
int main() {
    return work(100);
    return 1 / 0;
}

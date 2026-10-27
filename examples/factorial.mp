// factorial and power

int fact(int n) {
    if (n <= 1)
        return 1;
    return n * fact(n - 1);
}

int power(int base, int exp) {
    int result;
    result = 1;
    while (exp > 0) {
        result = result * base;
        exp = exp - 1;
    }
    return result;
}

int main() {
    int a;
    int b;
    a = fact(5);
    b = power(2, 10);
    return a + b;
}

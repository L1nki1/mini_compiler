int compiled_fn(int arg) {
    int x = 0;
    int sum = 0;
    do {
        x = x + 1;
        if (x == 3) {
            continue;
        }
        sum = sum + x;
    } while (x < arg);
    return sum;
}

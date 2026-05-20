int compiled_fn(int arg) {
    int sum = 0;
    for (int i = 0; i < arg; i = i + 1) {
        sum = sum + i;
    }
    return sum;
}

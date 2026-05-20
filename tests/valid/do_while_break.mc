int compiled_fn(int arg) {
    int x = 0;
    do {
        x = x + 1;
        if (x == arg) {
            break;
        }
    } while (x < 100);
    return x;
}


int main() {
    int a[5] = {1,2,3,4,5};
    int b[5] = {17,42,23,45,15};
    int c[5];
    for (int i = 0; i < 5; ++i) c[i] = a[i] + b[i];
    return c[0]; // use result so optimizer can't drop loop
}


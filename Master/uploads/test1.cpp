
int main() {
    int m =6;
    int a[6];
    int b[6];
    // int a[6] = {1,2,3,4,5,6};
    // int b[6] = {17,42,23,45,15,67};
    int c[6];
    // for (int i = 0; i < 6; ++i) 
    // {
    //     x[i] = i;
    //     y[i] = i + 1;
    // }
    for (int i = 0; i < 6; ++i) 
    {
    
        c[i] = a[i] + b[i];
    
    }
    return c[0];
}

// int main() {
//     int m = 8;
//     int a[m] = {1,2,3,4,5,6,7,8};
//     int b[m] = {17,42,23,45,15,67,89,9};
//     int c[m];
//     for (int i = 0; i < 8; ++i) 
//     {
    
//         c[i] = a[i] + b[i];
    
//     }
//     return c[0];
// }

// #include <iostream>
// #include <random>

// int main() {
//     const long long N = 100000000; // number of samples
//     long long inside = 0;

//     std::mt19937_64 rng(42);
//     std::uniform_real_distribution<double> dist(0.0, 1.0);

//     for (long long i = 0; i < N; i++) {
//         double x = dist(rng);
//         double y = dist(rng);
//         if (x * x + y * y <= 1.0)
//             inside++;
//     }

//     double pi = 4.0 * inside / N;
//     return pi;
// }

// #include <iostream>
// #include <random>

// int main() {
//     const long long N = 10;

//     int x[10], y[10], hit[10];
//     // std::vector<double> x(10), y(10);
//     // std::vector<int> hit(10);   // 0 or 1 per iteration

//     // -----------------------------
//     // DOALL loop #1: random numbers
//     // -----------------------------
//     std::mt19937_64 rng(1234);
//     std::uniform_real_distribution<double> dist(0.0, 1.0);

//     for (long long i = 0; i < 10; i++) {
//         x[i] = dist(rng);
//         y[i] = dist(rng);
//     }

//     // -----------------------------
//     // DOALL loop #2: Monte Carlo test
//     // -----------------------------
//     for (long long i = 0; i < 10; i++) {
//         double r2 = x[i] * x[i] + y[i] * y[i];
//         hit[i] = (r2 <= 1.0);   // independent write
//     }

    

//     return hit[0];
// }

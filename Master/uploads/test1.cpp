
int main() {
    int a[5] = {1,2,3,4,5};
    int b[5] = {17,42,23,45,15};
    int c[5];
    for (int i = 0; i < 5; ++i) 
    {
    
    c[i] = a[i] + b[i];
    
    }
    return c[0];
}


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
// #include <vector>

// int main() {
//     const long long N = 100000000;

//     std::vector<double> x(N), y(N);
//     std::vector<int> hit(N);   // 0 or 1 per iteration

//     // -----------------------------
//     // DOALL loop #1: random numbers
//     // -----------------------------
//     std::mt19937_64 rng(1234);
//     std::uniform_real_distribution<double> dist(0.0, 1.0);

//     for (long long i = 0; i < N; i++) {
//         x[i] = dist(rng);
//         y[i] = dist(rng);
//     }

//     // -----------------------------
//     // DOALL loop #2: Monte Carlo test
//     // -----------------------------
//     for (long long i = 0; i < N; i++) {
//         double r2 = x[i] * x[i] + y[i] * y[i];
//         hit[i] = (r2 <= 1.0);   // independent write
//     }

    

//     return hit[0];
// }

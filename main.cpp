#include "pricer.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>

void printSection(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

int main() {
    std::cout << std::fixed << std::setprecision(6);

    OptionParams p{100.0, 100.0, 0.05, 0.2, 1.0};

    printSection("Parâmetros");
    std::cout << "S=" << p.S << " K=" << p.K << " r=" << p.r
              << " sigma=" << p.sigma << " T=" << p.T << "\n";

    printSection("Black-Scholes (analítico)");
    double bsCall = blackScholesCall(p);
    double bsPut = blackScholesPut(p);
    std::cout << "Call: " << bsCall << "\n";
    std::cout << "Put:  " << bsPut << "\n";

    printSection("Monte Carlo vs Black-Scholes");
    for (int n : {10000, 1000000}) {
        auto start = std::chrono::high_resolution_clock::now();
        double mcCall = monteCarloCall(p, n);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;

        double erro = std::abs(mcCall - bsCall);
        std::cout << "N=" << n
                   << " | MC Call=" << mcCall
                   << " | BS Call=" << bsCall
                   << " | erro=" << erro
                   << " | tempo=" << elapsed.count() << "ms\n";
    }

    printSection("Sensibilidade (sigma e T aumentando o preço)");
    OptionParams pHighSigma = p;
    pHighSigma.sigma = 0.4;
    OptionParams pHighT = p;
    pHighT.T = 2.0;
    std::cout << "sigma=0.2 -> Call=" << bsCall << "\n";
    std::cout << "sigma=0.4 -> Call=" << blackScholesCall(pHighSigma) << "\n";
    std::cout << "T=1.0 -> Call=" << bsCall << "\n";
    std::cout << "T=2.0 -> Call=" << blackScholesCall(pHighT) << "\n";

    printSection("Gregas (diferenças finitas, sobre a call)");
    std::cout << "Delta: " << deltaCall(p) << "\n";
    std::cout << "Gamma: " << gammaCall(p) << "\n";
    std::cout << "Vega:  " << vegaCall(p) << "\n";

    return 0;
}

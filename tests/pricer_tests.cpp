#include "pricer.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct TestCase {
    std::string name;
    std::function<void()> run;
};

void expectNear(double actual, double expected, double tolerance) {
    if (std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(
            "esperado " + std::to_string(expected) +
            ", obtido " + std::to_string(actual));
    }
}

template <typename Function>
void expectInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    throw std::runtime_error("std::invalid_argument não foi lançada");
}

double normalPDF(double x) {
    constexpr double pi = 3.14159265358979323846;
    return std::exp(-0.5 * x * x) / std::sqrt(2.0 * pi);
}

}  // namespace

int main() {
    const OptionParams base{100.0, 100.0, 0.05, 0.2, 1.0};

    const std::vector<TestCase> tests{
        {"Black-Scholes reproduz valores de referência", [=] {
             expectNear(blackScholesCall(base), 10.450583572185565, 1e-10);
             expectNear(blackScholesPut(base), 5.573526022256971, 1e-10);
         }},
        {"call e put respeitam a paridade", [] {
             const std::vector<OptionParams> cases{
                 {100.0, 100.0, 0.05, 0.2, 1.0},
                 {80.0, 95.0, 0.03, 0.35, 0.5},
                 {140.0, 110.0, -0.01, 0.15, 2.0},
             };

             for (const OptionParams& p : cases) {
                 const double left = blackScholesCall(p) - blackScholesPut(p);
                 const double right = p.S - p.K * std::exp(-p.r * p.T);
                 expectNear(left, right, 1e-10);
             }
         }},
        {"gregas numéricas ficam próximas das fórmulas analíticas", [=] {
             const double sqrtT = std::sqrt(base.T);
             const double d1 =
                 (std::log(base.S / base.K) +
                  (base.r + 0.5 * base.sigma * base.sigma) * base.T) /
                 (base.sigma * sqrtT);

             const double expectedDelta = normCDF(d1);
             const double expectedGamma = normalPDF(d1) / (base.S * base.sigma * sqrtT);
             const double expectedVega = base.S * normalPDF(d1) * sqrtT;

             expectNear(deltaCall(base), expectedDelta, 1e-7);
             expectNear(gammaCall(base), expectedGamma, 1e-6);
             expectNear(vegaCall(base), expectedVega, 1e-5);
         }},
        {"Monte Carlo é reproduzível com seed fixa", [=] {
             constexpr int simulations = 250000;
             constexpr std::uint32_t seed = 20260826;
             const double first = monteCarloCall(base, simulations, seed);
             const double second = monteCarloCall(base, simulations, seed);

             expectNear(first, second, 0.0);
             expectNear(first, blackScholesCall(base), 0.15);
         }},
        {"curva de convergência termina perto de Black-Scholes", [=] {
             const std::vector<double> prices =
                 monteCarloConvergence(base, 250000, 50, 20260826);

             if (prices.size() != 50) {
                 throw std::runtime_error("quantidade inesperada de checkpoints");
             }
             expectNear(prices.back(), blackScholesCall(base), 0.15);
         }},
        {"parâmetros inválidos são rejeitados", [=] {
             OptionParams invalid = base;
             invalid.S = 0.0;
             expectInvalidArgument([&] { blackScholesCall(invalid); });

             invalid = base;
             invalid.sigma = std::numeric_limits<double>::quiet_NaN();
             expectInvalidArgument([&] { blackScholesPut(invalid); });

             expectInvalidArgument([&] { monteCarloCall(base, 0, 42); });
             expectInvalidArgument([&] { monteCarloConvergence(base, 10, 11, 42); });
         }},
    };

    int failures = 0;
    for (const TestCase& test : tests) {
        try {
            test.run();
            std::cout << "[ok] " << test.name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[falhou] " << test.name << ": " << error.what() << '\n';
        }
    }

    std::cout << '\n' << tests.size() - failures << " de " << tests.size()
              << " testes passaram\n";
    return failures == 0 ? 0 : 1;
}

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

struct OptionParams {
    double S;      // preço do ativo
    double K;      // strike
    double r;      // taxa livre de risco
    double sigma;  // volatilidade
    double T;      // tempo até vencimento (anos)
};

inline void validateOptionParams(const OptionParams& p) {
    if (!std::isfinite(p.S) || !std::isfinite(p.K) || !std::isfinite(p.r) ||
        !std::isfinite(p.sigma) || !std::isfinite(p.T)) {
        throw std::invalid_argument("os parâmetros da opção devem ser finitos");
    }
    if (p.S <= 0.0) throw std::invalid_argument("S deve ser positivo");
    if (p.K <= 0.0) throw std::invalid_argument("K deve ser positivo");
    if (p.sigma <= 0.0) throw std::invalid_argument("sigma deve ser positivo");
    if (p.T <= 0.0) throw std::invalid_argument("T deve ser positivo");
}

// ---------- Black-Scholes ----------

inline double normCDF(double x) {
    return 0.5 * (1.0 + erf(x / sqrt(2.0)));
}

inline double blackScholesCall(const OptionParams& p) {
    validateOptionParams(p);
    double d1 = (log(p.S / p.K) + (p.r + p.sigma * p.sigma / 2.0) * p.T) / (p.sigma * sqrt(p.T));
    double d2 = d1 - p.sigma * sqrt(p.T);
    return p.S * normCDF(d1) - p.K * exp(-p.r * p.T) * normCDF(d2);
}

inline double blackScholesPut(const OptionParams& p) {
    validateOptionParams(p);
    double d1 = (log(p.S / p.K) + (p.r + p.sigma * p.sigma / 2.0) * p.T) / (p.sigma * sqrt(p.T));
    double d2 = d1 - p.sigma * sqrt(p.T);
    return p.K * exp(-p.r * p.T) * normCDF(-d2) - p.S * normCDF(-d1);
}

// ---------- Monte Carlo ----------

inline double monteCarloCall(const OptionParams& p, int numSimulations,
                             std::optional<std::uint32_t> seed = std::nullopt) {
    validateOptionParams(p);
    if (numSimulations <= 0) {
        throw std::invalid_argument("o número de simulações deve ser positivo");
    }

    std::mt19937 gen(seed ? *seed : std::random_device{}());
    std::normal_distribution<double> normal(0.0, 1.0);

    double payoffSum = 0.0;
    for (int i = 0; i < numSimulations; ++i) {
        double z = normal(gen);
        double ST = p.S * exp((p.r - p.sigma * p.sigma / 2.0) * p.T + p.sigma * sqrt(p.T) * z);
        payoffSum += std::max(ST - p.K, 0.0);
    }
    double meanPayoff = payoffSum / numSimulations;
    return exp(-p.r * p.T) * meanPayoff;
}

inline double monteCarloPut(const OptionParams& p, int numSimulations,
                            std::optional<std::uint32_t> seed = std::nullopt) {
    validateOptionParams(p);
    if (numSimulations <= 0) {
        throw std::invalid_argument("o número de simulações deve ser positivo");
    }

    std::mt19937 gen(seed ? *seed : std::random_device{}());
    std::normal_distribution<double> normal(0.0, 1.0);

    double payoffSum = 0.0;
    for (int i = 0; i < numSimulations; ++i) {
        double z = normal(gen);
        double ST = p.S * exp((p.r - p.sigma * p.sigma / 2.0) * p.T + p.sigma * sqrt(p.T) * z);
        payoffSum += std::max(p.K - ST, 0.0);
    }
    double meanPayoff = payoffSum / numSimulations;
    return exp(-p.r * p.T) * meanPayoff;
}

// ---------- Gregas por diferenças finitas (sobre a call) ----------

inline double deltaCall(const OptionParams& p, double h = 1e-4) {
    validateOptionParams(p);
    if (h <= 0.0 || p.S <= h) throw std::invalid_argument("h deve estar entre zero e S");
    OptionParams up = p, down = p;
    up.S += h;
    down.S -= h;
    return (blackScholesCall(up) - blackScholesCall(down)) / (2.0 * h);
}

inline double gammaCall(const OptionParams& p, double h = 1e-4) {
    validateOptionParams(p);
    if (h <= 0.0 || p.S <= h) throw std::invalid_argument("h deve estar entre zero e S");
    OptionParams up = p, down = p;
    up.S += h;
    down.S -= h;
    return (blackScholesCall(up) - 2.0 * blackScholesCall(p) + blackScholesCall(down)) / (h * h);
}

inline double vegaCall(const OptionParams& p, double h = 1e-4) {
    validateOptionParams(p);
    if (h <= 0.0 || p.sigma <= h) throw std::invalid_argument("h deve estar entre zero e sigma");
    OptionParams up = p, down = p;
    up.sigma += h;
    down.sigma -= h;
    return (blackScholesCall(up) - blackScholesCall(down)) / (2.0 * h);
}

// ---------- Convergência do Monte Carlo ----------
// Roda UM único stream de simulações até maxN e devolve a média acumulada
// (preço estimado) em `numCheckpoints` pontos espaçados ao longo do caminho.
// É isso que a GUI plota pra mostrar o preço "assentando" perto do valor de
// Black-Scholes conforme mais simulações entram na média.

inline std::vector<double> monteCarloConvergence(
    const OptionParams& p, int maxN, int numCheckpoints,
    std::optional<std::uint32_t> seed = std::nullopt) {
    validateOptionParams(p);
    if (maxN <= 0) throw std::invalid_argument("maxN deve ser positivo");
    if (numCheckpoints <= 0 || numCheckpoints > maxN) {
        throw std::invalid_argument("numCheckpoints deve estar entre 1 e maxN");
    }

    std::mt19937 gen(seed ? *seed : std::random_device{}());
    std::normal_distribution<double> normal(0.0, 1.0);

    std::vector<double> checkpointPrices;
    checkpointPrices.reserve(numCheckpoints);

    double payoffSum = 0.0;
    int nextCheckpoint = 0;
    for (int i = 1; i <= maxN; ++i) {
        double z = normal(gen);
        double ST = p.S * exp((p.r - p.sigma * p.sigma / 2.0) * p.T + p.sigma * sqrt(p.T) * z);
        payoffSum += std::max(ST - p.K, 0.0);

        long long targetI = static_cast<long long>(nextCheckpoint + 1) * maxN / numCheckpoints;
        if (i >= targetI && nextCheckpoint < numCheckpoints) {
            double price = exp(-p.r * p.T) * (payoffSum / i);
            checkpointPrices.push_back(price);
            ++nextCheckpoint;
        }
    }
    return checkpointPrices;
}

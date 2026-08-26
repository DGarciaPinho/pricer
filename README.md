# Pricer de Opções em C++

Fiz esse projeto pra praticar C++ aplicado a finanças quantitativas: um precificador de
opções europeias (call e put) que calcula o preço de duas formas diferentes — pela fórmula
fechada de Black-Scholes e por simulação de Monte Carlo — e compara os dois resultados.

A ideia central é simples: se as duas abordagens estão certas, o preço via Monte Carlo tem
que convergir pro preço de Black-Scholes conforme eu aumento o número de simulações. Isso é
justamente o que o programa mostra ao rodar.

## O que o modelo faz

1. Define os parâmetros da opção numa struct (`OptionParams`): preço do ativo (`S`), strike
   (`K`), taxa livre de risco (`r`), volatilidade (`sigma`) e tempo até o vencimento em anos
   (`T`).
2. Calcula o preço analítico via **Black-Scholes** (`blackScholesCall` / `blackScholesPut`),
   usando a CDF normal implementada em cima de `erf` do `<cmath>`.
3. Calcula o preço via **Monte Carlo** (`monteCarloCall` / `monteCarloPut`): simula milhares
   (ou milhões) de trajetórias do preço final do ativo sob o modelo log-normal do
   Black-Scholes, calcula o payoff de cada uma (`max(S_T - K, 0)` pra call), tira a média e
   desconta a valor presente.
4. Roda o Monte Carlo com N=10.000 e depois N=1.000.000 e mostra o erro em relação ao
   Black-Scholes caindo à medida que N cresce — é a prova de que a simulação converge.
5. Mede o tempo de execução de cada rodada de Monte Carlo com `<chrono>`.
6. Mostra que aumentar `sigma` ou `T` aumenta o preço da call, que é o comportamento
   esperado (mais volatilidade ou mais tempo = mais valor de opcionalidade).
7. Como extensão, calculo as **Gregas** (Delta, Gamma, Vega) da call por diferenças finitas
   em cima da própria fórmula de Black-Scholes, perturbando `S` e `sigma` em um `h` pequeno.

## Estrutura

Toda a lógica de precificação mora em `pricer.h` (header-only), pra poder ser usada tanto
pelo CLI quanto pela GUI sem duplicar código:

- Struct `OptionParams`
- Black-Scholes (`normCDF`, `blackScholesCall`, `blackScholesPut`)
- Monte Carlo (`monteCarloCall`, `monteCarloPut`)
- Gregas por diferenças finitas (`deltaCall`, `gammaCall`, `vegaCall`)
- `monteCarloConvergence`, que roda uma simulação e devolve a média acumulada em vários
  checkpoints — é o que a GUI usa pra plotar a convergência

Em cima disso, tem duas "cascas" diferentes:

- **`main.cpp`** — a versão CLI original, que roda tudo com os parâmetros fixos (S=100,
  K=100, r=0.05, sigma=0.2, T=1) e imprime os resultados no terminal.
- **`gui_main.cpp`** — a GUI nativa (Dear ImGui + GLFW + OpenGL3), onde eu mexo nos
  parâmetros com sliders e vejo os resultados atualizando na hora.

E a pasta `vendor/imgui/` tem uma cópia vendorizada do [Dear ImGui](https://github.com/ocornut/imgui)
(tag v1.91.0), incluindo os backends de GLFW e OpenGL3 que a GUI usa.

## Como compilar e rodar

### CLI (sem dependências externas)

```bash
g++ -std=c++17 -O2 main.cpp -o pricer
./pricer
```

### GUI (precisa de GLFW + OpenGL)

No Arch:

```bash
sudo pacman -S glfw cmake
```

Depois, com CMake (compila o CLI e a GUI juntos):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

./build/pricer       # CLI
./build/pricer_gui   # GUI
```

(O `-O2` / `Release` não são obrigatórios, mas ajudam bastante na velocidade do Monte Carlo
com N grande.)

## Exemplo de saída

Rodando com os parâmetros padrão do `main()` (S=100, K=100, r=0.05, sigma=0.2, T=1 — os
mesmos que eu usei pra validar contra uma calculadora de Black-Scholes online):

```
=== Parâmetros ===
S=100.000000 K=100.000000 r=0.050000 sigma=0.200000 T=1.000000

=== Black-Scholes (analítico) ===
Call: 10.450584
Put:  5.573526

=== Monte Carlo vs Black-Scholes ===
N=10000 | MC Call=10.536057 | BS Call=10.450584 | erro=0.085474 | tempo=0.700438ms
N=1000000 | MC Call=10.464519 | BS Call=10.450584 | erro=0.013936 | tempo=122.053594ms

=== Sensibilidade (sigma e T aumentando o preço) ===
sigma=0.2 -> Call=10.450584
sigma=0.4 -> Call=18.022951
T=1.0 -> Call=10.450584
T=2.0 -> Call=16.126780

=== Gregas (diferenças finitas, sobre a call) ===
Delta: 0.636831
Gamma: 0.018762
Vega:  37.524034
```

Repara que o erro do Monte Carlo cai de ~0.085 (N=10 mil) pra ~0.014 (N=1 milhão) — é
exatamente essa convergência que eu queria confirmar. Como o Monte Carlo usa números
aleatórios (`std::random_device` como seed), os valores exatos mudam um pouco a cada
execução, mas a tendência de convergência se mantém.

## A GUI

A GUI (`pricer_gui`) é uma janela nativa (não é web, não abre navegador) com:

- **Sliders** pra `S`, `K`, `r`, `sigma`, `T` e pra `N` (número de simulações do Monte
  Carlo — esse em escala logarítmica, de 1.000 até 2.000.000, porque faz mais sentido
  explorar essa faixa em ordens de grandeza).
- Um botão **"Calcular"** que roda Black-Scholes e Monte Carlo com os parâmetros atuais e
  mostra os dois preços (call e put) lado a lado numa tabela, junto com o erro absoluto do
  Monte Carlo em relação ao Black-Scholes e o tempo de execução da simulação.
- As três **Gregas** (Delta, Gamma, Vega) recalculadas a cada clique.
- Um **gráfico de convergência**: em vez de só rodar o Monte Carlo uma vez, a GUI roda uma
  simulação de N passos e registra a média acumulada em 60 pontos ao longo do caminho, então
  plota essa curva. Dá pra ver visualmente o preço "assentando" perto da linha de
  Black-Scholes conforme mais simulações entram na conta — é a mesma ideia de convergência
  do CLI, só que visual e em uma única rodada em vez de comparar N=10k contra N=1M.

Não tem nada assíncrono: ao clicar em "Calcular" com N muito grande (perto de 2 milhões), a
janela trava por um instante enquanto a simulação roda na thread principal. Pra esse MVP tá
ok, mas é o primeiro candidato a melhoria se eu quiser deixar a GUI mais responsiva (rodar o
Monte Carlo numa thread separada).

## O que eu ainda quero fazer

Do documento original (`../projeto-cpp-opcoes-quant.md`), eu escolhi implementar Gregas como
extensão, e depois adicionei a GUI por conta própria. Outras ideias que ficaram de fora,
pra eu voltar depois:

- Ler um CSV de preços históricos e calcular volatilidade histórica a partir dele
- Paralelizar o Monte Carlo com `std::thread` ou OpenMP — o loop de simulações é
  embaraçosamente paralelo, então deve dar um ganho bom de performance em N grande, e
  resolveria de quebra o travamento da GUI com N muito alto
- Precificar opção americana via árvore binomial
- Rodar o Monte Carlo da GUI numa thread separada da UI, pra não travar a janela em N grande

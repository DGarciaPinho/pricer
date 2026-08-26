// GUI do pricer, feita com Dear ImGui + GLFW + OpenGL3.
// Reusa exatamente a mesma lógica de precificação do main.cpp (pricer.h) —
// a GUI é só uma casca visual em cima do motor de cálculo.

#include "pricer.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <chrono>
#include <cstdio>
#include <vector>

static void glfw_error_callback(int error, const char* description) {
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(900, 700, "Pricer de Opções — Black-Scholes vs Monte Carlo", nullptr, nullptr);
    if (window == nullptr) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Estado da UI: parâmetros da opção e configuração do Monte Carlo.
    OptionParams params{100.0, 100.0, 0.05, 0.2, 1.0};
    int mcN = 100000;

    // Resultados da última vez que o usuário clicou em "Calcular".
    bool hasResult = false;
    double bsCallResult = 0.0, bsPutResult = 0.0;
    double mcCallResult = 0.0, mcPutResult = 0.0;
    double mcElapsedMs = 0.0;
    double deltaResult = 0.0, gammaResult = 0.0, vegaResult = 0.0;
    std::vector<float> convergence;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Pricer de Opções", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

        ImGui::TextWrapped("Precificador de opções europeias: compara Black-Scholes (fórmula fechada) com Monte Carlo (simulação).");
        ImGui::Separator();

        ImGui::SeparatorText("Parâmetros da opção");
        float S = (float)params.S, K = (float)params.K, r = (float)params.r, sigma = (float)params.sigma, T = (float)params.T;
        ImGui::SliderFloat("S — preço do ativo", &S, 1.0f, 500.0f, "%.2f");
        ImGui::SliderFloat("K — strike", &K, 1.0f, 500.0f, "%.2f");
        ImGui::SliderFloat("r — taxa livre de risco", &r, 0.0f, 0.20f, "%.3f");
        ImGui::SliderFloat("sigma — volatilidade", &sigma, 0.01f, 1.0f, "%.3f");
        ImGui::SliderFloat("T — vencimento (anos)", &T, 0.01f, 5.0f, "%.2f");
        params.S = S;
        params.K = K;
        params.r = r;
        params.sigma = sigma;
        params.T = T;

        ImGui::SeparatorText("Monte Carlo");
        ImGui::SliderInt("N — número de simulações", &mcN, 1000, 2000000, "%d", ImGuiSliderFlags_Logarithmic);

        ImGui::Spacing();
        if (ImGui::Button("Calcular", ImVec2(150, 36))) {
            bsCallResult = blackScholesCall(params);
            bsPutResult = blackScholesPut(params);

            auto start = std::chrono::high_resolution_clock::now();
            mcCallResult = monteCarloCall(params, mcN);
            auto end = std::chrono::high_resolution_clock::now();
            mcElapsedMs = std::chrono::duration<double, std::milli>(end - start).count();
            mcPutResult = monteCarloPut(params, mcN);

            deltaResult = deltaCall(params);
            gammaResult = gammaCall(params);
            vegaResult = vegaCall(params);

            int numCheckpoints = 60;
            std::vector<double> conv = monteCarloConvergence(params, mcN, numCheckpoints);
            convergence.assign(conv.begin(), conv.end());

            hasResult = true;
        }

        if (hasResult) {
            ImGui::Spacing();
            ImGui::SeparatorText("Resultados");

            if (ImGui::BeginTable("resultados", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("");
                ImGui::TableSetupColumn("Call");
                ImGui::TableSetupColumn("Put");
                ImGui::TableHeadersRow();

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Black-Scholes");
                ImGui::TableNextColumn();
                ImGui::Text("%.6f", bsCallResult);
                ImGui::TableNextColumn();
                ImGui::Text("%.6f", bsPutResult);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Monte Carlo");
                ImGui::TableNextColumn();
                ImGui::Text("%.6f", mcCallResult);
                ImGui::TableNextColumn();
                ImGui::Text("%.6f", mcPutResult);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("Erro absoluto (call)");
                ImGui::TableNextColumn();
                ImGui::Text("%.6f", std::abs(mcCallResult - bsCallResult));
                ImGui::TableNextColumn();
                ImGui::Text("-");

                ImGui::EndTable();
            }

            ImGui::Text("Tempo do Monte Carlo (call, N=%d): %.3f ms", mcN, mcElapsedMs);

            ImGui::Spacing();
            ImGui::SeparatorText("Gregas (call, diferenças finitas)");
            ImGui::Text("Delta: %.6f    Gamma: %.6f    Vega: %.6f", deltaResult, gammaResult, vegaResult);

            if (!convergence.empty()) {
                ImGui::Spacing();
                ImGui::SeparatorText("Convergência do Monte Carlo");
                ImGui::TextWrapped("Preço estimado (média acumulada) conforme mais simulações entram na conta. A linha deve se aproximar do valor de Black-Scholes (%.4f).", bsCallResult);
                ImGui::PlotLines("##convergencia", convergence.data(), (int)convergence.size(), 0,
                                  nullptr, FLT_MAX, FLT_MAX, ImVec2(0, 150));
            }
        }

        ImGui::End();

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

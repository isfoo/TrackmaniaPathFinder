#define NOMINMAX
#include <windows.h>
#include <ShlObj.h>
#include <atlbase.h>
#include <iomanip>
#include <filesystem>
#include "gbxParser.h"
#include "solutionFinderCommon.h"
#include "assignmentRelaxationSolutionFinder.h"
#include "Lin-KernighanSolutionFinder.h"
#include "utility.h"
#include "common.h"
#include "fileLoadSave.h"
#include "imgui_directX11.h"

namespace fs = std::filesystem;

void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

constexpr float Pi = 3.14159265f;

void drawSolutionGraph(
    std::vector<int16_t> solution, std::vector<int16_t> compare, ImVec2 pos, ImVec2 size, 
    std::vector<float> x, std::vector<float> y, std::vector<float> textX, std::vector<float> textY,
    std::pair<NodeType, NodeType> hoveredConnection = { 0, 0 }
) {
    int N = int(solution.size());
    float minTextSize = 10.f;
    float maxTextSize = 20.f;
    auto minXY = std::min(size.x, size.y);
    float textSize = std::min(maxTextSize, std::max(minTextSize, 2.f * minXY / N));
    float r = (minXY / 2) - textSize * 2;

    std::vector<int> indexes(N);
    std::vector<int> revIndexes(N);
    std::iota(indexes.begin(), indexes.end(), 0);
    if (compare.size() == solution.size()) {
        for (int i = 0; i < N; ++i)
            indexes[i] = compare[i];
    }
    for (int i = 0; i < N; ++i)
        revIndexes[indexes[i]] = i;
    
    auto& drawList = *ImGui::GetWindowDrawList();
    
    auto drawArrow = [&drawList, &x, &y, N, minXY](int i, int j, ImColor color, float thickness) {
        float arrowLength = thickness * std::min(12.f, 0.75f * minXY / N);
        float arrowAngle = 25;
        auto a = ImVec2(x[i], y[i]);
        auto b = ImVec2(x[j], y[j]);
        float theta = std::atan2(b.y - a.y, b.x - a.x);
        float rad = arrowAngle * (Pi / 180);
        float x1 = b.x - arrowLength * std::cos(theta + rad);
        float y1 = b.y - arrowLength * std::sin(theta + rad);
        float x2 = b.x - arrowLength * std::cos(theta - rad);
        float y2 = b.y - arrowLength * std::sin(theta - rad);
        drawList.AddLine(a, b, color, thickness);
        drawList.AddTriangleFilled(b, ImVec2(x1, y1), ImVec2(x2, y2), color);
    };

    for (int i = 0; i < N - 1; ++i) {
        float r, g, b;
        float h = i * (1.0f / N);
        ImGui::ColorConvertHSVtoRGB(h, 1.f, 1.f, r, g, b);
        bool isHoveredConnection = solution[i] == hoveredConnection.first && solution[i + 1] == hoveredConnection.second;
        drawArrow(revIndexes[solution[i]], revIndexes[solution[i + 1]], ImColor(r, g, b), isHoveredConnection ? 3.0f : 1.0f);
    }
    if (textX.size() == N && textY.size() == N) {
        for (int i = 0; i < N; ++i) {
            auto text = (i == 0) ? std::string("S") : (i == N - 1) ? std::string("F") : std::to_string(indexes[i]);
            drawList.AddText(ImGui::GetDefaultFont(), textSize, ImVec2(textX[i], textY[i]), ImColor(255, 255, 255), text.c_str());
        }
    }
}
void drawPath(const std::vector<Position>& positions) {
    auto& drawList = *ImGui::GetWindowDrawList();
    for (int i = 0; i < positions.size() - 1; ++i) {
        float r, g, b;
        float h = i * (1.0f / positions.size());
        ImGui::ColorConvertHSVtoRGB(h, 1.f, 1.f, r, g, b);
        drawList.AddLine(ImVec2(positions[i].x, positions[i].z), ImVec2(positions[i + 1].x, positions[i + 1].z), ImColor(r, g, b), 1.0f);
    }
}
void drawSolutionGraph(std::vector<int16_t> solution, std::vector<int16_t> compare, ImVec2 pos, ImVec2 size, std::pair<NodeType, NodeType> hoveredConnection = { 0, 0 }) {
    compare.insert(compare.begin(), 0);
    solution.insert(solution.begin(), 0);
    int N = int(solution.size());
    float minTextSize = 10.f;
    float maxTextSize = 20.f;
    auto minXY = std::min(size.x, size.y);
    float textSize = std::min(maxTextSize, std::max(minTextSize, 2.f * minXY / N));
    float r = (minXY / 2) - textSize * 2;
    
    std::vector<float> x(N);
    std::vector<float> y(N);
    std::vector<float> textX(N);
    std::vector<float> textY(N);

    for (int i = 0; i < N; ++i) {
        x[i] = r * std::cos(2 * Pi * i / N) + pos.x + size.x / 2.0f;
        y[i] = r * std::sin(2 * Pi * i / N) + pos.y + size.y / 2.0f;
        textX[i] = (r + textSize) * std::cos(2 * Pi * i / N) + pos.x + size.x / 2.0f - textSize / 2;
        textY[i] = (r + textSize) * std::sin(2 * Pi * i / N) + pos.y + size.y / 2.0f - textSize / 2;
    }
    std::rotate(x.begin(), x.begin() + (N * 3) / 4 + 1, x.end());
    std::rotate(y.begin(), y.begin() + (N * 3) / 4 + 1, y.end());
    std::rotate(textX.begin(), textX.begin() + (N * 3) / 4 + 1, textX.end());
    std::rotate(textY.begin(), textY.begin() + (N * 3) / 4 + 1, textY.end());

    drawSolutionGraph(solution, compare, pos, size, x, y, textX, textY, hoveredConnection);
}

void scaleAndTranslatePositionsToFit(std::vector<Position>& positions, ImVec2 pos, ImVec2 size, float margin) {
    // translate to start at 0, 0:
    auto minX = std::min_element(positions.begin(), positions.end(), [](auto& a, auto& b) { return a.x < b.x; })->x;
    auto minY = std::min_element(positions.begin(), positions.end(), [](auto& a, auto& b) { return a.z < b.z; })->z;
    for (auto& position : positions) {
        position.x -= minX;
        position.z -= minY;
    }

    // scale to fit draw box size
    auto maxX = std::max_element(positions.begin(), positions.end(), [](auto& a, auto& b) { return a.x < b.x; })->x;
    auto maxY = std::max_element(positions.begin(), positions.end(), [](auto& a, auto& b) { return a.z < b.z; })->z;
    auto scaleFactor = std::min((size.x - margin * 2) / maxX, (size.y - margin * 2) / maxY);
    for (auto& position : positions) {
        position.x *= scaleFactor;
        position.z *= scaleFactor;
    }

    // translate to pos with centering
    for (auto& position : positions) {
        position.x += pos.x + (size.x - maxX * scaleFactor) / 2.0f;
        position.z += pos.y + (size.y - maxY * scaleFactor) / 2.0f;
    }
}
void drawSolutionGraph(std::vector<Position> positions, std::vector<int16_t> solution, std::vector<int16_t> compare, ImVec2 pos, ImVec2 size, std::pair<NodeType, NodeType> hoveredConnection = { 0, 0 }) {
    compare.insert(compare.begin(), 0);
    solution.insert(solution.begin(), 0);
    int N = int(solution.size());
    float minTextSize = 10.f;
    float maxTextSize = 20.f;
    auto minXY = std::min(size.x, size.y);
    float textSize = std::min(maxTextSize, std::max(minTextSize, 2.f * minXY / N));
    
    std::vector<float> x(N);
    std::vector<float> y(N);
    std::vector<float> textX(N);
    std::vector<float> textY(N);

    scaleAndTranslatePositionsToFit(positions, pos, size, textSize);
    for (int i = 0; i < N; ++i) {
        x[i] = positions[i].x;
        y[i] = positions[i].z;
        textX[i] = positions[i].x;
        textY[i] = positions[i].z - textSize;
    }

    drawSolutionGraph(solution, compare, pos, size, x, y, textX, textY, hoveredConnection);
}

void fileExplorer(char* outFilePath, const char* filter) {
    OPENFILENAMEA ofn;
    CHAR szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = MyImGui::Hwnd();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = filter;
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) {
        std::strcpy(outFilePath, ofn.lpstrFile);
    }
}

ImFont* setGuiStyle() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();

    ImFontConfig fontConfig;
    static const ImWchar glyphRanges[] = { 0x0020, 0x00ff, 0x0394, 0x0394, 0 }; // include delta symbol
    fontConfig.GlyphRanges = glyphRanges;
    ImFont* guiFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16 * 2, &fontConfig);
    io.Fonts->Build();

    ImGuiStyle* style = &ImGui::GetStyle();
    float hspacing = 8;
    float vspacing = 1;
    style->DisplaySafeAreaPadding = ImVec2(0, 0);
    style->WindowPadding = ImVec2(hspacing / 2, vspacing);
    style->FramePadding = ImVec2(hspacing, vspacing);
    style->ItemSpacing = ImVec2(hspacing, vspacing);
    style->ItemInnerSpacing = ImVec2(hspacing, vspacing);
    style->CellPadding = ImVec2(hspacing, vspacing);

    return guiFont;
}

int main(int argc, char** argv) {
    CoInitialize(NULL);
    MyImGui::Init(u"Trackmania Path Finder");

    constexpr int MinFontSize = 8;
    constexpr int MaxFontSize = 30;
    int fontSize = 16;

    InputData input;
    State state;

    std::atomic<bool> stopWorkingForConfig;
    SolutionConfig config(stopWorkingForConfig);
    config.maxSolutionCount = input.maxSolutionCount;
    config.partialSolutionCount = 0;
    config.stopWorking = false;

    auto guiFont = setGuiStyle();

    MyImGui::Run([&] {
        guiFont->Scale = fontSize / 22.0f;
        ImGui::PushFont(guiFont);

        auto io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
        ImGui::Begin("Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);

        if (ImGui::GetIO().KeyCtrl) {
            if (ImGui::GetIO().MouseWheel > 0) {
                fontSize = std::clamp(fontSize + 1, MinFontSize, MaxFontSize);
            }
            if (ImGui::GetIO().MouseWheel < 0) {
                fontSize = std::clamp(fontSize - 1, MinFontSize, MaxFontSize);
            }
        }

        auto tableInputEntry = [](const std::string& label, const std::string& helpText, std::function<void()> inputFunction) {
            ImGui::TableNextColumn();
            ImGui::Text("%s:", label.c_str());
            ImGui::SameLine();
            if (!helpText.empty())
                HelpMarker(helpText.c_str());
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1);
            inputFunction();
        };
        auto tableInputEntryText = [&tableInputEntry](const std::string& label, char(&inputText)[1024], const std::string& helpText) {
            tableInputEntry(label, helpText, [&]() {
                ImGui::InputText(("##" + label).c_str(), inputText, sizeof(inputText));
            });
        };
        auto tableInputEntryInt = [&tableInputEntry](const std::string& label, int& inputInt, int minValue, int maxValue, const std::string& helpText) {
            tableInputEntry(label, helpText, [&]() {
                if (ImGui::InputInt(("##" + label).c_str(), &inputInt)) {
                    inputInt = std::clamp(inputInt, minValue, maxValue);
                }
            });
        };
        auto tableInputEntryIntDisabledIfNoPositionData = [&tableInputEntryInt, &input](const std::string& label, int& inputInt, int minValue, int maxValue, const std::string& helpText) {
            if (input.positionReplayFilePath[0] == '\0') ImGui::BeginDisabled();
            tableInputEntryInt(label, inputInt, minValue, maxValue, helpText);
            if (input.positionReplayFilePath[0] == '\0') ImGui::EndDisabled();
        };
        auto tableInputEntryFile = [&tableInputEntry](const std::string& label, char(&inputText)[1024], const char* filter, const std::string& helpText) {
            tableInputEntry(label, helpText, [&]() {
                ImGui::PushID((label + " button").c_str());
                if (ImGui::Button("Find file")) {
                    fileExplorer(inputText, filter);
                }
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText("##input data file", inputText, sizeof(inputText));
            });
        };

        if (ImGui::BeginTable("menuTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
            ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
            ImGui::TableSetupColumn("Text2", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 2);
            ImGui::TableSetupColumn("Input2", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 3);
            tableInputEntryInt("font size", fontSize, MinFontSize, MaxFontSize, "You can use CTRL + Mouse wheel to change font size");
            if (state.isOnPathFinderTab) {
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                ImGui::Checkbox("show advanced settings", &input.showAdvancedSettings);
            }
            ImGui::EndTable();
        }

        if (ImGui::BeginTabBar("#Tabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Path Finder")) {
                state.isOnPathFinderTab = true;
                if (ImGui::BeginTable("menuTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                    ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                    ImGui::TableSetupColumn("Text2", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 2);
                    ImGui::TableSetupColumn("Input2", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 3);
                    if (input.showAdvancedSettings) {
                        tableInputEntryInt("max connection time", input.ignoredValue, 1, 100'000, "Connections with this or higher time\nwill not be considered in the solutions");
                        tableInputEntryInt("max route time", input.limitValue, 1, 100'000, "");
                    }
                    tableInputEntryInt("max nr of routes", input.maxSolutionCount, 1, 100'000, "Number of fastest routes you want to find.\n\nUnless you are working with a small number of connections you should not set it to an arbitrarily high value - this parameter plays a key role in how long the search process will take so you should set it to something reasonable");
                    tableInputEntryInt("max search time", input.maxTime, 1, 100'000, "Max time in seconds you want to search for.\n\nIt's mostly useful for heuristic algorithm since it will usually find most if not all top 100 solutions in the first ~10 seconds even for hard problems\n\nMight need to increase that time for some problems - you have to experiment yourself.");
                    if (input.showAdvancedSettings) {
                        tableInputEntryInt("max repeat CPs to add", input.maxRepeatNodesToAdd, 0, 100'000, "");
                        tableInputEntryText("turned off repeat CPs", input.turnedOffRepeatNodes, "List of CP numbers you want to ban from repeating");
                        tableInputEntryText("output data file", input.outputDataFile, "After completing running the algorithm this file\nwill have sorted list of top \"max number of routes\" found.");
                        tableInputEntryText("ring CPs", input.ringCps, "List of CP numbers that are rings.\nThat is CPs for which you want to include connection\nwhere you standing respawn after taking this CP\nto go back to previous CP");
                        if (config.weights.size() > 0) {
                            tableInputEntry("calculate route time", "", [&]() {
                                ImGui::PushID("Calculate route button");
                                if (ImGui::Button("Calculate time")) {
                                    std::string routeString = input.routeString;
                                    auto [sol, routeTime] = createSolutionFromString(routeString, config);
                                    state.outputRouteCalcTime = routeTime;
                                }
                                ImGui::PopID();
                                ImGui::SameLine();
                                ImGui::SetNextItemWidth(-1);
                                ImGui::InputText("##input route string", input.routeString, sizeof(input.routeString));
                            });
                            ImGui::TableNextColumn();
                            ImGui::SetNextItemWidth(-1);
                            if (state.outputRouteCalcTime == -1.0) {
                                ImGui::Text("Error");
                            } else {
                                ImGui::Text("Time = %.1f", state.outputRouteCalcTime);
                            }
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1);
                if (ImGui::BeginTable("menuTable2", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                    ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
            
                    if (input.showAdvancedSettings) {
                        tableInputEntryFile("CP positions file", input.positionReplayFilePath, "txt\0*.txt\0All\0*.*\0", "Optional for visualization only.\nFile containing positions of start, finish and all CPs created in \"CP positions creactor\" tab.\n\nWhen provided it enables graph view to show 2D view of the route");
                    }
                    tableInputEntryFile("input data file", input.inputDataFile, "All\0*.*\0CSV\0*.CSV\0", "Format is full matrix of decimal values in CSV format\nusing any delimiter, e.g. comma, space, tab.\n\nFirst row are times to CP1.\nLast row are times to finish.\nFirst column are times from start.\nLast column are times from last CP.");
                    ImGui::EndTable();
                }

                auto sortBestFoundSolutionsSolutions = [&]() {
                    std::sort(state.bestFoundSolutions.begin(), state.bestFoundSolutions.end(), [&](BestSolution& a, auto& b) {
                        if (input.isConnectionSearchAlgorithm) {
                            if (!input.sortConnectionSearchResultsByConnection) {
                                if (a.time < b.time)
                                    return true;
                                if (a.time > b.time)
                                    return false;
                            }
                            if (a.addedConnection.first < b.addedConnection.first)
                                return true;
                            if (a.addedConnection.first > b.addedConnection.first)
                                return false;
                            return a.addedConnection.second < b.addedConnection.second;
                        }
                        if (a.time < b.time)
                            return true;
                        if (a.time > b.time)
                            return false;
                        return a.solution < b.solution;
                    });
                };

                if (input.showAdvancedSettings) {
                    ImGui::Checkbox("Connection finder mode", &input.isConnectionSearchAlgorithm);
                }
                if (input.showAdvancedSettings && input.isConnectionSearchAlgorithm) {
                    if (ImGui::BeginTable("ConnectionFinderTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                        ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                        ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                        ImGui::TableSetupColumn("Text2", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 2);
                        ImGui::TableSetupColumn("Input2", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 3);
                        tableInputEntryInt("min connection time", input.connectionFinderSettings.minConnectionTime, 0, 100'000, "Connections with this or higher time\nwill be tried to be replaced with tested time");
                        tableInputEntryInt("max connection time", input.connectionFinderSettings.maxConnectionTime, 0, 100'000, "Connections with this or lower time\nwill be tried to be replaced with tested time");
                        tableInputEntryIntDisabledIfNoPositionData("min distance", input.connectionFinderSettings.minDistance, 0, 100'000, "Connections with this or higher distance\nwill be tried to be replaced with tested time\n\nNOTE: Requires CP positions file");
                        tableInputEntryIntDisabledIfNoPositionData("max distance", input.connectionFinderSettings.maxDistance, 0, 100'000, "Connections with this or lower distance\nwill be tried to be replaced with tested time\n\nNOTE: Requires CP positions file");
                        tableInputEntryIntDisabledIfNoPositionData("min height difference", input.connectionFinderSettings.minHeightDiff, -100'000, 100'000, "Connections with this or higher Height(To_CP) - Height(From_CP)\nwill be tried to be replaced with tested time\n\nNOTE: Requires CP positions file");
                        tableInputEntryIntDisabledIfNoPositionData("max height difference", input.connectionFinderSettings.maxHeightDiff, -100'000, 100'000, "Connections with this or lower Height(To_CP) - Height(From_CP)\nwill be tried to be replaced with tested time\n\nNOTE: Requires CP positions file");
                        tableInputEntryInt("tested time", input.connectionFinderSettings.testedConnectionTime, 0, 100'000, "Time that will be inserted into tested connections");
                        tableInputEntryText("source CPs", input.searchSourceNodes, "List of source CP numbers that should be considered while searching.\nIf empty - all CPs are considered");

                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::Checkbox("sort by connection", &input.sortConnectionSearchResultsByConnection)) {
                            sortBestFoundSolutionsSolutions();
                        }
                        ImGui::EndTable();
                    }
                }

                if (!state.errorMsg.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 0, 0, 255));
                    ImGui::Text("Error: %s", state.errorMsg.c_str());
                    ImGui::PopStyleColor();
                }

                auto startAlgorithm = [&](Algorithm algorithm) {
                    if (isRunning(state.algorithmRunTask)) {
                        state.errorMsg = "Already running";
                        return;
                    }
                    state.errorMsg = "";
                    state.bestFoundSolutions.clear();
                    state.copiedBestSolutionsAfterAlgorithmDone = false;

                    input.showResultsFilter = false;
                    state.resultRequiredConnections.clear();
                    state.resultOptionalConnections.clear();

                    state.isGraphWindowOpen = false;
                    state.solutionToShowInGraphWindow.clear();
                    state.solutionToCompareToInGraphWindow.clear();

                    runAlgorithm(algorithm, config, input, state);
                };
                bool disableAlgorithmButtons = isRunning(state.algorithmRunTask);
                if (disableAlgorithmButtons)
                    ImGui::BeginDisabled();
                if (ImGui::Button("Run exact algorithm")) {
                    state.isHeuristicAlgorithm = false;
                    startAlgorithm(Algorithm::Assignment);
                }
                if (input.showAdvancedSettings) {
                    ImGui::SameLine();
                    if (ImGui::Button("Run heuristic algorithm")) {
                        state.isHeuristicAlgorithm = true;
                        startAlgorithm(Algorithm::LinKernighan);
                    }
                }
                if (disableAlgorithmButtons)
                    ImGui::EndDisabled();
                if (isRunning(state.algorithmRunTask)) {
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) {
                        state.taskWasCanceled = true;
                    }
                }
                if (state.copiedBestSolutionsAfterAlgorithmDone) {
                    ImGui::SameLine();
                    ImGui::Checkbox("Show results connection filter", &input.showResultsFilter);
                }

                auto getRequiredAndOptionalConnectionSets = [&]() -> std::pair<FastSet2d, FastSet2d> {
                    if (state.bestFoundSolutions.empty())
                        return {};
                    auto allConnectionsSet = state.bestFoundSolutions[0].solutionConnections;
                    auto commonConnectionsSet = state.bestFoundSolutions[0].solutionConnections;
                    for (auto& solution : state.bestFoundSolutions) {
                        allConnectionsSet |= solution.solutionConnections;
                        commonConnectionsSet &= solution.solutionConnections;
                    }
                    auto diffConnectionsSet = allConnectionsSet;
                    diffConnectionsSet -= commonConnectionsSet;

                    return { commonConnectionsSet, diffConnectionsSet };
                };

                if (input.showResultsFilter) {
                    ImVec2 buttonSize(fontSize * 5.5f, 0);
                    ImVec4 colRequired = ImVec4(0.133f, 0.694f, 0.298f, 1.00f);
                    ImVec4 colBanned = ImVec4(0.922f, 0.2f, 0.141f, 1.00f);
                    ImVec4 colAutoBanned = colBanned;
                    ImVec4 colOptional = ImVec4(0.4f, 0.4f, 0.4f, 1.00f);
                    ImGuiStyle& style = ImGui::GetStyle();

                    auto addConnectionButton = [&](const FilterConnection& c, bool alwaysDisabled = false) {
                        if (c.status == FilterConnection::Required) ImGui::PushStyleColor(ImGuiCol_Button, colRequired);
                        if (c.status == FilterConnection::Banned) ImGui::PushStyleColor(ImGuiCol_Button, colBanned);
                        if (c.status == FilterConnection::AutoBanned) ImGui::PushStyleColor(ImGuiCol_Button, colAutoBanned);
                        if (c.status == FilterConnection::Optional) ImGui::PushStyleColor(ImGuiCol_Button, colOptional);
                       
                        if (c.status == FilterConnection::Required || c.status == FilterConnection::AutoBanned)
                            ImGui::BeginDisabled();
                        bool buttonWasPressed = ImGui::Button((std::to_string(c.connection.second) + "->" + std::to_string(c.connection.first)).c_str(), buttonSize);
                        if (c.status == FilterConnection::Required || c.status == FilterConnection::AutoBanned)
                            ImGui::EndDisabled();
                        ImGui::PopStyleColor();
                        return buttonWasPressed;
                    };

                    ImGui::Text("Required connections:");
                    float windowVisibleX = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
                    for (int i = 0; i < state.resultRequiredConnections.size(); ++i) {
                        ImGui::PushID(("Required connection" + std::to_string(i)).c_str());
                        addConnectionButton(state.resultRequiredConnections[i], true);
                        float lastButtonX = ImGui::GetItemRectMax().x;
                        float nextButtonX = lastButtonX + style.ItemSpacing.x + buttonSize.x;
                        if (i + 1 < state.resultRequiredConnections.size() && nextButtonX < windowVisibleX)
                            ImGui::SameLine();
                        ImGui::PopID();
                    }

                    ImGui::Text("Optional connections:");
                    for (int i = 0; i < state.resultOptionalConnections.size(); ++i) {
                        ImGui::PushID(("Optional connection" + std::to_string(i)).c_str());
                        if (addConnectionButton(state.resultOptionalConnections[i])) {
                            if (state.resultOptionalConnections[i].status == FilterConnection::Optional)
                                state.resultOptionalConnections[i].status = FilterConnection::Banned;
                            else if (state.resultOptionalConnections[i].status == FilterConnection::Banned)
                                state.resultOptionalConnections[i].status = FilterConnection::Optional;

                            state.bestFoundSolutions.clear();
                            FastSet2d bannedConnections(config.weights.size());
                            for (auto& c : state.resultOptionalConnections) {
                                if (c.status == FilterConnection::Banned) {
                                    bannedConnections.set(c.connection.first, c.connection.second);
                                }
                            }
                            for (auto& s : config.bestSolutions) {
                                auto set = s.solutionConnections;
                                set &= bannedConnections;
                                if (!set.any()) {
                                    state.bestFoundSolutions.push_back(s);
                                }
                            }
                            auto [commonConnectionsSet, optionalConnectionsSet] = getRequiredAndOptionalConnectionSets();

                            for (auto& c : state.resultOptionalConnections) {
                                if (bannedConnections.test(c.connection.first, c.connection.second)) {
                                    c.status = FilterConnection::Banned;
                                } else if (commonConnectionsSet.test(c.connection.first, c.connection.second)) {
                                    c.status = FilterConnection::Required;
                                } else if (optionalConnectionsSet.test(c.connection.first, c.connection.second)){
                                    c.status = FilterConnection::Optional;
                                } else {
                                    c.status = FilterConnection::AutoBanned;
                                }
                            }
                        }
                        float lastButtonX = ImGui::GetItemRectMax().x;
                        float nextButtonX = lastButtonX + style.ItemSpacing.x + buttonSize.x;
                        if (i + 1 < state.resultOptionalConnections.size() && nextButtonX < windowVisibleX)
                            ImGui::SameLine();
                        ImGui::PopID();
                    }
                }

                auto addNumberPadding = [](int value, int maxValue) {
                    int paddingCount = int(std::to_string(maxValue).size() - std::to_string(value).size());
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
                    ImGui::Text("%s", std::string(paddingCount, '0').c_str());
                    ImGui::PopStyleColor();
                    ImGui::SameLine();
                };

                if (ImGui::BeginTable("statusTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable)) {
                    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize,1.0f, 0);
                    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 0.5f, 1);
                    ImGui::TableSetupColumn("Candidates found", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 2);
                    ImGui::TableSetupColumn("Search progress", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 2.0f, 3);

                    ImGui::TableHeadersRow();
                    ImGui::TableNextColumn();
                    std::string status;
                    if (!state.algorithmRunTask.valid()) {
                        status = "Waiting for start";
                    } else if (state.endedWithTimeout) {
                        if (isRunning(state.algorithmRunTask))
                            status = "Timeout (Still running)";
                        else
                            status = "Timeout";
                    } else if (state.taskWasCanceled) {
                        if (isRunning(state.algorithmRunTask))
                            status = "Canceled (Still running)";
                        else
                            status = "Canceled";
                    } else if (isRunning(state.algorithmRunTask)) {
                        status = "Running";
                    } else {
                        status = "Done";
                    }
                    ImGui::Text("%s", status.c_str());

                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f [s]", state.timer.getTime());

                    ImGui::TableNextColumn();
                    ImGui::Text("%d", config.solutionsVec.size());

                    ImGui::TableNextColumn();
                    if (input.isConnectionSearchAlgorithm) {
                        ImGui::Text("Checked %d / %d connections", config.partialSolutionCount.load(), state.connectionsToTest.size());
                    } else if (state.isHeuristicAlgorithm) {
                        auto n = config.partialSolutionCount.load();
                        auto optVal = n >> 32;
                        auto tryVal = n & 0xffffffff;
                        ImGui::Text("Completed "); 
                        ImGui::SameLine();
                        addNumberPadding(int(tryVal), 1000);
                        ImGui::Text("%d tries for %d-opt", tryVal, optVal);
                    } else {
                        ImGui::Text("Partial routes processed: %d", config.partialSolutionCount.load());
                    }

                    ImGui::EndTable();
                }

                if (config.stopWorking && !state.copiedBestSolutionsAfterAlgorithmDone) {
                    state.copiedBestSolutionsAfterAlgorithmDone = true;
                    state.bestFoundSolutions = config.bestSolutions;
                    sortBestFoundSolutionsSolutions();
                    auto [commonConnectionsSet, optionalConnectionsSet] = getRequiredAndOptionalConnectionSets();
                    for (auto& connection : commonConnectionsSet.toSortedList()) {
                        state.resultRequiredConnections.emplace_back(connection, FilterConnection::Required);
                    }
                    for (auto& connection : optionalConnectionsSet.toSortedList()) {
                        state.resultOptionalConnections.emplace_back(connection, FilterConnection::Optional);
                    }
                } else if (!state.copiedBestSolutionsAfterAlgorithmDone && config.solutionsVec.size() > state.bestFoundSolutions.size()) {
                    for (int i = int(state.bestFoundSolutions.size()); i < config.solutionsVec.size(); ++i) {
                        state.bestFoundSolutions.push_back(config.solutionsVec[i]);
                    }
                    sortBestFoundSolutionsSolutions();
                }
                auto bestSolutionCount = std::min<int>(int(state.bestFoundSolutions.size()), config.maxSolutionCount);
                auto maxSolutionTime = state.bestFoundSolutions.empty() ? 0 : std::max_element(state.bestFoundSolutions.begin(), state.bestFoundSolutions.begin() + bestSolutionCount, [](auto& a, auto& b) { return a.time < b.time; })->time;
                auto textDigitWidth = ImGui::CalcTextSize("0").x;

                auto updateSolutionId = [&](int& id, std::vector<int16_t>& solution) {
                    if (id < 0) {
                        solution.clear();
                        return;
                    }
                    if (solution.empty()) {
                        id = -1;
                        return;
                    }
                    for (int i = id; i < bestSolutionCount; ++i) {
                        if (state.bestFoundSolutions[i].solution == solution) {
                            id = i;
                            return;
                        }
                    }
                    id = -1;
                    solution.clear();
                };
                updateSolutionId(state.solutionToShowId, state.solutionToShowInGraphWindow);
                updateSolutionId(state.solutionToCompareId, state.solutionToCompareToInGraphWindow);
                
                if (ImGui::BeginTable("solutionsTable", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, textDigitWidth * (std::to_string(config.maxSolutionCount).size() + 1), 0);
                    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, textDigitWidth * (std::max(4, int(std::to_string(maxSolutionTime).size())) + 1), 1);
                    ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 3.5f, 2);
                    if (input.isConnectionSearchAlgorithm) {
                        ImGui::TableSetupColumn("Connection", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 5.5f, 3);
                    } else {
                        ImGui::TableSetupColumn("Variations", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 5.5f, 3);
                    }
                    ImGui::TableSetupColumn("Route", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 4);
                    ImGui::TableHeadersRow();

                    ImGuiListClipper clipper;
                    clipper.Begin(bestSolutionCount);
                    while (clipper.Step()) {
                        for (int j = clipper.DisplayStart; j < clipper.DisplayEnd; ++j) {
                            auto& B_ = state.bestFoundSolutions[j].solution;
                            auto time = state.bestFoundSolutions[j].time;
                            auto solStr = state.bestFoundSolutions[j].solutionString;
                    
                            ImGui::TableNextColumn();
                            addNumberPadding(j + 1, config.maxSolutionCount);
                            ImGui::Text("%d", j + 1);

                            ImGui::TableNextColumn();
                            addNumberPadding(time, maxSolutionTime);
                            ImGui::Text("%.1f", time / 10.0);

                            ImGui::TableNextColumn();
                            ImGui::PushID((std::to_string(j) + "_solution_button").c_str());
                            if (ImGui::Button("Graph")) {
                                state.isGraphWindowOpen = true;
                                state.solutionToShowId = j;
                                state.solutionToShowInGraphWindow = B_;
                            }
                            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                                if (state.solutionToCompareToInGraphWindow == B_) {
                                    state.solutionToCompareId = -1;
                                    state.solutionToCompareToInGraphWindow.clear();
                                } else {
                                    state.solutionToCompareId = j;
                                    state.solutionToCompareToInGraphWindow = B_;
                                }
                            }
                            ImGui::PopID();
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetNextWindowSize(ImVec2(350, 375));
                                ImGui::BeginTooltip();
                                auto pos = ImGui::GetWindowPos();
                                drawSolutionGraph(B_, state.solutionToCompareToInGraphWindow, pos, ImVec2(350, 350));
                                auto& drawList = *ImGui::GetWindowDrawList();
                                std::string text = (state.solutionToCompareToInGraphWindow == B_) ? "Right-click to disable compare" : "Right-click to compare against";
                                drawList.AddText(ImGui::GetDefaultFont(), 20, ImVec2(pos.x + 10, pos.y + 350), ImColor(255, 255, 255), text.c_str());
                                ImGui::EndTooltip();
                            }
                            ImGui::TableNextColumn();
                            if (state.bestFoundSolutions[j].addedConnection != NullEdge) {
                                auto& c = state.bestFoundSolutions[j].addedConnection;
                                ImGui::Text((std::to_string(c.first) + "->" + std::to_string(c.second)).c_str());
                            } else {
                                ImGui::PushID((std::to_string(j) + "_variations_button").c_str());
                                std::string variationButtonLabel = "Var(";
                                if (state.copiedBestSolutionsAfterAlgorithmDone) {
                                    variationButtonLabel += std::to_string(state.bestFoundSolutions[j].allVariations.size());
                                } else {
                                    variationButtonLabel += "???";
                                }
                                variationButtonLabel += ")";
                                if (!state.copiedBestSolutionsAfterAlgorithmDone || state.bestFoundSolutions[j].allVariations.size() == 1) {
                                    ImGui::BeginDisabled();
                                }
                                ImGui::SetNextItemWidth(-1);
                                if (ImGui::Button(variationButtonLabel.c_str(), ImVec2(-1, 0))) {
                                    state.isVariationsWindowOpen = true;
                                    state.solutionToShowVariationId = j;
                                    state.solutionToShowVariation = state.bestFoundSolutions[j];
                                }
                                if (!state.copiedBestSolutionsAfterAlgorithmDone || state.bestFoundSolutions[j].allVariations.size() == 1) {
                                    ImGui::EndDisabled();
                                }
                                ImGui::PopID();
                            }
                            ImGui::TableNextColumn();
                            ImGui::SetNextItemWidth(-1);
                            ImGui::InputText(("##solution" + std::to_string(j)).c_str(), solStr.data(), solStr.size(), ImGuiInputTextFlags_ReadOnly);
                        }
                    }
                    clipper.End();
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            } else {
                state.isOnPathFinderTab = false;
            }
            if (ImGui::BeginTabItem("CP positions creator (experimental)")) {
                static std::string cpPositionsErrorMsg;

                if (ImGui::BeginTable("menuTableCp", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                    ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                    tableInputEntryFile("Full Replay Gbx", input.positionReplayFile, "Gbx\0*.Gbx\0All\0*.*\0", "Can be either Replay or Ghost file in Gbx format.\nIt's assumed that it is a full replay from start to finish");
                    tableInputEntryText("CP order (optional)", input.cpOrder, "Comma separated full list of CP numbers in order they were driven.\nIt's useful if you have no replay at hand that drives all CPs in the correct order.");
                    tableInputEntryText("output positions file", input.outputPositionsFile, "Text file that will contain positions of start, CPs and finish.\nThis is used for automatic generation of spreadsheet values and visualizations.");
                    ImGui::EndTable();
                }
                if (ImGui::Button("Create positions file")) {
                    cpPositionsErrorMsg.clear();
                    state.calculatedCpPositions.clear();
                    auto replayData = getReplayData(std::wstring(input.positionReplayFile, input.positionReplayFile + strlen(input.positionReplayFile)), cpPositionsErrorMsg);
                    if (cpPositionsErrorMsg.empty()) {
                        state.calculatedCpPositions = getCpPositions(replayData);
                        auto cpOrder = parseIntList(input.cpOrder, 1, state.calculatedCpPositions.size() - 2, "CP order list", cpPositionsErrorMsg);
                        if (state.calculatedCpPositions.empty()) {
                            cpPositionsErrorMsg = "Failed to read positions";
                        } else if (state.calculatedCpPositions.size() == 1) {
                            cpPositionsErrorMsg = "Failed to find CP positions in replay file";
                        } else if (!cpOrder.empty()) {
                            std::vector<int> path(state.calculatedCpPositions.size() - 2);
                            std::iota(path.begin(), path.end(), 1);
                            auto sortedCpOrder = cpOrder;
                            std::sort(sortedCpOrder.begin(), sortedCpOrder.end());
                            if (sortedCpOrder != path) {
                                state.calculatedCpPositions.clear();
                                if (path.empty()) {
                                    cpPositionsErrorMsg = "There are only 2 positions (assumed start and finish) in replay, so cannot apply CP order list";
                                } else if (sortedCpOrder.size() > path.size()) {
                                    for (auto cp : sortedCpOrder) {
                                        if (cp < path[0] || cp > path.back()) {
                                            cpPositionsErrorMsg = "CP " + std::to_string(cp) + " provided in list is outside of expected [" + std::to_string(path[0]) + ", " + std::to_string(path.back()) + "] range";
                                            break;
                                        }
                                    }
                                    for (int i = 1; i < sortedCpOrder.size(); ++i) {
                                        if (sortedCpOrder[i - 1] == sortedCpOrder[i]) {
                                            cpPositionsErrorMsg = "Duplicate CP " + std::to_string(sortedCpOrder[i]) + " in CP order list";
                                            break;
                                        }
                                    }
                                } else {
                                    for (auto cp : path) {
                                        if (std::find(sortedCpOrder.begin(), sortedCpOrder.end(), cp) == sortedCpOrder.end()) {
                                            cpPositionsErrorMsg = "Missing CP " + std::to_string(cp) + " in CP order list";
                                            break;
                                        }
                                    }
                                }
                                if (cpPositionsErrorMsg.empty()) { // should never happen
                                    cpPositionsErrorMsg = "Unexpected problem with CP order list";
                                }
                            } else {
                                state.calculatedCpOrder = cpOrder;
                                state.calculatedCpOrder.insert(state.calculatedCpOrder.begin(), 0);
                                state.calculatedCpOrder.push_back(state.calculatedCpOrder.size());
                                std::vector<Position> newPositions(state.calculatedCpPositions.size());
                                for (int i = 0; i < state.calculatedCpOrder.size(); ++i) {
                                    newPositions[state.calculatedCpOrder[i]] = state.calculatedCpPositions[i];
                                }
                                state.calculatedCpPositions = newPositions;
                            }
                        } else {
                            state.calculatedCpOrder.resize(state.calculatedCpPositions.size());
                            std::iota(state.calculatedCpOrder.begin(), state.calculatedCpOrder.end(), 0);
                        }
                    }
                    if (cpPositionsErrorMsg.empty()) {
                        std::ofstream positionsFile(input.outputPositionsFile, std::ios::trunc);
                        positionsFile << std::setprecision(2) << std::fixed;
                        for (auto& pos : state.calculatedCpPositions) {
                            positionsFile << pos.x << '\t' << pos.y << '\t' << pos.z << '\n';
                        }
                    }
                }
                ImGui::SameLine();
                std::string status = state.calculatedCpPositions.empty() ? "Waiting for start" : "Done";
                ImGui::Text(" Status: %s", status.c_str());

                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 0, 0, 255));
                if (cpPositionsErrorMsg.empty()) {
                    ImGui::Text("");
                } else {
                    ImGui::Text("Error: %s", cpPositionsErrorMsg.c_str());
                }
                ImGui::PopStyleColor();

                if (!state.calculatedCpPositions.empty()) {
                    auto path = state.calculatedCpOrder;
                    path.erase(path.begin());
                    std::vector<int16_t> path16Bit;
                    for (auto& node : path)
                        path16Bit.push_back(node);
                    auto pos = ImGui::GetCursorPos();
                    auto size = ImVec2(ImGui::GetWindowSize().x - pos.x, ImGui::GetWindowSize().y - pos.y);
                    drawSolutionGraph(state.calculatedCpPositions, path16Bit, {}, pos, size);
                }

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Replay visualizer (experimental)")) {
                static std::string replayVisulizerErrorMsg;
                if (ImGui::BeginTable("menuTableReplayVisualizer", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                    ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                    tableInputEntryFile("Replay/Ghost file", input.positionReplayFile, "Gbx\0*.Gbx\0All\0*.*\0", "Can be either Replay or Ghost file in Gbx format.");
                    ImGui::EndTable();
                }
                if (ImGui::Button("Visualize")) {
                    state.pathToVisualize.clear();
                    replayVisulizerErrorMsg.clear();
                    auto replayData = getReplayData(std::wstring(input.positionReplayFile, input.positionReplayFile + strlen(input.positionReplayFile)), replayVisulizerErrorMsg);

                    if (replayVisulizerErrorMsg.empty()) {
                        if (replayData.empty()) {
                            replayVisulizerErrorMsg = "No data found in file";
                        }
                        for (auto& rep : replayData) {
                            for (auto& sample : rep.ghostSamples) {
                                state.pathToVisualize.push_back(sample.pos);
                            }
                        }
                    }
                }

                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 0, 0, 255));
                if (replayVisulizerErrorMsg.empty()) {
                    ImGui::Text("");
                } else {
                    ImGui::Text("Error: %s", replayVisulizerErrorMsg.c_str());
                }
                ImGui::PopStyleColor();

                if (!state.pathToVisualize.empty()) {
                    auto pos = ImGui::GetCursorPos();
                    auto size = ImVec2(ImGui::GetWindowSize().x - pos.x, ImGui::GetWindowSize().y - pos.y);
                    auto pathToVisualizeCopy = state.pathToVisualize;
                    scaleAndTranslatePositionsToFit(pathToVisualizeCopy, pos, size, 5.0f);
                    drawPath(pathToVisualizeCopy);
                }

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        if (state.isGraphWindowOpen) {
            ImGui::SetNextWindowPos(ImVec2(200, 50), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(680 + 200, 680), ImGuiCond_Once);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
            ImGui::Begin("Solution graph", &state.isGraphWindowOpen, ImGuiWindowFlags_NoCollapse);
            auto size = ImGui::GetWindowSize();
            size.y -= fontSize * 1.5f;
            auto pos = ImGui::GetWindowPos();
            pos.y += fontSize * 1.5f;
            
            auto minXY = std::min(size.x, size.y);
            guiFont->Scale = std::max(0.5f, (minXY / 680) * 0.8f);
            ImGui::PushFont(guiFont);
            float tableWidth = (guiFont->Scale / 0.6f) * 200;

            if (state.solutionToShowId != -1) {
                auto graphSize = size;
                graphSize.x -= tableWidth;
                if (!state.cpPositionsVis.empty() && state.realCpPositionView) {
                    drawSolutionGraph(state.cpPositionsVis, state.solutionToShowInGraphWindow, {}, pos, graphSize, state.hoveredConnection);
                } else {
                    drawSolutionGraph(state.solutionToShowInGraphWindow, state.solutionToCompareToInGraphWindow, pos, graphSize, state.hoveredConnection);
                }
            }


            auto tablePosX = size.x - tableWidth - 2.0f;
            auto graphMenuTableSizeY = 5 * (2.0f + ImGui::GetTextLineHeight());
            auto diffTableSizeY = size.y - fontSize * 1.5f - graphMenuTableSizeY;
            if (state.solutionToShowId != -1 && state.solutionToCompareId != -1) {
                auto N = state.solutionToShowInGraphWindow.size();
                std::vector<int16_t> solution1(N);
                std::vector<int16_t> solution2(N);
                solution1[0] = state.solutionToShowInGraphWindow[0];
                solution2[0] = state.solutionToCompareToInGraphWindow[0];
                for (int i = 1; i < N; ++i) {
                    solution1[state.solutionToShowInGraphWindow[i - 1]] = state.solutionToShowInGraphWindow[i];
                    solution2[state.solutionToCompareToInGraphWindow[i - 1]] = state.solutionToCompareToInGraphWindow[i];
                }
                std::vector<int16_t> revSolution1(N + 1);
                std::vector<int16_t> revSolution2(N + 1);
                for (int i = 0; i < N; ++i) {
                    revSolution1[solution1[i]] = i;
                    revSolution2[solution2[i]] = i;
                }
                revSolution1[0] = int16_t(N);
                revSolution2[0] = int16_t(N);

                ImGui::SetCursorPos(ImVec2(tablePosX, fontSize * 1.5f));
                if (ImGui::BeginTable("diffTable", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY, ImVec2(tableWidth, diffTableSizeY))) {
                    std::string deltaTimeStr = { char(0xCE), char(0x94) }; // Utf-8 delta symbol
                    deltaTimeStr += "Time";
                    ImGui::TableSetupColumn(std::to_string(state.solutionToShowId + 1).c_str(), 0, 1.0f);
                    ImGui::TableSetupColumn(std::to_string(state.solutionToCompareId + 1).c_str(), 0, 1.0f);
                    ImGui::TableSetupColumn(deltaTimeStr.c_str(), 0, 0.8f);
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();

                    state.hoveredConnection = { 0,0 };
                    for (int i = 0; i < N; ++i) {
                        if (solution1[i] != solution2[i]) {
                            ImGui::TableNextColumn();
                            bool isSelected = false;
                            ImGui::PushID((std::to_string(i) + "_diffTable").c_str());
                            ImGui::Selectable("##", isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, ImGui::GetTextLineHeight()));
                            if (ImGui::IsItemHovered()) {
                                state.hoveredConnection = { NodeType(i), NodeType(solution1[i]) };
                            }
                            ImGui::PopID();
                            ImGui::SameLine();
                            ImGui::Text("%d-%d", i, solution1[i]);
                            ImGui::TableNextColumn();
                            ImGui::Text("%d-%d", i, solution2[i]);
                            ImGui::TableNextColumn();
                            auto diffTime = config.condWeights[solution1[i]][i][revSolution1[i]] - config.condWeights[solution2[i]][i][revSolution2[i]];
                            auto diffTimeLength = std::to_string(std::abs(diffTime)).size() + (std::abs(diffTime) < 10);
                            auto diffTimeTextWidth = diffTimeLength * ImGui::CalcTextSize("0").x + (diffTime < 0 ? ImGui::CalcTextSize("-.").x : ImGui::CalcTextSize(".").x);
                            ImGui::Dummy(ImVec2((0.8f/2.8f * tableWidth) - diffTimeTextWidth - tableWidth / 8, 0.0f));
                            ImGui::SameLine();
                            ImGui::Text("%.1f", diffTime / 10.f);
                        }
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::SetCursorPos(ImVec2(tablePosX, fontSize * 1.5f + diffTableSizeY));
            if (ImGui::BeginTable("graphMenuTable", 1, ImGuiTableFlags_SizingStretchSame, ImVec2(tableWidth, 0.0f))) {
                ImGui::TableSetupColumn("graphMenuTableColumn");
                ImGui::TableNextColumn();
                if (!state.cpPositionsVis.empty()) {
                    if (state.realCpPositionView) {
                        if (ImGui::Button("Change to abstract view", ImVec2(tableWidth, 0.f))) {
                            state.realCpPositionView = false;
                        }
                    } else {
                        if (ImGui::Button("Change to real view", ImVec2(tableWidth, 0.f))) {
                            state.realCpPositionView = true;
                        }
                    }
                }
                ImGui::TableNextColumn();
                if (state.solutionToShowId != -1) {
                    if (state.solutionToCompareToInGraphWindow != state.solutionToShowInGraphWindow) {
                        if (ImGui::Button("Set to compare", ImVec2(tableWidth, 0.f))) {
                            state.solutionToCompareToInGraphWindow = state.solutionToShowInGraphWindow;
                            state.solutionToCompareId = state.solutionToShowId;
                        }
                    }
                }
                ImGui::TableNextColumn();
                if (state.solutionToCompareId != -1) {
                    if (ImGui::Button("Disable compare", ImVec2(tableWidth, 0.f))) {
                        state.solutionToCompareToInGraphWindow.clear();
                    }
                }
                ImGui::TableNextColumn();
                if (state.solutionToShowId != -1) {
                    ImGui::Text("Viewing Solution %d", state.solutionToShowId + 1);
                }
                ImGui::TableNextColumn();
                if (state.solutionToCompareId != -1) {
                    ImGui::Text("Comparing against %d", state.solutionToCompareId + 1);
                }
                ImGui::EndTable();
            }
            ImGui::PopFont();

            ImGui::End();
            ImGui::PopStyleColor();
        }
        if (state.solutionToShowVariationId == -1) {
            state.isVariationsWindowOpen = false;
        }
        if (state.isVariationsWindowOpen) {
            ImGui::SetNextWindowPos(ImVec2(200, 50), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(680 + 200, 680), ImGuiCond_Once);
            ImGui::Begin("Variations", &state.isVariationsWindowOpen, ImGuiWindowFlags_NoCollapse);

            if (ImGui::BeginTable("infoTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 0);
                ImGui::TableSetupColumn("Input2", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                ImGui::Text("solution ID = %d", state.solutionToShowVariationId);
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                ImGui::Checkbox("include repeat node variations", &state.showRepeatNodeVariations);
                ImGui::EndTable();
            }

            auto& variations = state.showRepeatNodeVariations ? state.solutionToShowVariation.allVariations : state.solutionToShowVariation.variations;
            for (int i = 0; i < variations.size(); ++i) {
                auto solStr = createSolutionString(variations[i], config.repeatNodeMatrix, config.useRespawnMatrix);
                ImGui::SetNextItemWidth(-1);
                ImGui::InputText(("##variation" + std::to_string(i)).c_str(), solStr.data(), solStr.size(), ImGuiInputTextFlags_ReadOnly);
            }
            ImGui::End();
        }
        ImGui::PopFont();
    });
    std::quick_exit(0);
}

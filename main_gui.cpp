#define NOMINMAX
#include <windows.h>
#include <ShlObj.h>
#include <atlbase.h>
#include <iomanip>
#include <filesystem>
#include "gbxParser.h"
#include "solutionFinder.h"
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
void folderExplorer(char* outFilePath) {
    CComPtr<IFileOpenDialog> pFolderDlg;
    pFolderDlg.CoCreateInstance(CLSID_FileOpenDialog);

    FILEOPENDIALOGOPTIONS opt{};
    pFolderDlg->GetOptions(&opt);
    pFolderDlg->SetOptions(opt | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);

    if (SUCCEEDED(pFolderDlg->Show(MyImGui::Hwnd()))) {
        CComPtr<IShellItem> pSelectedItem;
        pFolderDlg->GetResult(&pSelectedItem);
        CComHeapPtr<wchar_t> pPath;
        pSelectedItem->GetDisplayName(SIGDN_FILESYSPATH, &pPath);
        auto length = std::wcslen(pPath.m_pData);
        for (int i = 0; i <= length; ++i) {
            outFilePath[i] = char(pPath.m_pData[i]);
        }
    }
}

std::vector<Position> readPositionsFile(const std::string& positionsFilePath) {
    std::ifstream positionsFile(positionsFilePath);
    std::vector<Position> cpPositions;
    Position pos;
    while (positionsFile >> pos.x >> pos.y >> pos.z) {
        cpPositions.push_back(pos);
    }
    return cpPositions;
}

struct FilterConnection {
    enum Status {
        Required, Banned, Optional, AutoBanned
    };
    FilterConnection() {}
    FilterConnection(std::pair<int, int> connection, Status status) : connection(connection), status(status) {}

    std::pair<int, int> connection;
    Status status;
};

struct ConnectionFinderSettings {
    int testedConnectionTime = 0;
    int minConnectionTime = 600;
    int maxConnectionTime = 100'000;
    int minDistance = 0;
    int maxDistance = 100'000;
    int minHeightDiff = -100'000;
    int maxHeightDiff = 100'000;
};

int main(int argc, char** argv) {
    CoInitialize(NULL);
    MyImGui::Init(u"Trackmania Path Finder");

    int ignoredValueInput = 600;
    int ignoredValue = ignoredValueInput * 10;
    int inputLimitValue = 100'000;
    int maxRepeatNodesToAdd = 100'000;
    int maxSolutionCountInput = 100;
    int maxTime = 10;
    constexpr int MinFontSize = 8;
    constexpr int MaxFontSize = 30;
    int fontSize = 16;
    std::vector<BestSolution> bestFoundSolutions;

    char inputDataFile[1024] = { 0 };
    char inputPositionReplayFile[1024] = { 0 };
    char inputPositionReplayFilePath[1024] = { 0 };
    char inputReplayFolderPath[1024] = { 0 };
    char appendDataFile[1024] = { 0 };
    strcpy(appendDataFile, "");
    char outputDataFile[1024] = { 0 };
    strcpy(outputDataFile, "out.txt");
    char outputPositionsFile[1024] = { 0 };
    strcpy(outputPositionsFile, "CP_positions.txt");
    char outputSpreadsheetFile[1024] = { 0 };
    strcpy(outputSpreadsheetFile, "spreadsheet.csv");

    std::string errorMsg;
    std::future<void> algorithmRunTask;
    std::atomic<bool> taskWasCanceled = false;

    std::vector<int> repeatNodesTurnedOff;
    char inputTurnedOffRepeatNodes[1024] = { 0 };

    std::vector<int> ringCps;
    char inputRingCps[1024] = { 0 };

    std::vector<int16_t> cpOrder;
    std::vector<int16_t> calculatedCpOrder;
    char inputCpOrder[1024] = { 0 };

    char inputRouteString[1024] = { 0 };
    double outputRouteCalcTime = 0;

    std::future<void> matrixCreateTask;
    std::vector<std::vector<int>> createdMatrix;
    std::string createdMatrixLog;
    std::mutex createdMatrixLogMutex;
    Timer spreadSheetTimer;
    spreadSheetTimer.stop();

    std::vector<Position> calculatedCpPositions;

    std::vector<Position> pathToVisualize;

    bool isHeuristicAlgorithm = false;
    bool endedWithTimeout = false;
    std::thread timerThread;

    auto timer = Timer();
    timer.stop();

    std::atomic<bool> stopWorkingForConfig;
    SolutionConfig config(stopWorkingForConfig);
    config.ignoredValue = ignoredValue;
    config.maxSolutionCount = maxSolutionCountInput;
    config.partialSolutionCount = 0;
    config.stopWorking = false;

    bool isGraphWindowOpen = false;
    std::vector<int16_t> solutionToShowInGraphWindow;
    std::vector<int16_t> solutionToCompareToInGraphWindow;
    int solutionToShowId = -1;
    int solutionToCompareId = -1;
    std::vector<Position> cpPositionsVis;
    bool realCpPositionView = false;

    bool isVariationsWindowOpen = false;
    BestSolution solutionToShowVariation;
    int solutionToShowVariationId = -1;
    bool showRepeatNodeVariations = true;

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
    std::pair<NodeType, NodeType> hoveredConnection = { 0, 0 };

    bool isOnPathFinderTab = true;
    bool showAdvancedSettings = false;

    bool copiedBestSolutionsAfterAlgorithmDone = false;

    ConnectionFinderSettings connectionFinderSettingsInput;
    ConnectionFinderSettings connectionFinderSettings;
    bool isConnectionSearchAlgorithm = false;
    std::vector<Edge> connectionsToTest;
    bool sortConnectionSearchResultsByConnection = false;
    char inputSearchSourceNodes[1024] = { 0 };
    std::vector<int> searchSourceNodes;

    bool showResultsFilter = false;
    std::vector<FilterConnection> resultRequiredConnections;
    std::vector<FilterConnection> resultOptionalConnections;

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

        if (ImGui::BeginTable("menuTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
            ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
            ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
            ImGui::TableSetupColumn("Text2", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 2);
            ImGui::TableSetupColumn("Input2", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 3);
            tableInputEntry("font size", "You can use CTRL + Mouse wheel to change font size", [&]() {
                if (ImGui::InputInt("##font size", &fontSize)) {
                    fontSize = std::clamp(fontSize, MinFontSize, MaxFontSize);
                }
            });
            if (isOnPathFinderTab) {
                ImGui::TableNextColumn();
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                ImGui::Checkbox("show advanced settings", &showAdvancedSettings);
            }
            ImGui::EndTable();
        }

        if (ImGui::BeginTabBar("#Tabs", ImGuiTabBarFlags_None)) {
            if (ImGui::BeginTabItem("Path Finder")) {
                isOnPathFinderTab = true;
                if (ImGui::BeginTable("menuTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                    ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                    ImGui::TableSetupColumn("Text2", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 2);
                    ImGui::TableSetupColumn("Input2", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 3);
                    if (showAdvancedSettings) {
                        tableInputEntry("max connection time", "Connections with this or higher time\nwill not be considered in the solutions", [&]() {
                            if (ImGui::InputInt("##max node value threshold", &ignoredValueInput)) {
                                ignoredValueInput = std::clamp(ignoredValueInput, 1, 100'000);
                                ignoredValue = ignoredValueInput * 10;
                            }
                        });
                    
                        tableInputEntry("max route time", "", [&]() {
                            if (ImGui::InputInt("##max solution length", &inputLimitValue)) {
                                inputLimitValue = std::clamp(inputLimitValue, 1, 100'000);
                            }
                        });
                    }
                    tableInputEntry("max nr of routes", "Number of fastest routes you want to find.\n\nUnless you are working with a small number of connections you should not set it to an arbitrarily high value - this parameter plays a key role in how long the search process will take so you should set it to something reasonable", [&]() {
                        if (ImGui::InputInt("##max number of routes", &maxSolutionCountInput)) {
                            maxSolutionCountInput = std::clamp(maxSolutionCountInput, 1, 100'000);
                        }
                    });
                    tableInputEntry("max search time", "Max time in seconds you want to search for.\n\nIt's mostly useful for heuristic algorithm since it will usually find most if not all top 100 solutions in the first ~10 seconds even for hard problems\n\nMight need to increase that time for some problems - you have to experiment yourself.", [&]() {
                        if (ImGui::InputInt("##max search time", &maxTime)) {
                            maxTime = std::clamp(maxTime, 1, 100'000);
                        }
                    });
                    if (showAdvancedSettings) {
                        tableInputEntry("max repeat CPs to add", "", [&]() {
                            if (ImGui::InputInt("##max repeat nodes to add", &maxRepeatNodesToAdd)) {
                                maxRepeatNodesToAdd = std::clamp(maxRepeatNodesToAdd, 0, 100'000);
                            }
                        });
                        tableInputEntry("turned off repeat CPs", "List of CP numbers you want to ban from repeating", [&]() {
                            if (ImGui::InputText("##turned off repeat nodes", inputTurnedOffRepeatNodes, sizeof(inputTurnedOffRepeatNodes))) {
                                auto nodes = splitLineOfFloatsToInts(inputTurnedOffRepeatNodes, ignoredValue);
                                repeatNodesTurnedOff.clear();
                                for (auto node : nodes) {
                                    if (node != ignoredValue)
                                        repeatNodesTurnedOff.push_back(node);
                                }
                            }
                        });
                        tableInputEntry("output data file", "After completing running the algorithm this file\nwill have sorted list of top \"max number of routes\" found.", [&]() {
                            ImGui::InputText("##output data file", outputDataFile, sizeof(outputDataFile));
                        });
                        tableInputEntry("append data file", "Every time candidate route is found it's saved to this file.\nThe data will be added to the end of the file\nwithout removing what was there before.\n\nYou have to sort that list yourself to find best routes.\n\nBy default it's empty so it's turned off", [&]() {
                            ImGui::InputText("##append data file", appendDataFile, sizeof(appendDataFile));
                        });
                        if (config.weights.size() > 0) {
                            tableInputEntry("calculate route time", "", [&]() {
                                ImGui::PushID("Calculate route button");
                                if (ImGui::Button("Calculate time")) {
                                    std::string routeString = inputRouteString;
                                    auto [sol, routeTime] = createSolutionFromString(routeString, config);
                                    outputRouteCalcTime = routeTime;
                                }
                                ImGui::PopID();
                                ImGui::SameLine();
                                ImGui::SetNextItemWidth(-1);
                                ImGui::InputText("##input route string", inputRouteString, sizeof(inputRouteString));
                            });
                            ImGui::TableNextColumn();
                            ImGui::SetNextItemWidth(-1);
                            if (outputRouteCalcTime == -1.0) {
                                ImGui::Text("Error");
                            } else {
                                ImGui::Text("Time = %.1f", outputRouteCalcTime);
                            }
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1);
                if (ImGui::BeginTable("menuTable2", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                    ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
            
                    if (showAdvancedSettings) {
                        tableInputEntry("ring CPs", "List of CP numbers that are rings.\nThat is CPs for which you want to include connection\nwhere you standing respawn after taking this CP\nto go back to previous CP", [&]() {
                            if (ImGui::InputText("##ring cps", inputRingCps, sizeof(inputRingCps))) {
                                auto nodes = splitLineOfFloatsToInts(inputRingCps, ignoredValue);
                                ringCps.clear();
                                for (auto node : nodes) {
                                    if (node != ignoredValue)
                                        ringCps.push_back(node);
                                }
                            }
                        });
                        tableInputEntry("CP positions file", "Optional for visualization only.\nFile containing positions of start, finish and all CPs created in \"CP positions creactor\" tab.\n\nWhen provided it enables graph view to show 2D view of the route", [&]() {
                            if (ImGui::Button("Find file")) {
                                fileExplorer(inputPositionReplayFilePath, "txt\0*.txt\0All\0*.*\0");
                            }
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1);
                            ImGui::InputText("##positions file", inputPositionReplayFilePath, sizeof(inputPositionReplayFilePath));
                        });
                    }
                    tableInputEntry("input data file", "Format is full matrix of decimal values in CSV format\nusing any delimiter, e.g. comma, space, tab.\n\nFirst row are times to CP1.\nLast row are times to finish.\nFirst column are times from start.\nLast column are times from last CP.", [&]() {
                        ImGui::PushID("Input data file button");
                        if (ImGui::Button("Find file")) {
                            fileExplorer(inputDataFile, "All\0*.*\0CSV\0*.CSV\0");
                        }
                        ImGui::PopID();
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1);
                        ImGui::InputText("##input data file", inputDataFile, sizeof(inputDataFile));
                    });
                    ImGui::EndTable();
                }

                auto sortBestFoundSolutionsSolutions = [&]() {
                    std::sort(bestFoundSolutions.begin(), bestFoundSolutions.end(), [&](BestSolution& a, auto& b) {
                        if (isConnectionSearchAlgorithm) {
                            if (!sortConnectionSearchResultsByConnection) {
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

                if (showAdvancedSettings) {
                    ImGui::Checkbox("Connection finder mode", &isConnectionSearchAlgorithm);
                }
                if (showAdvancedSettings && isConnectionSearchAlgorithm) {
                    if (ImGui::BeginTable("ConnectionFinderTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                        ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                        ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                        ImGui::TableSetupColumn("Text2", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 2);
                        ImGui::TableSetupColumn("Input2", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 3);
                        tableInputEntry("min connection time", "Connections with this or higher time\nwill be tried to be replaced with tested time", [&]() {
                            if (ImGui::InputInt("##finder min node connection", &connectionFinderSettingsInput.minConnectionTime)) {
                                connectionFinderSettingsInput.minConnectionTime = std::clamp(connectionFinderSettingsInput.minConnectionTime, 0, 100'000);
                            }
                        });
                        tableInputEntry("max connection time", "Connections with this or lower time\nwill be tried to be replaced with tested time", [&]() {
                            if (ImGui::InputInt("##finder max node connection", &connectionFinderSettingsInput.maxConnectionTime)) {
                                connectionFinderSettingsInput.maxConnectionTime = std::clamp(connectionFinderSettingsInput.maxConnectionTime, 0, 100'000);
                            }
                        });

                        tableInputEntry("min distance", "Connections with this or higher distance\nwill be tried to be replaced with tested time\n\nNOTE: Requires CP positions file", [&]() {
                            if (inputPositionReplayFilePath[0] == '\0') ImGui::BeginDisabled();
                            if (ImGui::InputInt("##finder min dist connection", &connectionFinderSettingsInput.minDistance)) {
                                connectionFinderSettingsInput.minDistance = std::clamp(connectionFinderSettingsInput.minDistance, 0, 100'000);
                            }
                            if (inputPositionReplayFilePath[0] == '\0') ImGui::EndDisabled();
                        });
                        tableInputEntry("max distance", "Connections with this or lower distance\nwill be tried to be replaced with tested time\n\nNOTE: Requires CP positions file", [&]() {
                            if (inputPositionReplayFilePath[0] == '\0') ImGui::BeginDisabled();
                            if (ImGui::InputInt("##finder max dist connection", &connectionFinderSettingsInput.maxDistance)) {
                                connectionFinderSettingsInput.maxDistance = std::clamp(connectionFinderSettingsInput.maxDistance, 0, 100'000);
                            }
                            if (inputPositionReplayFilePath[0] == '\0') ImGui::EndDisabled();
                        });

                        tableInputEntry("min height difference", "Connections with this or higher Height(To_CP) - Height(From_CP)\nwill be tried to be replaced with tested time\n\nNOTE: Requires CP positions file", [&]() {
                            if (inputPositionReplayFilePath[0] == '\0') ImGui::BeginDisabled();
                            if (ImGui::InputInt("##finder min height connection", &connectionFinderSettingsInput.minHeightDiff)) {
                                connectionFinderSettingsInput.minHeightDiff = std::clamp(connectionFinderSettingsInput.minHeightDiff, -100'000, 100'000);
                            }
                            if (inputPositionReplayFilePath[0] == '\0') ImGui::EndDisabled();
                        });
                        tableInputEntry("max height difference", "Connections with this or lower Height(To_CP) - Height(From_CP)\nwill be tried to be replaced with tested time\n\nNOTE: Requires CP positions file", [&]() {
                            if (inputPositionReplayFilePath[0] == '\0') ImGui::BeginDisabled();
                            if (ImGui::InputInt("##finder max height connection", &connectionFinderSettingsInput.maxHeightDiff)) {
                                connectionFinderSettingsInput.maxHeightDiff = std::clamp(connectionFinderSettingsInput.maxHeightDiff, -100'000, 100'000);
                            }
                            if (inputPositionReplayFilePath[0] == '\0') ImGui::EndDisabled();
                        });

                        tableInputEntry("tested time", "Time that will be inserted into tested connections", [&]() {
                            if (ImGui::InputInt("##finder tested connection time", &connectionFinderSettingsInput.testedConnectionTime)) {
                                connectionFinderSettingsInput.testedConnectionTime = std::clamp(connectionFinderSettingsInput.testedConnectionTime, 0, 100'000);
                            }
                        });

                        tableInputEntry("source CPs", "List of source CP numbers that should be considered while searching.\nIf empty - all CPs are considered", [&]() {
                            if (ImGui::InputText("##sourceCPs", inputSearchSourceNodes, sizeof(inputSearchSourceNodes))) {
                                auto nodes = splitLineOfFloatsToInts(inputSearchSourceNodes, ignoredValue);
                                searchSourceNodes.clear();
                                for (auto node : nodes) {
                                    if (node != ignoredValue)
                                        searchSourceNodes.push_back(node);
                                }
                            }
                        });

                        ImGui::TableNextColumn();
                        ImGui::SetNextItemWidth(-1);
                        if (ImGui::Checkbox("sort by connection", &sortConnectionSearchResultsByConnection)) {
                            sortBestFoundSolutionsSolutions();
                        }

                        ImGui::EndTable();
                    }
                }

                if (!errorMsg.empty()) {
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 0, 0, 255));
                    ImGui::Text("Error: %s", errorMsg.c_str());
                    ImGui::PopStyleColor();
                }

                auto startAlgorithm = [&](bool isExactAlgorithm) {
                    if (!isRunning(algorithmRunTask)) {
                        errorMsg = "";
                        taskWasCanceled = false;
                        endedWithTimeout = false;
                        bestFoundSolutions.clear();

                        isGraphWindowOpen = false;
                        solutionToShowInGraphWindow.clear();
                        solutionToCompareToInGraphWindow.clear();

                        copiedBestSolutionsAfterAlgorithmDone = false;
                        showResultsFilter = false;
                        resultRequiredConnections.clear();
                        resultOptionalConnections.clear();

                        config.limit = inputLimitValue * 10;
                        config.ignoredValue = ignoredValue;
                        config.maxSolutionCount = maxSolutionCountInput;
                        config.appendFileName = appendDataFile;
                        config.outputFileName = outputDataFile;
                        config.partialSolutionCount = 0;
                        config.stopWorking = false;
                        config.repeatNodeMatrix.clear();
                        config.solutionsVec.clear();
                        config.addedConnection = NullEdge;

                        auto [A_, B_] = loadCsvData(inputDataFile, config.ignoredValue, errorMsg);
                        config.weights = A_;
                        config.condWeights = B_;
                        config.useRespawnMatrix = Vector3d<Bool>(int(config.condWeights.size()));

                        cpPositionsVis.clear();
                        if (inputPositionReplayFilePath[0] != '\0') {
                            cpPositionsVis = readPositionsFile(inputPositionReplayFilePath);
                            if (cpPositionsVis.size() != config.weights.size()) {
                                errorMsg = "incorrect CP position file - wrong number of CPs";
                            }
                        }

                        if (errorMsg.empty()) {
                            timer = Timer();
                            timerThread = std::thread([&taskWasCanceled, &timer, maxTime, &config, &endedWithTimeout]() {
                                while (!config.stopWorking && !taskWasCanceled && timer.getTime() < maxTime) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                                }
                                endedWithTimeout = timer.getTime() >= maxTime;
                                config.stopWorking = true;
                            });
                            if (isConnectionSearchAlgorithm) {
                                connectionFinderSettings = connectionFinderSettingsInput;
                                connectionFinderSettings.minConnectionTime *= 10;
                                connectionFinderSettings.maxConnectionTime *= 10;
                                connectionFinderSettings.testedConnectionTime *= 10;
                                config.useExtendedMatrix = isUsingExtendedMatrix(config.condWeights);
                                config.bestSolutions.clear();
                                connectionsToTest.clear();
                                algorithmRunTask = std::async(std::launch::async | std::launch::deferred, [isExactAlgorithm, &timer, &config, &taskWasCanceled, &timerThread, &endedWithTimeout, &maxRepeatNodesToAdd, &repeatNodesTurnedOff, &cpPositionsVis, &ringCps, &connectionFinderSettings, &connectionsToTest, &searchSourceNodes]() mutable {
                                    auto privateConfig = config;
                                    privateConfig.maxSolutionCount = 1;

                                    for (int src = 0; src < config.weights.size() - 1; ++src) {
                                        if (!searchSourceNodes.empty() && std::find(searchSourceNodes.begin(), searchSourceNodes.end(), src) == searchSourceNodes.end())
                                            continue;
                                        for (int dst = 1; dst < config.weights.size(); ++dst) {
                                            if (src == dst)
                                                continue;

                                            if (config.weights[dst][src] > connectionFinderSettings.maxConnectionTime)
                                                continue;
                                            if (config.weights[dst][src] < connectionFinderSettings.minConnectionTime)
                                                continue;

                                            if (!cpPositionsVis.empty()) {
                                                auto srcPos = cpPositionsVis[src];
                                                auto dstPos = cpPositionsVis[dst];
                                                if (dstPos.y - srcPos.y > connectionFinderSettings.maxHeightDiff)
                                                    continue;
                                                if (dstPos.y - srcPos.y < connectionFinderSettings.minHeightDiff)
                                                    continue;
                                                if (dist3d(srcPos, dstPos) > connectionFinderSettings.maxDistance)
                                                    continue;
                                                if (dist3d(srcPos, dstPos) < connectionFinderSettings.minDistance)
                                                    continue;
                                            }
                                            connectionsToTest.emplace_back(src, dst);
                                        }
                                    }
                                    for (auto [src, dst] : connectionsToTest) {
                                        if (config.stopWorking)
                                            break;

                                        privateConfig.addedConnection = Edge{ src, dst };
                                        privateConfig.weights[dst][src] = connectionFinderSettings.testedConnectionTime;
                                        privateConfig.bestSolutions.clear();
                                        privateConfig.solutionsVec.clear();
                                        privateConfig.limit = config.limit;
                                        std::fill(privateConfig.condWeights[dst][src].begin(), privateConfig.condWeights[dst][src].end(), privateConfig.weights[dst][src]);
                                        privateConfig.repeatNodeMatrix = addRepeatNodeEdges(privateConfig.weights, privateConfig.condWeights, privateConfig.ignoredValue, maxRepeatNodesToAdd, repeatNodesTurnedOff);
                                        addRingCps(config, ringCps);
                                        privateConfig.weights = createAtspMatrixFromInput(privateConfig.weights);
                                        std::fill(privateConfig.condWeights[0].back().begin(), privateConfig.condWeights[0].back().end(), 0);
                                        if (isExactAlgorithm) {
                                            findSolutionsPriority(privateConfig);
                                        } else {
                                            findSolutionsHeuristic(privateConfig);
                                        }

                                        if (!privateConfig.bestSolutions.empty()) {
                                            auto& newSolution = privateConfig.bestSolutions[0];
                                            config.solutionsVec.push_back_not_thread_safe(newSolution);
                                            insertSorted(config.bestSolutions, newSolution, [](auto& a, auto& b) {
                                                if (a.time < b.time)
                                                    return true;
                                                if (a.time > b.time)
                                                    return false;
                                                return a.solution < b.solution;
                                            });
                                            if (config.bestSolutions.size() >= config.maxSolutionCount) {
                                                config.limit = config.bestSolutions.back().time;
                                                config.bestSolutions.pop_back();
                                            }
                                        }

                                        privateConfig.weights = config.weights;
                                        privateConfig.condWeights = config.condWeights;
                                        config.partialSolutionCount += 1;
                                    }
                                    config.stopWorking = true;
                                    timerThread.join();
                                    timer.stop();
                                });
                            } else {
                                config.repeatNodeMatrix = addRepeatNodeEdges(config.weights, config.condWeights, config.ignoredValue, maxRepeatNodesToAdd, repeatNodesTurnedOff);
                                addRingCps(config, ringCps);
                                clearFile(config.outputFileName);
                                writeSolutionFileProlog(config.appendFileName, inputDataFile, config.limit, isExactAlgorithm, maxRepeatNodesToAdd > 0, repeatNodesTurnedOff);
                                algorithmRunTask = std::async(std::launch::async | std::launch::deferred, [isExactAlgorithm, &timer, &config, &taskWasCanceled, &timerThread, &endedWithTimeout]() mutable {
                                    config.weights = createAtspMatrixFromInput(config.weights);
                                    std::fill(config.condWeights[0].back().begin(), config.condWeights[0].back().end(), 0);
                                    config.useExtendedMatrix = isUsingExtendedMatrix(config.condWeights);
                                    config.bestSolutions.clear();
                                    if (isExactAlgorithm) {
                                        findSolutionsPriority(config);
                                    } else {
                                        findSolutionsHeuristic(config);
                                    }
                                    config.stopWorking = true;
                                    timerThread.join();
                                    writeSolutionFileEpilog(config.appendFileName, taskWasCanceled, endedWithTimeout);
                                    overwriteFileWithSortedSolutions(config.outputFileName, config.maxSolutionCount, config.solutionsVec, config.repeatNodeMatrix, config.useRespawnMatrix);
                                    timer.stop();
                                });
                            }
                        }
                    } else {
                        errorMsg = "Already running";
                    }
                };
                bool disableAlgorithmButtons = isRunning(algorithmRunTask);
                if (disableAlgorithmButtons)
                    ImGui::BeginDisabled();
                if (ImGui::Button("Run exact algorithm")) {
                    isHeuristicAlgorithm = false;
                    startAlgorithm(true);
                }
                if (showAdvancedSettings) {
                    ImGui::SameLine();
                    if (ImGui::Button("Run heuristic algorithm")) {
                        isHeuristicAlgorithm = true;
                        startAlgorithm(false);
                    }
                }
                if (disableAlgorithmButtons)
                    ImGui::EndDisabled();
                if (isRunning(algorithmRunTask)) {
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel")) {
                        taskWasCanceled = true;
                    }
                }
                if (copiedBestSolutionsAfterAlgorithmDone) {
                    ImGui::SameLine();
                    ImGui::Checkbox("Show results connection filter", &showResultsFilter);
                }

                auto getRequiredAndOptionalConnectionSets = [&]() -> std::pair<FastSet2d, FastSet2d> {
                    if (bestFoundSolutions.empty())
                        return {};
                    auto allConnectionsSet = bestFoundSolutions[0].solutionConnections;
                    auto commonConnectionsSet = bestFoundSolutions[0].solutionConnections;
                    for (auto& solution : bestFoundSolutions) {
                        allConnectionsSet |= solution.solutionConnections;
                        commonConnectionsSet &= solution.solutionConnections;
                    }
                    auto diffConnectionsSet = allConnectionsSet;
                    diffConnectionsSet -= commonConnectionsSet;

                    return { commonConnectionsSet, diffConnectionsSet };
                };

                if (showResultsFilter) {
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
                    for (int i = 0; i < resultRequiredConnections.size(); ++i) {
                        ImGui::PushID(("Required connection" + std::to_string(i)).c_str());
                        addConnectionButton(resultRequiredConnections[i], true);
                        float lastButtonX = ImGui::GetItemRectMax().x;
                        float nextButtonX = lastButtonX + style.ItemSpacing.x + buttonSize.x;
                        if (i + 1 < resultRequiredConnections.size() && nextButtonX < windowVisibleX)
                            ImGui::SameLine();
                        ImGui::PopID();
                    }

                    ImGui::Text("Optional connections:");
                    for (int i = 0; i < resultOptionalConnections.size(); ++i) {
                        ImGui::PushID(("Optional connection" + std::to_string(i)).c_str());
                        if (addConnectionButton(resultOptionalConnections[i])) {
                            if (resultOptionalConnections[i].status == FilterConnection::Optional)
                                resultOptionalConnections[i].status = FilterConnection::Banned;
                            else if (resultOptionalConnections[i].status == FilterConnection::Banned)
                                resultOptionalConnections[i].status = FilterConnection::Optional;

                            bestFoundSolutions.clear();
                            FastSet2d bannedConnections(config.weights.size());
                            for (auto& c : resultOptionalConnections) {
                                if (c.status == FilterConnection::Banned) {
                                    bannedConnections.set(c.connection.first, c.connection.second);
                                }
                            }
                            for (auto& s : config.bestSolutions) {
                                auto set = s.solutionConnections;
                                set &= bannedConnections;
                                if (!set.any()) {
                                    bestFoundSolutions.push_back(s);
                                }
                            }
                            auto [commonConnectionsSet, optionalConnectionsSet] = getRequiredAndOptionalConnectionSets();

                            for (auto& c : resultOptionalConnections) {
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
                        if (i + 1 < resultOptionalConnections.size() && nextButtonX < windowVisibleX)
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
                    if (!algorithmRunTask.valid()) {
                        status = "Waiting for start";
                    } else if (endedWithTimeout) {
                        if (isRunning(algorithmRunTask))
                            status = "Timeout (Still running)";
                        else
                            status = "Timeout";
                    } else if (taskWasCanceled) {
                        if (isRunning(algorithmRunTask))
                            status = "Canceled (Still running)";
                        else
                            status = "Canceled";
                    } else if (isRunning(algorithmRunTask)) {
                        status = "Running";
                    } else {
                        status = "Done";
                    }
                    ImGui::Text("%s", status.c_str());

                    ImGui::TableNextColumn();
                    ImGui::Text("%.1f [s]", timer.getTime());

                    ImGui::TableNextColumn();
                    ImGui::Text("%d", config.solutionsVec.size());

                    ImGui::TableNextColumn();
                    if (isConnectionSearchAlgorithm) {
                        ImGui::Text("Checked %d / %d connections", config.partialSolutionCount.load(), connectionsToTest.size());
                    } else if (isHeuristicAlgorithm) {
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

                if (config.stopWorking && !copiedBestSolutionsAfterAlgorithmDone) {
                    copiedBestSolutionsAfterAlgorithmDone = true;
                    bestFoundSolutions = config.bestSolutions;
                    sortBestFoundSolutionsSolutions();
                    auto [commonConnectionsSet, optionalConnectionsSet] = getRequiredAndOptionalConnectionSets();
                    for (auto& connection : commonConnectionsSet.toSortedList()) {
                        resultRequiredConnections.emplace_back(connection, FilterConnection::Required);
                    }
                    for (auto& connection : optionalConnectionsSet.toSortedList()) {
                        resultOptionalConnections.emplace_back(connection, FilterConnection::Optional);
                    }
                } else if (!copiedBestSolutionsAfterAlgorithmDone && config.solutionsVec.size() > bestFoundSolutions.size()) {
                    for (int i = int(bestFoundSolutions.size()); i < config.solutionsVec.size(); ++i) {
                        bestFoundSolutions.push_back(config.solutionsVec[i]);
                    }
                    sortBestFoundSolutionsSolutions();
                }
                auto bestSolutionCount = std::min<int>(int(bestFoundSolutions.size()), config.maxSolutionCount);
                auto maxSolutionTime = bestFoundSolutions.empty() ? 0 : std::max_element(bestFoundSolutions.begin(), bestFoundSolutions.begin() + bestSolutionCount, [](auto& a, auto& b) { return a.time < b.time; })->time;
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
                        if (bestFoundSolutions[i].solution == solution) {
                            id = i;
                            return;
                        }
                    }
                    id = -1;
                    solution.clear();
                };
                updateSolutionId(solutionToShowId, solutionToShowInGraphWindow);
                updateSolutionId(solutionToCompareId, solutionToCompareToInGraphWindow);
                
                if (ImGui::BeginTable("solutionsTable", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, textDigitWidth * (std::to_string(config.maxSolutionCount).size() + 1), 0);
                    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, textDigitWidth * (std::max(4, int(std::to_string(maxSolutionTime).size())) + 1), 1);
                    ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 3.5f, 2);
                    if (isConnectionSearchAlgorithm) {
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
                            auto& B_ = bestFoundSolutions[j].solution;
                            auto time = bestFoundSolutions[j].time;
                            auto solStr = bestFoundSolutions[j].solutionString;// std::string("");// createSolutionString(B_, config.repeatNodeMatrix, config.useRespawnMatrix);
                    
                            ImGui::TableNextColumn();
                            addNumberPadding(j + 1, config.maxSolutionCount);
                            ImGui::Text("%d", j + 1);

                            ImGui::TableNextColumn();
                            addNumberPadding(time, maxSolutionTime);
                            ImGui::Text("%.1f", time / 10.0);

                            ImGui::TableNextColumn();
                            ImGui::PushID((std::to_string(j) + "_solution_button").c_str());
                            if (ImGui::Button("Graph")) {
                                isGraphWindowOpen = true;
                                solutionToShowId = j;
                                solutionToShowInGraphWindow = B_;
                            }
                            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                                if (solutionToCompareToInGraphWindow == B_) {
                                    solutionToCompareId = -1;
                                    solutionToCompareToInGraphWindow.clear();
                                } else {
                                    solutionToCompareId = j;
                                    solutionToCompareToInGraphWindow = B_;
                                }
                            }
                            ImGui::PopID();
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetNextWindowSize(ImVec2(350, 375));
                                ImGui::BeginTooltip();
                                auto pos = ImGui::GetWindowPos();
                                drawSolutionGraph(B_, solutionToCompareToInGraphWindow, pos, ImVec2(350, 350));
                                auto& drawList = *ImGui::GetWindowDrawList();
                                std::string text = (solutionToCompareToInGraphWindow == B_) ? "Right-click to disable compare" : "Right-click to compare against";
                                drawList.AddText(ImGui::GetDefaultFont(), 20, ImVec2(pos.x + 10, pos.y + 350), ImColor(255, 255, 255), text.c_str());
                                ImGui::EndTooltip();
                            }
                            ImGui::TableNextColumn();
                            if (bestFoundSolutions[j].addedConnection != NullEdge) {
                                auto& c = bestFoundSolutions[j].addedConnection;
                                ImGui::Text((std::to_string(c.first) + "->" + std::to_string(c.second)).c_str());
                            } else {
                                ImGui::PushID((std::to_string(j) + "_variations_button").c_str());
                                std::string variationButtonLabel = "Var(";
                                if (copiedBestSolutionsAfterAlgorithmDone) {
                                    variationButtonLabel += std::to_string(bestFoundSolutions[j].allVariations.size());
                                } else {
                                    variationButtonLabel += "???";
                                }
                                variationButtonLabel += ")";
                                if (!copiedBestSolutionsAfterAlgorithmDone || bestFoundSolutions[j].allVariations.size() == 1) {
                                    ImGui::BeginDisabled();
                                }
                                ImGui::SetNextItemWidth(-1);
                                if (ImGui::Button(variationButtonLabel.c_str(), ImVec2(-1, 0))) {
                                    isVariationsWindowOpen = true;
                                    solutionToShowVariationId = j;
                                    solutionToShowVariation = bestFoundSolutions[j];
                                }
                                if (!copiedBestSolutionsAfterAlgorithmDone || bestFoundSolutions[j].allVariations.size() == 1) {
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
                isOnPathFinderTab = false;
            }
            if (ImGui::BeginTabItem("CP positions creator (experimental)")) {
                static std::string cpPositionsErrorMsg;

                if (ImGui::BeginTable("menuTableCp", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                    ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                    tableInputEntry("Full Replay Gbx", "Can be either Replay or Ghost file in Gbx format.\nIt's assumed that it is a full replay from start to finish", [&]() {
                        if (ImGui::Button("Find file")) {
                            fileExplorer(inputPositionReplayFile, "Gbx\0*.Gbx\0All\0*.*\0");
                        }
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1);
                        ImGui::InputText("##full replay file", inputPositionReplayFile, sizeof(inputPositionReplayFile));
                    });
                    tableInputEntry("CP order (optional)", "Comma separated full list of CP numbers in order they were driven.\nIt's useful if you have no replay at hand that drives all CPs in the correct order.", [&]() {
                        if (ImGui::InputText("##CP order", inputCpOrder, sizeof(inputCpOrder))) {
                            auto nodes = splitLineOfFloatsToInts(inputCpOrder, ignoredValue);
                            cpOrder.clear();
                            for (auto node : nodes) {
                                if (node != ignoredValue)
                                    cpOrder.push_back(node);
                            }
                        }
                    });
                    tableInputEntry("output positions file", "Text file that will contain positions of start, CPs and finish.\nThis is used for automatic generation of spreadsheet values and visualizations.", [&]() {
                        ImGui::InputText("##output positions file", outputPositionsFile, sizeof(outputPositionsFile));
                    });
                    ImGui::EndTable();
                }
                if (ImGui::Button("Create positions file")) {
                    cpPositionsErrorMsg.clear();
                    calculatedCpPositions.clear();
                    auto replayData = getReplayData(std::wstring(inputPositionReplayFile, inputPositionReplayFile + strlen(inputPositionReplayFile)), cpPositionsErrorMsg);
                    if (cpPositionsErrorMsg.empty()) {
                        calculatedCpPositions = getCpPositions(replayData);
                        if (calculatedCpPositions.empty()) {
                            cpPositionsErrorMsg = "Failed to read positions";
                        } else if (calculatedCpPositions.size() == 1) {
                            cpPositionsErrorMsg = "Failed to find CP positions in replay file";
                        } else if (!cpOrder.empty()) {
                            std::vector<int16_t> path(calculatedCpPositions.size() - 2);
                            std::iota(path.begin(), path.end(), 1);
                            auto sortedCpOrder = cpOrder;
                            std::sort(sortedCpOrder.begin(), sortedCpOrder.end());
                            if (sortedCpOrder != path) {
                                calculatedCpPositions.clear();
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
                                calculatedCpOrder = cpOrder;
                                calculatedCpOrder.insert(calculatedCpOrder.begin(), 0);
                                calculatedCpOrder.push_back(int16_t(calculatedCpOrder.size()));
                                std::vector<Position> newPositions(calculatedCpPositions.size());
                                for (int i = 0; i < calculatedCpOrder.size(); ++i) {
                                    newPositions[calculatedCpOrder[i]] = calculatedCpPositions[i];
                                }
                                calculatedCpPositions = newPositions;
                            }
                        } else {
                            calculatedCpOrder.resize(calculatedCpPositions.size());
                            std::iota(calculatedCpOrder.begin(), calculatedCpOrder.end(), 0);
                        }
                    }
                    if (cpPositionsErrorMsg.empty()) {
                        std::ofstream positionsFile(outputPositionsFile, std::ios::trunc);
                        positionsFile << std::setprecision(2) << std::fixed;
                        for (auto& pos : calculatedCpPositions) {
                            positionsFile << pos.x << '\t' << pos.y << '\t' << pos.z << '\n';
                        }
                    }
                }
                ImGui::SameLine();
                std::string status = calculatedCpPositions.empty() ? "Waiting for start" : "Done";
                ImGui::Text(" Status: %s", status.c_str());

                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(220, 0, 0, 255));
                if (cpPositionsErrorMsg.empty()) {
                    ImGui::Text("");
                } else {
                    ImGui::Text("Error: %s", cpPositionsErrorMsg.c_str());
                }
                ImGui::PopStyleColor();

                if (!calculatedCpPositions.empty()) {
                    auto path = calculatedCpOrder;
                    path.erase(path.begin());
                    auto pos = ImGui::GetCursorPos();
                    auto size = ImVec2(ImGui::GetWindowSize().x - pos.x, ImGui::GetWindowSize().y - pos.y);
                    drawSolutionGraph(calculatedCpPositions, path, {}, pos, size);
                }

                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Spreadsheet creator (experimental)")) {
                if (ImGui::BeginTable("menuTableSpreadsheet", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                    ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                    tableInputEntry("folder with replays", "Folder containing all .Gbx replay/ghost files to be used for spreadsheet creation.\nIt searches all sub-folders.\nOnly files with .gbx extension are considered.\n\nWARNING: make sure all .gbx files in folder were driven on the correct map, otherwise you might include wrong data", [&]() {
                        if (ImGui::Button("Find folder")) {
                            folderExplorer(inputReplayFolderPath);
                        }
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1);
                        ImGui::InputText("##gbx replay folder", inputReplayFolderPath, sizeof(inputReplayFolderPath));
                    });
                    tableInputEntry("positions file", "File containing positions of start, finish and all CPs created in \"CP positions creactor\" tab.", [&]() {
                        if (ImGui::Button("Find file")) {
                            fileExplorer(inputPositionReplayFilePath, "txt\0*.txt\0All\0*.*\0");
                        }
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1);
                        ImGui::InputText("##positions file", inputPositionReplayFilePath, sizeof(inputPositionReplayFilePath));
                    });
                    tableInputEntry("output file", "Text file that will contain positions of start, CPs and finish.\nThis is used for automatic generation of spreadsheet values and visualizations.", [&]() {
                        ImGui::InputText("##output spreadsheet file", outputSpreadsheetFile, sizeof(outputSpreadsheetFile));
                    });
                    ImGui::EndTable();
                }
                static std::atomic<uint64_t> processedFilesTotalSize = 0;

                if (ImGui::Button("Create spreadsheet")) {
                    if (!isRunning(matrixCreateTask)) {
                        std::scoped_lock l{ createdMatrixLogMutex };
                        createdMatrixLog.clear();
                        createdMatrix.clear();
                        spreadSheetTimer = Timer();
                        processedFilesTotalSize = 0;
                        matrixCreateTask = std::async(std::launch::async, [&]() {
                            std::ofstream spreadsheetFile(outputSpreadsheetFile);
                            auto cpPositions = readPositionsFile(inputPositionReplayFilePath);
                            if (cpPositions.empty()) {
                                std::scoped_lock l{ createdMatrixLogMutex };
                                createdMatrixLog = "Failed to read CP positions file";
                                return;
                            }
                            createdMatrix.resize(cpPositions.size(), std::vector<int>(cpPositions.size(), 10000));
                            std::mutex matrixMutex;
                            ThreadPool threadPool;
                            for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(inputReplayFolderPath)) {
                                if (!dirEntry.is_regular_file())
                                    continue;
                                auto extension = dirEntry.path().extension().string();
                                std::transform(extension.begin(), extension.end(), extension.begin(), [](auto& c) { return std::tolower(c); });
                                if (extension != ".gbx")
                                    continue;
                                threadPool.addTask([dirEntry, &createdMatrix, &matrixMutex, &createdMatrixLog, &createdMatrixLogMutex, &cpPositions](int) {
                                    char out[512];
                                    auto wString = dirEntry.path().wstring();
                                    ImTextStrToUtf8(out, sizeof(out), (ImWchar*)wString.c_str(), (ImWchar*)wString.c_str() + wString.size());
                                    auto fileSize = std::filesystem::file_size(dirEntry.path());
                                    std::string error;
                                    auto repData = getReplayData(dirEntry.path().wstring(), error);
                                    processedFilesTotalSize += fileSize;
                                    std::string logEntry = std::string(out) + '\n';
                                    if (!error.empty()) {
                                        std::scoped_lock l{ createdMatrixLogMutex };
                                        createdMatrixLog.insert(0, logEntry + "\t\t" + error + "\n");
                                        return;
                                    }
                                    auto connections = getConnections(repData, cpPositions);
                                    for (auto& connection : connections) {
                                        logEntry += "\t\t" + std::to_string(connection.src) + " -> " + std::to_string(connection.dst) + " time = " + floatToString(connection.time / 10.f, 1) + "\n";
                                        std::scoped_lock l{ matrixMutex };
                                        if (createdMatrix[connection.dst][connection.src] > connection.time) {
                                            createdMatrix[connection.dst][connection.src] = connection.time;
                                        }
                                    }
                                    std::scoped_lock l{ createdMatrixLogMutex };
                                    createdMatrixLog += logEntry;
                                });
                            }
                            threadPool.wait();
                            spreadsheetFile << std::setprecision(1) << std::fixed;
                            for (int i = 1; i < createdMatrix.size(); ++i) {
                                for (int j = 0; j < createdMatrix.size() - 1; ++j) {
                                    spreadsheetFile << createdMatrix[i][j] / 10.f << "\t";
                                }
                                spreadsheetFile << '\n';
                            }
                            spreadSheetTimer.stop();
                        });
                    }
                }
                std::string status;
                if (!matrixCreateTask.valid()) {
                    status = "Waiting for start";
                } else if (isRunning(matrixCreateTask)) {
                    status = "Running";
                } else {
                    status = "Done";
                }
                ImGui::Text("Status: %s", status.c_str());
                if (processedFilesTotalSize != 0) {
                    if (spreadSheetTimer.getTime() > 0.2) {
                        ImGui::Text("Time: %.1f [s] (%d MB/s)", spreadSheetTimer.getTime(), int(processedFilesTotalSize.load() / 1'000'000 / spreadSheetTimer.getTime()));
                    } else {
                        ImGui::Text("Time: %.1f [s]", spreadSheetTimer.getTime());
                    }
                } else {
                    ImGui::Text("");
                }

                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 1));
                {
                    std::scoped_lock l{ createdMatrixLogMutex };
                    ImGui::InputTextMultiline("##", (char*)createdMatrixLog.c_str(), createdMatrixLog.size(), ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
                }
                ImGui::PopStyleColor();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Replay visualizer (experimental)")) {
                static std::string replayVisulizerErrorMsg;
                if (ImGui::BeginTable("menuTableReplayVisualizer", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                    ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12.0f, 0);
                    ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                    tableInputEntry("Replay/Ghost file", "Can be either Replay or Ghost file in Gbx format.", [&]() {
                        if (ImGui::Button("Find file")) {
                            fileExplorer(inputPositionReplayFile, "Gbx\0*.Gbx\0All\0*.*\0");
                        }
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(-1);
                        ImGui::InputText("##replay file", inputPositionReplayFile, sizeof(inputPositionReplayFile));
                    });
                    ImGui::EndTable();
                }
                if (ImGui::Button("Visualize")) {
                    pathToVisualize.clear();
                    replayVisulizerErrorMsg.clear();
                    auto replayData = getReplayData(std::wstring(inputPositionReplayFile, inputPositionReplayFile + strlen(inputPositionReplayFile)), replayVisulizerErrorMsg);

                    if (replayVisulizerErrorMsg.empty()) {
                        if (replayData.empty()) {
                            replayVisulizerErrorMsg = "No data found in file";
                        }
                        for (auto& rep : replayData) {
                            for (auto& sample : rep.ghostSamples) {
                                pathToVisualize.push_back(sample.pos);
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

                if (!pathToVisualize.empty()) {
                    auto pos = ImGui::GetCursorPos();
                    auto size = ImVec2(ImGui::GetWindowSize().x - pos.x, ImGui::GetWindowSize().y - pos.y);
                    auto pathToVisualizeCopy = pathToVisualize;
                    scaleAndTranslatePositionsToFit(pathToVisualizeCopy, pos, size, 5.0f);
                    drawPath(pathToVisualizeCopy);
                }

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();

        if (isGraphWindowOpen) {
            ImGui::SetNextWindowPos(ImVec2(200, 50), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(680 + 200, 680), ImGuiCond_Once);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 1));
            ImGui::Begin("Solution graph", &isGraphWindowOpen, ImGuiWindowFlags_NoCollapse);
            auto size = ImGui::GetWindowSize();
            size.y -= fontSize * 1.5f;
            auto pos = ImGui::GetWindowPos();
            pos.y += fontSize * 1.5f;
            
            auto minXY = std::min(size.x, size.y);
            guiFont->Scale = std::max(0.5f, (minXY / 680) * 0.8f);
            ImGui::PushFont(guiFont);
            float tableWidth = (guiFont->Scale / 0.6f) * 200;

            if (solutionToShowId != -1) {
                auto graphSize = size;
                graphSize.x -= tableWidth;
                if (!cpPositionsVis.empty() && realCpPositionView) {
                    drawSolutionGraph(cpPositionsVis, solutionToShowInGraphWindow, {}, pos, graphSize, hoveredConnection);
                } else {
                    drawSolutionGraph(solutionToShowInGraphWindow, solutionToCompareToInGraphWindow, pos, graphSize, hoveredConnection);
                }
            }


            auto tablePosX = size.x - tableWidth - 2.0f;
            auto graphMenuTableSizeY = 5 * (2.0f + ImGui::GetTextLineHeight());
            auto diffTableSizeY = size.y - fontSize * 1.5f - graphMenuTableSizeY;
            if (solutionToShowId != -1 && solutionToCompareId != -1) {
                auto N = solutionToShowInGraphWindow.size();
                std::vector<int16_t> solution1(N);
                std::vector<int16_t> solution2(N);
                solution1[0] = solutionToShowInGraphWindow[0];
                solution2[0] = solutionToCompareToInGraphWindow[0];
                for (int i = 1; i < N; ++i) {
                    solution1[solutionToShowInGraphWindow[i - 1]] = solutionToShowInGraphWindow[i];
                    solution2[solutionToCompareToInGraphWindow[i - 1]] = solutionToCompareToInGraphWindow[i];
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
                    ImGui::TableSetupColumn(std::to_string(solutionToShowId + 1).c_str(), 0, 1.0f);
                    ImGui::TableSetupColumn(std::to_string(solutionToCompareId + 1).c_str(), 0, 1.0f);
                    ImGui::TableSetupColumn(deltaTimeStr.c_str(), 0, 0.8f);
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();

                    hoveredConnection = { 0,0 };
                    for (int i = 0; i < N; ++i) {
                        if (solution1[i] != solution2[i]) {
                            ImGui::TableNextColumn();
                            bool isSelected = false;
                            ImGui::PushID((std::to_string(i) + "_diffTable").c_str());
                            ImGui::Selectable("##", isSelected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, ImGui::GetTextLineHeight()));
                            if (ImGui::IsItemHovered()) {
                                hoveredConnection = { NodeType(i), NodeType(solution1[i]) };
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
                if (!cpPositionsVis.empty()) {
                    if (realCpPositionView) {
                        if (ImGui::Button("Change to abstract view", ImVec2(tableWidth, 0.f))) {
                            realCpPositionView = false;
                        }
                    } else {
                        if (ImGui::Button("Change to real view", ImVec2(tableWidth, 0.f))) {
                            realCpPositionView = true;
                        }
                    }
                }
                ImGui::TableNextColumn();
                if (solutionToShowId != -1) {
                    if (solutionToCompareToInGraphWindow != solutionToShowInGraphWindow) {
                        if (ImGui::Button("Set to compare", ImVec2(tableWidth, 0.f))) {
                            solutionToCompareToInGraphWindow = solutionToShowInGraphWindow;
                            solutionToCompareId = solutionToShowId;
                        }
                    }
                }
                ImGui::TableNextColumn();
                if (solutionToCompareId != -1) {
                    if (ImGui::Button("Disable compare", ImVec2(tableWidth, 0.f))) {
                        solutionToCompareToInGraphWindow.clear();
                    }
                }
                ImGui::TableNextColumn();
                if (solutionToShowId != -1) {
                    ImGui::Text("Viewing Solution %d", solutionToShowId + 1);
                }
                ImGui::TableNextColumn();
                if (solutionToCompareId != -1) {
                    ImGui::Text("Comparing against %d", solutionToCompareId + 1);
                }
                ImGui::EndTable();
            }
            ImGui::PopFont();

            ImGui::End();
            ImGui::PopStyleColor();
        }
        if (solutionToShowVariationId == -1) {
            isVariationsWindowOpen = false;
        }
        if (isVariationsWindowOpen) {
            ImGui::SetNextWindowPos(ImVec2(200, 50), ImGuiCond_Once);
            ImGui::SetNextWindowSize(ImVec2(680 + 200, 680), ImGuiCond_Once);
            ImGui::Begin("Variations", &isVariationsWindowOpen, ImGuiWindowFlags_NoCollapse);

            if (ImGui::BeginTable("infoTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
                ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 0);
                ImGui::TableSetupColumn("Input2", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                ImGui::Text("solution ID = %d", solutionToShowVariationId);
                ImGui::TableNextColumn();
                ImGui::SetNextItemWidth(-1);
                ImGui::Checkbox("include repeat node variations", &showRepeatNodeVariations);
                ImGui::EndTable();
            }

            auto& variations = showRepeatNodeVariations ? solutionToShowVariation.allVariations : solutionToShowVariation.variations;
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

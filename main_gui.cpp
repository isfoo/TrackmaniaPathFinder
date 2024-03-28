#define NOMINMAX
#include <windows.h>
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

void drawSolutionGraph(std::vector<int16_t> solution, std::vector<int16_t> compare, ImVec2 pos, ImVec2 size, std::pair<NodeType, NodeType> hoveredConnection = { 0, 0 }) {
	compare.insert(compare.begin(), 0);
	solution.insert(solution.begin(), 0);

	int N = solution.size();

	std::vector<int> indexes(N);
	std::vector<int> revIndexes(N);
	std::iota(indexes.begin(), indexes.end(), 0);
	if (compare.size() == solution.size()) {
		for (int i = 0; i < N; ++i)
			indexes[i] = compare[i];
	}
	for (int i = 0; i < N; ++i)
		revIndexes[indexes[i]] = i;

	float minTextSize = 10.f;
	float maxTextSize = 20.f;
	auto minXY = std::min(size.x, size.y);
	float textSize = std::min(maxTextSize, std::max(minTextSize, 2.f * minXY / N));
	float r = (minXY / 2) - textSize * 2;
	
	auto& drawList = *ImGui::GetWindowDrawList();

	std::vector<float> x(N);
	std::vector<float> y(N);
	std::vector<float> textX(N);
	std::vector<float> textY(N);
	
	constexpr float Pi = 3.141592653589793238463;

	auto drawArrow = [&drawList, &x, &y, N, minXY](int i, int j, ImColor color, float thickness) {
		float arrowLength = thickness * std::min(12.f, 0.75f * minXY / N);
		float arrowAngle = 25;
		auto a = ImVec2(x[i], y[i]);
		auto b = ImVec2(x[j], y[j]);
		double theta = std::atan2(b.y - a.y, b.x - a.x);
		double rad = arrowAngle * (Pi / 180);
		double x1 = b.x - arrowLength * std::cos(theta + rad);
		double y1 = b.y - arrowLength * std::sin(theta + rad);
		double x2 = b.x - arrowLength * std::cos(theta - rad);
		double y2 = b.y - arrowLength * std::sin(theta - rad);
		drawList.AddLine(a, b, color, thickness);
		drawList.AddTriangleFilled(b, ImVec2(x1, y1), ImVec2(x2, y2), color);
	};

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

	for (int i = 0; i < N - 1; ++i) {
		float r, g, b;
		float h = i * (1.0f / N);
		ImGui::ColorConvertHSVtoRGB(h, 1.f, 1.f, r, g, b);
		bool isHoveredConnection = solution[i] == hoveredConnection.first && solution[i + 1] == hoveredConnection.second;
		drawArrow(revIndexes[solution[i]], revIndexes[solution[i + 1]], ImColor(r, g, b), isHoveredConnection ? 3.0f : 1.0f);
	}
	for (int i = 0; i < N; ++i) {
		auto text = (i == 0) ? std::string("S") : (i == N - 1) ? std::string("F") : std::to_string(indexes[i]);
		drawList.AddText(ImGui::GetDefaultFont(), textSize, ImVec2(textX[i], textY[i]), ImColor(255, 255, 255), text.c_str());
	}
}

int main(int argc, char** argv) {
	MyImGui::Init(u"Trackmania Path Finder");

	int ignoredValueInput = 600;
	int ignoredValue = ignoredValueInput * 10;
	int inputLimitValue = 100'000;
	int maxRepeatNodesToAdd = 0;
	int maxSolutionCountInput = 100;
	int maxTime = 10;
	constexpr int MinFontSize = 8;
	constexpr int MaxFontSize = 30;
	int fontSize = 16;
	std::vector<std::pair<std::vector<int16_t>, int>> bestFoundSolutions;

	char inputDataFile[1024] = { 0 };
	char appendDataFile[1024] = { 0 };
	strcpy(appendDataFile, "");
	char outputDataFile[1024] = { 0 };
	strcpy(outputDataFile, "out.txt");

	std::string errorMsg;
	std::future<void> algorithmRunTask;
	std::atomic<bool> taskWasCanceled = false;

	std::vector<int> repeatNodesTurnedOff;
	char inputTurnedOffRepeatNodes[1024] = { 0 };

	std::vector<int> ringCps;
	char inputRingCps[1024] = { 0 };

	bool isHeuristicAlgorithm = false;
	bool endedWithTimeout = false;
	std::thread timerThread;

	auto timer = Timer();
	timer.stop();

	SolutionConfig config;
	config.ignoredValue = ignoredValue;
	config.maxSolutionCount = maxSolutionCountInput;
	config.partialSolutionCount = 0;
	config.stopWorking = false;

	bool isGraphWindowOpen = false;
	std::vector<int16_t> solutionToShowInGraphWindow;
	std::vector<int16_t> solutionToCompareToInGraphWindow;
	int solutionToShowId = -1;
	int solutionToCompareId = -1;

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
			ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12, 0);
			ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
			ImGui::TableSetupColumn("Text2", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12, 2);
			ImGui::TableSetupColumn("Input2", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 3);
			tableInputEntry("font size", "You can use CTRL + Mouse wheel to change font size", [&]() {
				if (ImGui::InputInt("##font size", &fontSize)) {
					fontSize = std::clamp(fontSize, MinFontSize, MaxFontSize);
				}
			});
			ImGui::TableNextRow();
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
			tableInputEntry("max number of routes", "", [&]() {
				if (ImGui::InputInt("##max number of routes", &maxSolutionCountInput)) {
					maxSolutionCountInput = std::clamp(maxSolutionCountInput, 1, 100'000);
				}
			});
			tableInputEntry("max search time", "Max time in seconds you want to search for.\n\nIt's mostly useful for heuristic algorithm since it will usually find most if not all top 100 solutions in the first ~10 seconds even for hard problems\n\nMight need to increase that time for some problems - you have to experiment yourself.", [&]() {
				if (ImGui::InputInt("##max search time", &maxTime)) {
					maxTime = std::clamp(maxTime, 1, 100'000);
				}
			});
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
			ImGui::EndTable();
		}
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 1);
		if (ImGui::BeginTable("menuTable2", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
			ImGui::TableSetupColumn("Text1", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 12, 0);
			ImGui::TableSetupColumn("Input1", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 1);
			
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
			tableInputEntry("input data file", "Format is full matrix of decimal values in CSV format\nusing any delimiter, e.g. comma, space, tab.\n\nFirst row are times to CP1.\nLast row are times to finish.\nFirst column are times from start.\nLast column are times from last CP.", [&]() {
				if (ImGui::Button("Find file")) {
					OPENFILENAMEA ofn;
					CHAR szFile[260] = { 0 };
					ZeroMemory(&ofn, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = MyImGui::Hwnd();
					ofn.lpstrFile = szFile;
					ofn.nMaxFile = sizeof(szFile);
					ofn.lpstrFilter = "All\0*.*\0Text\0*.TXT\0";
					ofn.nFilterIndex = 1;
					ofn.lpstrFileTitle = NULL;
					ofn.nMaxFileTitle = 0;
					ofn.lpstrInitialDir = NULL;
					ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
					if (GetOpenFileNameA(&ofn) == TRUE) {
						std::strcpy(inputDataFile, ofn.lpstrFile);
					}
				}
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-1);
				ImGui::InputText("##input data file", inputDataFile, sizeof(inputDataFile));
			});
			ImGui::EndTable();
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

				config.limit = inputLimitValue * 10;
				config.ignoredValue = ignoredValue;
				config.maxSolutionCount = maxSolutionCountInput;
				config.appendFileName = appendDataFile;
				config.outputFileName = outputDataFile;
				config.partialSolutionCount = 0;
				config.stopWorking = false;
				config.repeatNodeMatrix.clear();
				config.solutionsVec.clear();

				auto [A_, B_] = loadCsvData(inputDataFile, config.ignoredValue, errorMsg);
				config.weights = A_;
				config.condWeights = B_;

				if (maxRepeatNodesToAdd > 0) {
					config.repeatNodeMatrix = addRepeatNodeEdges(config.weights, config.condWeights, config.ignoredValue, maxRepeatNodesToAdd, repeatNodesTurnedOff);
				}

				config.useRespawnMatrix = Vector3d<Bool>(int(config.condWeights.size()));
				for (auto ringCp : ringCps) {
					if (ringCp >= config.condWeights.size())
						continue;
					for (int i = 0; i < config.condWeights.size(); ++i) {
						if (std::find(ringCps.begin(), ringCps.end(), i) != ringCps.end())
							continue;
						for (int j = 0; j < config.condWeights.size(); ++j) {
							if (j == ringCp)
								continue;
							if (config.weights[ringCp][i] < config.ignoredValue && config.condWeights[j][i].back() < config.condWeights[j][ringCp][i]) {
								// faster to respawn from ringCp to i then go from i to j, then to directly go from ring to j
								config.condWeights[j][ringCp][i] = config.condWeights[j][i].back();
								config.useRespawnMatrix[j][ringCp][i] = true;
								if (!config.repeatNodeMatrix.empty()) {
									config.repeatNodeMatrix[j][ringCp][i] = config.repeatNodeMatrix[j][i].back();
								}
								config.weights[j][ringCp] = std::min(config.weights[j][ringCp], config.condWeights[j][i].back());
							}
						}
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
					clearFile(outputDataFile);
					writeSolutionFileProlog(appendDataFile, inputDataFile, config.limit, isExactAlgorithm, maxRepeatNodesToAdd > 0, repeatNodesTurnedOff);
					algorithmRunTask = std::async(std::launch::async | std::launch::deferred, [isExactAlgorithm, &timer, &config, &taskWasCanceled, &timerThread, &endedWithTimeout, appendDataFile, outputDataFile]() mutable {
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
						writeSolutionFileEpilog(appendDataFile, taskWasCanceled, endedWithTimeout);
						overwriteFileWithSortedSolutions(outputDataFile, config.maxSolutionCount, config.solutionsVec, config.repeatNodeMatrix, config.useRespawnMatrix);
						timer.stop();
					});
				}
			} else {
				errorMsg = "Already running";
			}
		};
		if (ImGui::Button("Run exact algorithm")) {
			isHeuristicAlgorithm = false;
			startAlgorithm(true);
		}
		ImGui::SameLine();
		if (ImGui::Button("Run heuristic algorithm")) {
			isHeuristicAlgorithm = true;
			startAlgorithm(false);
		}

		if (isRunning(algorithmRunTask)) {
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				taskWasCanceled = true;
			}
		}

		auto addNumberPadding = [](int value, int maxValue) {
			int paddingCount = std::to_string(maxValue).size() - std::to_string(value).size();
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
			if (isHeuristicAlgorithm) {
				auto n = config.partialSolutionCount.load();
				auto optVal = n >> 32;
				auto tryVal = n & 0xffffffff;
				ImGui::Text("Complated "); 
				ImGui::SameLine();
				addNumberPadding(tryVal, 1000);
				ImGui::Text("%d tries for %d-opt", tryVal, optVal);
			} else {
				ImGui::Text("Partial routes processed: %d", config.partialSolutionCount.load());
			}

			ImGui::EndTable();
		}

		if (config.solutionsVec.size() > bestFoundSolutions.size()) {
			for (int i = int(bestFoundSolutions.size()); i < config.solutionsVec.size(); ++i) {
				bestFoundSolutions.push_back(config.solutionsVec[i]);
			}
			std::sort(bestFoundSolutions.begin(), bestFoundSolutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
		}
		auto bestSolutionCount = std::min<int>(int(bestFoundSolutions.size()), config.maxSolutionCount);
		auto maxSolutionTime = bestFoundSolutions.empty() ? 0 : std::max_element(bestFoundSolutions.begin(), bestFoundSolutions.begin() + bestSolutionCount, [](auto& a, auto& b) { return a.second < b.second; })->second;
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
				if (bestFoundSolutions[i].first == solution) {
					id = i;
					return;
				}
			}
			id = -1;
			solution.clear();
		};
		updateSolutionId(solutionToShowId, solutionToShowInGraphWindow);
		updateSolutionId(solutionToCompareId, solutionToCompareToInGraphWindow);


		if (ImGui::BeginTable("solutionsTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable | ImGuiTableFlags_NoBordersInBody)) {
			ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, textDigitWidth * (std::to_string(config.maxSolutionCount).size() + 1), 0);
			ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, textDigitWidth * (std::max<int>(4, std::to_string(maxSolutionTime).size()) + 1), 1);
			ImGui::TableSetupColumn("Graph", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, fontSize * 3.5f, 2);
			ImGui::TableSetupColumn("Route", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_NoResize, 1.0f, 3);
			ImGui::TableHeadersRow();

			ImGuiListClipper clipper;
			clipper.Begin(bestSolutionCount);
			while (clipper.Step()) {
				for (int j = clipper.DisplayStart; j < clipper.DisplayEnd; ++j) {
					auto& [B_, time] = bestFoundSolutions[j];
					auto solStr = createSolutionString(B_, config.repeatNodeMatrix, config.useRespawnMatrix);
					
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
					ImGui::SetNextItemWidth(-1);
					ImGui::InputText(("##solution" + std::to_string(j)).c_str(), solStr.data(), solStr.size(), ImGuiInputTextFlags_ReadOnly);
				}
			}
			clipper.End();
			ImGui::EndTable();
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
				drawSolutionGraph(solutionToShowInGraphWindow, solutionToCompareToInGraphWindow, pos, graphSize, hoveredConnection);
			}


			auto tablePosX = size.x - tableWidth - 2.0f;
			auto graphMenuTableSizeY = 4 * (2.0f + ImGui::GetTextLineHeight());
			auto diffTableSizeY = size.y - fontSize * 1.5f - graphMenuTableSizeY;
			if (solutionToShowId != -1 && solutionToCompareId != -1) {
				auto N = solutionToShowInGraphWindow.size();
				std::vector<NodeType> solution1(N);
				std::vector<NodeType> solution2(N);
				solution1[0] = solutionToShowInGraphWindow[0];
				solution2[0] = solutionToCompareToInGraphWindow[0];
				for (int i = 1; i < N; ++i) {
					solution1[solutionToShowInGraphWindow[i - 1]] = solutionToShowInGraphWindow[i];
					solution2[solutionToCompareToInGraphWindow[i - 1]] = solutionToCompareToInGraphWindow[i];
				}
				std::vector<NodeType> revSolution1(N + 1);
				std::vector<NodeType> revSolution2(N + 1);
				for (int i = 0; i < N; ++i) {
					revSolution1[solution1[i]] = i;
					revSolution2[solution2[i]] = i;
				}
				revSolution1[0] = N;
				revSolution2[0] = N;

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
								hoveredConnection = { i, solution1[i] };
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
		ImGui::PopFont();
	});
	std::quick_exit(0);
}

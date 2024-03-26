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

int main(int argc, char** argv) {
	MyImGui::Init(u"Trackmania Path Finder");

	int ignoredValueInput = 600;
	int ignoredValue = ignoredValueInput * 10;
	int inputLimitValue = 100'000;
	int maxRepeatNodesToAdd = 100;
	int maxSolutionCountInput = 100;
	int maxTime = 10;
	bool allowRepeatNodes = false;
	constexpr int MinFontSize = 8;
	constexpr int MaxFontSize = 30;
	int fontSize = 15;
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

	MyImGui::Run([&] {
		ImGui::GetIO().FontGlobalScale = fontSize / 10.0f;

		constexpr int LongestLineLength = 29;
		constexpr int FontPixelSize = 7;
		float boxValuePosX = (LongestLineLength + 1) * FontPixelSize * ImGui::GetIO().FontGlobalScale;

		auto io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
		ImGui::Begin("Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		if (ImGui::GetIO().KeyCtrl) {
			if (ImGui::GetIO().MouseWheel > 0) {
				fontSize = std::clamp(fontSize + 1, MinFontSize, MaxFontSize);
			}
			if (ImGui::GetIO().MouseWheel < 0) {
				fontSize = std::clamp(fontSize - 1, MinFontSize, MaxFontSize);
			}
		}

		ImGui::Text("Font size:");
		ImGui::SameLine();
		HelpMarker("You can use CTRL + Mouse wheel to change font size");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputInt("##font size", &fontSize)) {
			fontSize = std::clamp(fontSize, MinFontSize, MaxFontSize);
		}

		ImGui::Dummy(ImVec2(0.0f, 20.0f));

		ImGui::Text("max connection time:");
		ImGui::SameLine();
		HelpMarker("Connections with this or higher time\nwill not be considered in the solutions");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputInt("##max node value threshold", &ignoredValueInput)) {
			ignoredValueInput = std::clamp(ignoredValueInput, 1, 100'000);
			ignoredValue = ignoredValueInput * 10;
		}
		ImGui::Text("max route time:");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputInt("##max solution length", &inputLimitValue)) {
			inputLimitValue = std::clamp(inputLimitValue, 1, 100'000);
		}
		ImGui::Text("max number of routes:");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputInt("##max number of routes", &maxSolutionCountInput)) {
			maxSolutionCountInput = std::clamp(maxSolutionCountInput, 1, 100'000);
		}
		ImGui::Text("max search time:");
		ImGui::SameLine();
		HelpMarker("Max time in seconds you want to search for.\n\nIt's mostly useful for heuristic algorithm since it will usually find most if not all top 100 solutions in the first ~10 seconds even for hard problems\n\nMight need to increase that time for some problems - you have to experiment yourself.");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputInt("##max search time", &maxTime)) {
			maxTime = std::clamp(maxTime, 1, 100'000);
		}
		ImGui::Text("output append data file:");
		ImGui::SameLine();
		HelpMarker("Every time candidate route is found it's saved to this file.\nThe data will be added to the end of the file\nwithout removing what was there before.\n\nYou have to sort that list yourself to find best routes.\n\nBy default it's empty so it's turned off");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##append data file", appendDataFile, sizeof(appendDataFile));
		
		ImGui::Text("output data file:");
		ImGui::SameLine();
		HelpMarker("After completing running the algorithm this file\nwill have sorted list of top \"max number of routes\" found.");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##output data file", outputDataFile, sizeof(outputDataFile));

		ImGui::Text("input data file:");
		ImGui::SameLine();
		HelpMarker("Format is full matrix of decimal values in CSV format\nusing any delimiter, e.g. comma, space, tab.\n\nFirst row are times to CP1.\nLast row are times to finish.\nFirst column are times from start.\nLast column are times from last CP.");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
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

		ImGui::Text("ring CPs:");
		ImGui::SameLine();
		HelpMarker("List of CP numbers that are rings.\nThat is CPs for which you want to include connection\nwhere you standing respawn after taking this CP\nto go back to previous CP\n\nWARNING: This has huge impact on algorithm performance");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputText("##ring cps", inputRingCps, sizeof(inputRingCps))) {
			auto nodes = splitLineOfFloatsToInts(inputRingCps, ignoredValue);
			ringCps.clear();
			for (auto node : nodes) {
				if (node != ignoredValue)
					ringCps.push_back(node);
			}
		}

		ImGui::Text("allow repeat CPs:");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		ImGui::Checkbox("##Allow repeat nodes", &allowRepeatNodes);
		if (allowRepeatNodes) {
			ImGui::Text("max connections to add:");
			ImGui::SameLine();
			ImGui::SetCursorPosX(boxValuePosX);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputInt("##max repeat nodes to add", &maxRepeatNodesToAdd)) {
				maxRepeatNodesToAdd = std::clamp(maxRepeatNodesToAdd, 1, 100'000);
			}

			ImGui::Text("turned off repeat CPs:");
			ImGui::SameLine();
			HelpMarker("List of CP numbers you want to ban from repeating");
			ImGui::SameLine();
			ImGui::SetCursorPosX(boxValuePosX);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##turned off repeat nodes", inputTurnedOffRepeatNodes, sizeof(inputTurnedOffRepeatNodes))) {
				auto nodes = splitLineOfFloatsToInts(inputTurnedOffRepeatNodes, ignoredValue);
				repeatNodesTurnedOff.clear();
				for (auto node : nodes) {
					if (node != ignoredValue)
						repeatNodesTurnedOff.push_back(node);
				}
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

				if (allowRepeatNodes) {
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
					writeSolutionFileProlog(appendDataFile, inputDataFile, config.limit, isExactAlgorithm, allowRepeatNodes, repeatNodesTurnedOff);
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
		}
		else if (isRunning(algorithmRunTask)) {
			status = "Running";
		} else {
			status = "Done";
		}
		ImGui::Text("Status: %s", status.c_str());
		ImGui::Text("Time elapsed: %.1f [s]", timer.getTime());
		if (isHeuristicAlgorithm) {
			auto n = config.partialSolutionCount.load();
			auto optVal = n >> 32;
			auto tryVal = n & 0xffffffff;
			ImGui::Text("Complated %4d tries for %d-opt", tryVal, optVal);
		} else {
			ImGui::Text("Partial routes processed: %d", config.partialSolutionCount.load());
		}
		ImGui::Text("Candidate routes found: %d", config.solutionsVec.size());

		if (config.solutionsVec.size() > bestFoundSolutions.size()) {
			for (int i = int(bestFoundSolutions.size()); i < config.solutionsVec.size(); ++i) {
				bestFoundSolutions.push_back(config.solutionsVec[i]);
			}
			std::sort(bestFoundSolutions.begin(), bestFoundSolutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
		}

		ImGuiListClipper clipper;
		clipper.Begin(std::min<int>(int(bestFoundSolutions.size()), config.maxSolutionCount));
		while (clipper.Step()) {
			for (int j = clipper.DisplayStart; j < clipper.DisplayEnd; ++j) {
				auto& [B_, time] = bestFoundSolutions[j];
				auto solStr = createSolutionString(B_, config.repeatNodeMatrix, config.useRespawnMatrix);
				ImGui::Text("%.1f", time / 10.0);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-1);
				ImGui::InputText(("##solution" + std::to_string(j)).c_str(), solStr.data(), solStr.size(), ImGuiInputTextFlags_ReadOnly);
			}
		}
		clipper.End();

		ImGui::End();
	});
	std::quick_exit(0);
}

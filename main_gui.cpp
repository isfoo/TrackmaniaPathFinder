#define NOMINMAX
#include <windows.h>
#include "solutionFinder.h"
#include "utility.h"
#include "common.h"
#include "fileLoadSave.h"
#include "imgui_directX11.h"
#include "lkh.h"

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
	if (argc >= 1 && !strcmp(argv[0], "LKH")) {
		lkhChildWorkerProcess(argc - 1, &argv[1]);
		return  0;
	}
	const char* programPath = argv[0];

	MyImGui::Init(u"Trackmania Path Finder");

	float ignoredValue = 600;
	float limitValue = 100'000;
	int maxSolutionCount = 20;
	int maxRepeatNodesToAdd = 100;
	int heuristicSearchDepth = 2;
	int foundRepeatNodesCount = -1;
	bool allowRepeatNodes = false;
	constexpr int MinFontSize = 8;
	constexpr int MaxFontSize = 30;
	int fontSize = 15;
	std::vector<std::vector<std::vector<int>>> repeatNodeMatrix;
	ThreadSafeVec<std::pair<std::vector<int16_t>, float>> solutionsView;
	std::vector<std::pair<std::vector<int16_t>, float>> bestFoundSolutions;
	std::atomic<int> partialSolutionCount = 0;

	char inputDataFile[1024] = { 0 };
	char appendDataFile[1024] = { 0 };
	strcpy(appendDataFile, "append_out.txt");
	char outputDataFile[1024] = { 0 };
	strcpy(outputDataFile, "out.txt");

	std::string errorMsg;
	std::future<void> algorithmRunTask;
	std::atomic<bool> taskWasCanceled = false;

	std::vector<int> repeatNodesTurnedOff;
	char inputTurnedOffRepeatNodes[1024] = { 0 };

	bool isHeuristicAlgorithm = false;

	auto timer = Timer();
	timer.stop();

	MyImGui::Run([&] {
		ImGui::GetIO().FontGlobalScale = fontSize / 10.0;

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

		ImGui::Text("max node value threshold:");
		ImGui::SameLine();
		HelpMarker("Node values with this or higher value\nwill not be considered in the solutions");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputFloat("##max node value threshold", &ignoredValue)) {
			ignoredValue = std::clamp(ignoredValue, 1.0f, 100'000.0f);
		}
		ImGui::Text("max solution time:");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputFloat("##max solution length", &limitValue)) {
			limitValue = std::clamp(limitValue, 1.0f, 100'000.0f);
		}
		ImGui::Text("max number of solutions:");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputInt("##max number of solutions", &maxSolutionCount)) {
			maxSolutionCount = std::clamp(maxSolutionCount, 1, 100'000);
		}
		ImGui::Text("heuristic search depth:");
		ImGui::SameLine();
		HelpMarker("Heuristic search first finds initial solution (likely optimal)\nThen it tries to remove each connection from that solution to see what it finds.\nThis process continues recursively and this value decides how deep it goes.\n\nfor 100 CP it means that:\ndepth = 0: 1 run\ndepth = 1: 100 runs\ndepth = 2: 10000 runs\ndepth = 3: 1000000 runs\n\nThe algorithm does early stopping depending on max solution time\nso if it's low then the number of actual runs will be much lower");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputInt("##heuristic search depth", &heuristicSearchDepth)) {
			heuristicSearchDepth = std::clamp(heuristicSearchDepth, 0, 5);
		}
		ImGui::Text("output append data file:");
		ImGui::SameLine();
		HelpMarker("Every time candidate solution is found it's saved to this file.\nThe data will be added to the end of the file\nwithout removing what was there before.\n\nYou have to sort that list yourself to find best solutions.");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		ImGui::InputText("##append data file", appendDataFile, sizeof(appendDataFile));
		
		ImGui::Text("output data file:");
		ImGui::SameLine();
		HelpMarker("Every time candidate solution is found it's saved to this file.\nThe data replaces whatever existed in that file beforehand.\n\nAt the end the file is again updated with sorted list of candidate solutions found.");
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
				foundRepeatNodesCount = -1;
			}
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1);
		if (ImGui::InputText("##input data file", inputDataFile, sizeof(inputDataFile))) {
			foundRepeatNodesCount = -1;
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
				auto nodes = splitToFloats(inputTurnedOffRepeatNodes, ignoredValue);
				repeatNodesTurnedOff.clear();
				for (auto node : nodes) {
					if (node != ignoredValue)
						repeatNodesTurnedOff.push_back(node);
				}
			}

			if (ImGui::Button("Count repeat connections")) {
				foundRepeatNodesCount = countRepeatNodeEdges(loadCsvData(inputDataFile, ignoredValue, errorMsg), ignoredValue, repeatNodesTurnedOff);
			}
			if (foundRepeatNodesCount != -1) {
				ImGui::SameLine();
				ImGui::SetCursorPosX(boxValuePosX);
				ImGui::Text("Found %d repeat repeat connections.", foundRepeatNodesCount);
			}
		} else {
			foundRepeatNodesCount = -1;
			maxRepeatNodesToAdd = 100;
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
				partialSolutionCount = 0;
				repeatNodeMatrix.clear();
				auto A = loadCsvData(inputDataFile, ignoredValue, errorMsg);
				if (allowRepeatNodes) {
					repeatNodeMatrix = addRepeatNodeEdges(A, ignoredValue, 1000000, repeatNodesTurnedOff);
				}
				if (errorMsg.empty()) {
					bestFoundSolutions.clear();
					solutionsView.clear();
					timer = Timer();
					clearFile(outputDataFile);
					writeSolutionFileProlog(appendDataFile, inputDataFile, limitValue, isExactAlgorithm, allowRepeatNodes, repeatNodesTurnedOff);
					algorithmRunTask = std::async(std::launch::async | std::launch::deferred, [isExactAlgorithm, &timer, &solutionsView, &partialSolutionCount, &taskWasCanceled, &repeatNodeMatrix, appendDataFile, outputDataFile, A, ignoredValue, limitValue, maxSolutionCount, programPath, heuristicSearchDepth]() mutable {
						if (isExactAlgorithm)
							runAlgorithm(A, maxSolutionCount, limitValue, ignoredValue, solutionsView, appendDataFile, outputDataFile, repeatNodeMatrix, partialSolutionCount, taskWasCanceled);
						else
							runAlgorithmHlk(A, programPath, heuristicSearchDepth, appendDataFile, outputDataFile, ignoredValue, limitValue, solutionsView, repeatNodeMatrix, partialSolutionCount, taskWasCanceled);
						writeSolutionFileEpilog(appendDataFile, taskWasCanceled);
						overwriteFileWithSortedSolutions(outputDataFile, solutionsView, repeatNodeMatrix);
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
		if (!algorithmRunTask.valid())
			status = "Waiting for start";
		else if (taskWasCanceled) {
			if (isRunning(algorithmRunTask))
				status = "Canceled (Still running)";
			else
				status = "Canceled";
		}
		else if (isRunning(algorithmRunTask))
			status = "Running";
		else
			status = "Done";
		ImGui::Text("Status: %s", status.c_str());
		ImGui::Text("Time elapsed: %.1f [s]", timer.getTime());
		if (isHeuristicAlgorithm)
			ImGui::Text("Starting points checked: %d", partialSolutionCount.load());
		else
			ImGui::Text("Partial solutions processed: %d", partialSolutionCount.load());
		ImGui::Text("Candidate solutions found: %d", solutionsView.size());

		if (solutionsView.size() > bestFoundSolutions.size()) {
			for (int i = bestFoundSolutions.size(); i < solutionsView.size(); ++i) {
				bestFoundSolutions.push_back(solutionsView[i]);
			}
			std::sort(bestFoundSolutions.begin(), bestFoundSolutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
		}
		for (int j = 0; j < bestFoundSolutions.size() && j < 100; ++j) {
			auto& [B_, time] = bestFoundSolutions[j];
			auto solStr = createSolutionString(B_, repeatNodeMatrix);
			ImGui::Text("%.2f", time);
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-1);
			ImGui::InputText(("##solution" + std::to_string(j)).c_str(), solStr.data(), solStr.size(), ImGuiInputTextFlags_ReadOnly);
		}

		ImGui::End();
	});
	std::quick_exit(0);
}

#define NOMINMAX
#include <windows.h>
#include "solutionFinder.h"
#include "utility.h"
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

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
int main() {
	MyImGui::Init(u"Trackmania Path Finder");

	float ignoredValue = 600;
	float limitValue = 100'000;
	int maxSolutionCount = 20;
	int maxRepeatNodesToAdd = 100;
	int foundRepeatNodesCount = -1;
	bool allowRepeatNodes = false;
	bool useLegacyOutputFormat = false;
	constexpr int MinFontSize = 8;
	constexpr int MaxFontSize = 30;
	int fontSize = 15;
	std::vector<std::vector<int>> repeatNodeMatrix;
	ThreadSafeVec<std::pair<std::vector<int16_t>, float>> solutionsView;
	std::vector<std::pair<std::vector<int16_t>, float>> bestFoundSolutions;

	char inputDataFile[1024] = { 0 };
	char outputDataFile[1024] = { 0 };
	strcpy(outputDataFile, "out.txt");

	std::string errorMsg;
	std::future<void> algorithmRunTask;
	std::atomic<bool> taskWasCanceled = false;

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
		ImGui::Text("output data file:");
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

		ImGui::Text("use legacy output format:");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		ImGui::Checkbox("##use legacy output format", &useLegacyOutputFormat);

		ImGui::Text("allow repeat nodes:");
		ImGui::SameLine();
		ImGui::SetCursorPosX(boxValuePosX);
		ImGui::SetNextItemWidth(-1);
		ImGui::Checkbox("##Allow repeat nodes", &allowRepeatNodes);
		if (allowRepeatNodes) {
			if (ImGui::Button("Count repeat nodes")) {
				foundRepeatNodesCount = getRepeatNodeEdges(loadCsvData(inputDataFile, ignoredValue, errorMsg), ignoredValue).size();
			}
			if (foundRepeatNodesCount != -1) {
				ImGui::SameLine();
				ImGui::SetCursorPosX(boxValuePosX);
				ImGui::Text("Found %d repeat nodes.", foundRepeatNodesCount);
			}

			ImGui::Text("max repeat nodes to add:");
			ImGui::SameLine();
			ImGui::SetCursorPosX(boxValuePosX);
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputInt("##max repeat nodes to add", &maxRepeatNodesToAdd)) {
				maxRepeatNodesToAdd = std::clamp(maxRepeatNodesToAdd, 1, 100'000);
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

		if (ImGui::Button("Run algorithm")) {
			if (!isRunning(algorithmRunTask)) {
				errorMsg = "";
				taskWasCanceled = false;
				repeatNodeMatrix.clear();
				auto A = loadCsvData(inputDataFile, ignoredValue, errorMsg);
				if (allowRepeatNodes) {
					repeatNodeMatrix = addRepeatNodeEdges(A, getRepeatNodeEdges(A, ignoredValue), maxRepeatNodesToAdd);
				}
				if (errorMsg.empty()) {
					bestFoundSolutions.clear();
					solutionsView = ThreadSafeVec<std::pair<std::vector<int16_t>, float>>{};
					timer = Timer();
					algorithmRunTask = std::async(std::launch::async | std::launch::deferred, [&timer, &solutionsView, &taskWasCanceled, &repeatNodeMatrix, useLegacyOutputFormat, outputDataFile, A, ignoredValue, limitValue, maxSolutionCount]() {
						auto sols = runAlgorithm(A, ignoredValue, limitValue, maxSolutionCount, solutionsView, taskWasCanceled);
						saveSolutionsToFile(outputDataFile, sols, repeatNodeMatrix, useLegacyOutputFormat);
						timer.stop();
					});
				}
			} else {
				errorMsg = "Already running";
			}
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
		ImGui::Text("Candidate solutions found: %d", solutionsView.size());
		ImGui::SameLine();
		HelpMarker("Number of solutions found while looking for the best ones.\n\nIf This exceeds \"max number of solutions\" it is meaningless\nand is shown just to give rough idea of how algorithm progresses");
		ImGui::Text("Time elapsed: %.1f", timer.getTime());

		if (solutionsView.size() > bestFoundSolutions.size()) {
			for (int i = bestFoundSolutions.size(); i < solutionsView.size(); ++i) {
				bestFoundSolutions.push_back(solutionsView[i]);
			}
			std::sort(bestFoundSolutions.begin(), bestFoundSolutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
		}
		for (int j = 0; j < bestFoundSolutions.size() && j < 100; ++j) {
			auto& [B_, time] = bestFoundSolutions[j];
			auto B = B_;
			if (!useLegacyOutputFormat) {
				B.insert(B.begin(), 0);
				for (auto& b : B)
					b += 1;
			}

			ImGui::Text("%.2f", time);
			ImGui::SameLine();
			std::string solStr = "[";
			if (!useLegacyOutputFormat) {
				solStr += "Start";
			} else {
				solStr += std::to_string(B[0] - 1);
			}
			for (int i = 1; i < B.size(); ++i) {
				if (useLegacyOutputFormat && !repeatNodeMatrix.empty() && repeatNodeMatrix[B[i]][B[i - 1]]) {
					solStr += ",(" + std::to_string(repeatNodeMatrix[B[i]][B[i - 1]] - 1) + "),";
				} else if (!useLegacyOutputFormat && i > 1 && !repeatNodeMatrix.empty() && repeatNodeMatrix[B_[i-1]][B_[i - 2]]) {
					solStr += ",(" + std::to_string(repeatNodeMatrix[B_[i - 1]][B_[i - 2]]) + "),";
				} else if (B[i] == B[i - 1] + 1) {
					solStr += '-';
					i += 1;
					while (i < B.size() && B[i] == B[i - 1] + 1) {
						i += 1;
					}
					i -= 1;
				} else {
					solStr += ',';
				}
				if (i == B.size() - 1 && !useLegacyOutputFormat)
					solStr += "Finish";
				else
					solStr += std::to_string(B[i] - 1);
			}
			solStr += "]";
			ImGui::SetNextItemWidth(-1);
			ImGui::InputText(("##solution" + std::to_string(j)).c_str(), solStr.data(), solStr.size(), ImGuiInputTextFlags_ReadOnly);
		}

		ImGui::End();
	});
	std::quick_exit(0);
}

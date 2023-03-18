#define NOMINMAX
#include <windows.h>
#include "solutionFinder.h"
#include "utility.h"
#include "fileLoadSave.h"
#include "imgui_directX11.h"
namespace fs = std::filesystem;

#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
int main() {
	MyImGui::Init(u"Trackmania Path Finder");

	float ignoredValue = 9000;
	float limitValue = 1850;
	int maxSolutionCount = 20;
	int outputStyle = 0;
	ThreadSafeVec<std::pair<std::vector<int16_t>, float>> solutionsView;
	std::vector<std::pair<std::vector<int16_t>, float>> bestFoundSolutions;

	char inputDataFile[1024] = { 0 };
	char outputDataFile[1024] = { 0 };
	strcpy(outputDataFile, "out.txt");

	std::string errorMsg;
	std::future<void> algorithmRunTask;

	auto timer = Timer();
	timer.stop();

	MyImGui::Run([&] {
		auto io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y));
		ImGui::Begin("Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

		ImGui::BeginColumns("", 2, ImGuiColumnsFlags_NoBorder);
		ImGui::SetColumnWidth(0, 250);
		ImGui::SetColumnWidth(1, 900);
		ImGui::Text("ignored node value:");
		ImGui::NextColumn();
		if (ImGui::InputFloat("##ignored node value", &ignoredValue)) {
			ignoredValue = std::clamp(ignoredValue, 1.0f, 100'000.0f);
		}
		ImGui::NextColumn();
		ImGui::Text("max solution time:");
		ImGui::NextColumn();
		if (ImGui::InputFloat("##max solution length", &limitValue)) {
			limitValue = std::clamp(limitValue, 1.0f, 100'000.0f);
		}
		ImGui::NextColumn();
		ImGui::Text("max number of solutions:");
		ImGui::NextColumn();
		if (ImGui::InputInt("##max number of solutions", &maxSolutionCount)) {
			maxSolutionCount = std::clamp(maxSolutionCount, 1, 100'000);
		}
		ImGui::NextColumn();
		ImGui::Text("output data file:");
		ImGui::NextColumn();
		ImGui::InputText("##output data file", outputDataFile, sizeof(outputDataFile));
		ImGui::NextColumn();
		ImGui::Text("input data file:");
		ImGui::NextColumn();
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
		ImGui::InputText("##input data file", inputDataFile, sizeof(inputDataFile));
		ImGui::EndColumns();

		ImGui::RadioButton("use output style 1, e.g. [0,2,3,4,5,1,7,8]", &outputStyle, 0);
		ImGui::RadioButton("use output style 2, e.g. [0,2-5,1,7-8]", &outputStyle, 1);

		if (ImGui::Button("Run algorithm")) {
			if (!isRunning(algorithmRunTask)) {
				errorMsg = "";
				auto A = loadCsvData(inputDataFile, ignoredValue, errorMsg);
				if (errorMsg.empty()) {
					bestFoundSolutions.clear();
					solutionsView = ThreadSafeVec<std::pair<std::vector<int16_t>, float>>{};
					timer = Timer();
					algorithmRunTask = std::async(std::launch::async | std::launch::deferred, [&timer, &solutionsView, outputDataFile, A, ignoredValue, limitValue, maxSolutionCount, outputStyle]() {
						auto sols = runAlgorithm(A, ignoredValue, limitValue, maxSolutionCount, solutionsView);
						saveSolutionsToFile(outputDataFile, sols, outputStyle == 0);
						timer.stop();
					});
				}
			} else {
				errorMsg = "Already running";
			}
		}
		std::string status;
		if (!algorithmRunTask.valid())
			status = "Waiting for start";
		else if (isRunning(algorithmRunTask))
			status = "Running";
		else
			status = "Done";
		ImGui::Text("Status: %s", status.c_str());
		ImGui::Text("Candidate solutions found: %d", solutionsView.size());
		ImGui::Text("Time elapsed: %.1f", timer.getTime());

		if (solutionsView.size() > bestFoundSolutions.size()) {
			for (int i = bestFoundSolutions.size(); i < solutionsView.size(); ++i) {
				bestFoundSolutions.push_back(solutionsView[i]);
			}
			std::sort(bestFoundSolutions.begin(), bestFoundSolutions.end(), [](auto& a, auto& b) { return a.second < b.second; });
		}
		for (int j = 0; j < bestFoundSolutions.size() && j < 100; ++j) {
			auto& [B, time] = bestFoundSolutions[j];
			ImGui::Text("%.2f", time);
			ImGui::SameLine();
			std::string solStr = "[";
			solStr += std::to_string(B[0] - 1);
			for (int i = 1; i < B.size(); ++i) {
				if (B[i] == B[i - 1] + 1) {
					solStr += '-';
					i += 1;
					while (i < B.size() && B[i] == B[i - 1] + 1) {
						i += 1;
					}
					i -= 1;
				} else {
					solStr += ',';
				}
				solStr += std::to_string(B[i] - 1);
			}
			solStr += "]";
			ImGui::Text("%s", solStr.c_str());
		}

		if (!errorMsg.empty())
			ImGui::Text("Error: %s", errorMsg.c_str());

		ImGui::End();
	});
}

#include "solutionFinder.h"
#include "fileLoadSave.h"
#include "utility.h"

#include <filesystem>
namespace fs = std::filesystem;

int main(int argc, char** argv) {
	std::cout.sync_with_stdio(false);

	if (argc < 2) {
		std::cout << "ERROR: You have to provide data file as first command line argument, for example:\n";
		std::cout << '\t' << fs::path(argv[0]).filename().string() << " data.csv\n";
		std::cout << '\n';
		std::cout << "You can add 2nd argument specifying ignored node value:\n";
		std::cout << '\t' << fs::path(argv[0]).filename().string() << " data.csv 50000\n";
		std::cout << "By default that value is set to 9000\n";
		std::cout << '\n';
		std::cout << "You can add 3rd argument specifying max solution time:\n";
		std::cout << '\t' << fs::path(argv[0]).filename().string() << " data.csv 50000 1900\n";
		std::cout << "By default there is no limit\n";
		std::cout << '\n';
		std::cout << "You can add 4th argument specifying max number of solutions (top solutions):\n";
		std::cout << '\t' << fs::path(argv[0]).filename().string() << " data.csv 50000 1900 10\n";
		std::cout << "By default there is no maximum\n";
		std::cin.get();
		return 1;
	}

	std::ifstream inFile(argv[1]);
	if (!inFile) {
		std::cout << "ERROR: couldn't open input file \"" << argv[1] << "\"\n";
		return 1;
	}

	float ignoredValue = 9000;
	if (argc >= 3) {
		auto ignoredValueOpt = parseFloat(argv[2]);
		if (!ignoredValueOpt) {
			std::cout << "ERROR: 2nd argument \"" << argv[2] << "\" is not a number, but expected ignored node value\n";
			return 1;
		}
		ignoredValue = *ignoredValueOpt;
	}

	float limit = MaxLimit;
	if (argc >= 4) {
		auto limitValueOpt = parseFloat(argv[3]);
		if (!limitValueOpt) {
			std::cout << "ERROR: 3rd argument \"" << argv[3] << "\" is not a number, but expected limit value\n";
			return 1;
		}
		limit = *limitValueOpt;
	}

	float maxSolutionCount = MaxMaxSolutionCount;
	if (argc >= 5) {
		auto limitValueOpt = parseFloat(argv[4]);
		if (!limitValueOpt) {
			std::cout << "ERROR: 4th argument \"" << argv[4] << "\" is not a number, but expected max number of solutions\n";
			return 1;
		}
		maxSolutionCount = *limitValueOpt;
	}

	std::string errorMsg;
	auto A = loadCsvData(argv[1], ignoredValue, errorMsg);
	if (!errorMsg.empty()) {
		std::cout << "Error: " << errorMsg << '\n';
		return 1;
	}

	auto timer = Timer();
	auto solutions = runAlgorithm(A, ignoredValue, limit, maxSolutionCount);
	saveSolutionsToFile("out.txt", solutions, argc < 6);
	std::cout << "time: " << timer.getTime() << '\n';
}

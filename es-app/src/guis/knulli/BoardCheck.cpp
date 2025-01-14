#include "guis/knulli/BoardCheck.h"
#include "utils/Platform.h"
#include "Paths.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"
#include <stdio.h>
#include <sys/wait.h>
#include <fstream>
#include <vector>
#include <string>

const std::string BOARD_FILE = "/boot/boot/batocera.board";

std::string BoardCheck::getBoard()
{
	if (Utils::FileSystem::exists(BOARD_FILE)) {
		std::ifstream file(BOARD_FILE);
		std::string board;
		std::getline(file, board);
		return board;
	}
	return "";
}
bool BoardCheck::isBoard(const std::vector<std::string>& boards)
{
	std::string board = BoardCheck::getBoard();
	for (const auto& b : boards) {
		if (board == b) {
			return true;
		}
	}
    return false;
}
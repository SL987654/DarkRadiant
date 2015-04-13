#include "GameFileLoader.h"

#include <boost/algorithm/string/case_conv.hpp>

namespace game
{

// Constructor
GameFileLoader::GameFileLoader(Manager::GameMap& games, const std::string& path) :
	_games(games),
	_path(path)
{}

// Main functor () function, gets called with the file (without path)
void GameFileLoader::operator() (const boost::filesystem::path& file)
{
	if (boost::algorithm::to_lower_copy(file.extension().string()) != GAME_FILE_EXT)
	{
		// Don't process files not ending with .game
		return;
	}

	// Create a new Game object
	GamePtr newGame(new Game(_path, file.filename().string()));
	std::string gameName = newGame->getName();

	if (!gameName.empty())
	{
		// Store the game into the map
		_games[gameName] = newGame;
	}
}

} // namespace game

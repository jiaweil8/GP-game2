#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>


struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----
	enum class GameState {
		Flash,
		Dark,
		Win,
		Lose
	};
	GameState game_state = GameState::Flash;
	uint32_t step_count = 0;
	float flash_timer = 3.0f;
	//flash_count is used to track how many times the player has entered the Flash state: the more times the player has entered the Flash state, the dimmer the flashlight will be in the Dark state:
	uint32_t flash_count = 0;

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;
	Scene::Transform *player = nullptr;
	//use cell coordinates to track player position in the maze:
	enum class Cell {
		Empty,
		Wall,
		Trap,
		Goal
	};
	glm::ivec2 player_cell = glm::ivec2(0, 0);
	glm::vec3 player_start_position;
	Cell get_cell(glm::ivec2 cell);
	glm::ivec2 player_map_pos = glm::ivec2(-1, -1);


	
	//camera:
	Scene::Camera *camera = nullptr;
	glm::vec3 camera_start_position;

};

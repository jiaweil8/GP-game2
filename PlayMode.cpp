#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <random>
#include <iostream>

constexpr float CELL_SIZE = 2.0f;
//I use unusual marks to help me understand the maze layout:
static std::vector<std::string> const LEVEL = {
	"###############",
	"#.............#",
	"#.#####.#####.#",
	"#.....#.....#G#",
	"#####.#.###.#.#",
	"#.....#..X#...#",
	"#.####...X###.#",
	"#....X.##.....#",
	"###.X..##.XXX.#",
	"#...X.....#...#",
	"#.#####.#.#X#.#",
	"#.......#...#.#",
	"#.#######.X...#",
	"#P............#",
	"###############"
};

GLuint flashlight_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > flashlight_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("FlashLight.pnct"));
	flashlight_meshes_for_lit_color_texture_program =
		ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > flashlight_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(
		data_path("FlashLight.scene"),
		[&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name) {
			Mesh const &mesh = flashlight_meshes->lookup(mesh_name);
			scene.drawables.emplace_back(transform);
			Scene::Drawable &drawable = scene.drawables.back();
			drawable.pipeline = lit_color_texture_program_pipeline;
			drawable.pipeline.vao =
				flashlight_meshes_for_lit_color_texture_program;
			drawable.pipeline.type = mesh.type;
			drawable.pipeline.start = mesh.start;
			drawable.pipeline.count = mesh.count;
		}
	);
});

PlayMode::PlayMode() : scene(*flashlight_scene) {
	
	//find player transform:
	Scene::Transform *wall_template = nullptr;
	Scene::Transform *trap_template = nullptr;
	Scene::Transform *goal_template = nullptr;
	for (auto &transform : scene.transforms) {
		if (transform.name == "Player") {
			player = &transform;
		}
		else if (transform.name == "Wall_00") {
			wall_template = &transform;
		}
		else if (transform.name == "Trap_00") {
		trap_template = &transform;
		}
		else if (transform.name == "Goal") {
			goal_template = &transform;
		}
	}
	if (player == nullptr) {
		throw std::runtime_error("Player not found.");
	}
	if (wall_template == nullptr) {
		throw std::runtime_error("Wall_00 not found.");
	}
	if (trap_template == nullptr) {
		throw std::runtime_error("Trap_00 not found.");
	}
	if (goal_template == nullptr) {
		throw std::runtime_error("Goal not found.");
	}
	player_start_position = player->position;

	//find wall, trap and goal drawable templates, I need this to make copies of them for the maze:
	Scene::Drawable *wall_drawable_template = nullptr;
	Scene::Drawable *trap_drawable_template = nullptr;
	Scene::Drawable *goal_drawable_template = nullptr;
	for (auto &drawable : scene.drawables) {
		if (drawable.transform == wall_template) {
			wall_drawable_template = &drawable;
			break;
		}
	}
	if (wall_drawable_template == nullptr) {
		throw std::runtime_error("Wall_00 drawable not found.");
	}
	for (auto &drawable : scene.drawables) {
		if (drawable.transform == trap_template) {
			trap_drawable_template = &drawable;
			break;
		}
	}
	if (trap_drawable_template == nullptr) {
		throw std::runtime_error("Trap_00 drawable not found.");
	}
	for (auto &drawable : scene.drawables) {
		if (drawable.transform == goal_template) {
			goal_drawable_template = &drawable;
			break;
		}
	}

	if (goal_drawable_template == nullptr) {
		throw std::runtime_error("Goal drawable not found.");
	}

	//find player start position in the maze:
	for (int row = 0; row < static_cast<int>(LEVEL.size()); ++row) {
		for (int col = 0; col < static_cast<int>(LEVEL[row].size()); ++col) {
			if (LEVEL[row][col] == 'P') {
				player_map_pos = glm::ivec2(col, row);
			}
		}
	}
	if (player_map_pos.x < 0) {
		throw std::runtime_error("P not found in LEVEL.");
	}

	//make walls for the maze:
	for (int row = 0; row < static_cast<int>(LEVEL.size()); ++row) {
		for (int col = 0; col < static_cast<int>(LEVEL[row].size()); ++col) {
			char tile = LEVEL[row][col];
			glm::ivec2 cell(col - player_map_pos.x, player_map_pos.y - row);
			if (tile == '#') {
				scene.transforms.emplace_back();
				Scene::Transform *wall = &scene.transforms.back();
				wall->name = "RuntimeWall";
				wall->position =player_start_position + glm::vec3(cell.x * CELL_SIZE, cell.y * CELL_SIZE, 0.0f);
				wall->rotation = wall_template->rotation;
				wall->scale = wall_template->scale;
				wall->parent = wall_template->parent;
				scene.drawables.emplace_back(wall);
				scene.drawables.back().pipeline = wall_drawable_template->pipeline;
			}
			else if (tile == 'X') {
				scene.transforms.emplace_back();
				Scene::Transform *trap = &scene.transforms.back();
				trap->name = "RuntimeTrap";
				trap->position = player_start_position + glm::vec3(cell.x * CELL_SIZE, cell.y * CELL_SIZE, -1.5f);
				trap->rotation = trap_template->rotation;
				trap->scale = trap_template->scale;
				trap->parent = trap_template->parent;
				scene.drawables.emplace_back(trap);
				scene.drawables.back().pipeline = trap_drawable_template->pipeline;
			}
			else if (tile == 'G') {
				scene.transforms.emplace_back();
				Scene::Transform *goal = &scene.transforms.back();
				goal->name = "RuntimeGoal";
				goal->position = player_start_position + glm::vec3(cell.x * CELL_SIZE, cell.y * CELL_SIZE, 0.0f);
				goal->rotation = goal_template->rotation;
				goal->scale = goal_template->scale;
				goal->parent = goal_template->parent;
				scene.drawables.emplace_back(goal);
				scene.drawables.back().pipeline =
					goal_drawable_template->pipeline;
			}
		}
	}

	//disable the wall template, since I don't want to draw it:
	wall_drawable_template->pipeline.program = 0;
	trap_drawable_template->pipeline.program = 0;
	goal_drawable_template->pipeline.program = 0;

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
	camera_start_position = camera->transform->position;
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_ESCAPE) {
			SDL_SetWindowRelativeMouseMode(Mode::window, false);
			return true;
		} else if (evt.key.key == SDLK_A) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			down.pressed = false;
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == false) {
			SDL_SetWindowRelativeMouseMode(Mode::window, true);
			return true;
		}
	} else if (evt.type == SDL_EVENT_MOUSE_MOTION) {
		if (SDL_GetWindowRelativeMouseMode(Mode::window) == true) {
			glm::vec2 motion = glm::vec2(
				evt.motion.xrel / float(window_size.y),
				-evt.motion.yrel / float(window_size.y)
			);
			camera->transform->rotation = glm::normalize(
				camera->transform->rotation
				* glm::angleAxis(-motion.x * camera->fovy, glm::vec3(0.0f, 1.0f, 0.0f))
				* glm::angleAxis(motion.y * camera->fovy, glm::vec3(1.0f, 0.0f, 0.0f))
			);
			return true;
		}
	}
	if (evt.type == SDL_EVENT_KEY_DOWN && evt.key.key == SDLK_R) {
		Mode::set_current(std::make_shared<PlayMode>());
		return true;
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//light flash timer counts down in the Flash state, then switches to Dark state:
	if (game_state == GameState::Flash) {
		up.pressed = false;
		down.pressed = false;
		left.pressed = false;
		right.pressed = false;
		flash_timer -= elapsed;
		if (flash_timer <= 0.0f) {
			game_state = GameState::Dark;
			step_count = 0;
			flash_count++;
			std::cout << "DARK!\n";
		}
	}
	//moving the player in the maze is only allowed in the Dark state:
	else if (game_state == GameState::Dark) {
		//move player in the maze:
		glm::ivec2 move_direction(0, 0);
		if (up.pressed) {
			move_direction = glm::ivec2(0, 1);
			up.pressed = false;
		}
		if (down.pressed) {
			move_direction = glm::ivec2(0, -1);
			down.pressed = false;
		}
		if (left.pressed) {
			move_direction = glm::ivec2(-1, 0);
			left.pressed = false;
		}
		if (right.pressed) {
			move_direction = glm::ivec2(1, 0);
			right.pressed = false;
		}
		if (move_direction != glm::ivec2(0, 0)) {
			glm::ivec2 next_cell = player_cell + move_direction;
			Cell next = get_cell(next_cell);
			if (next != Cell::Wall) {
				player_cell = next_cell;
				step_count++;
				if (next == Cell::Trap) {
					game_state = GameState::Lose;
					std::cout << "LOSE!\n";
				}
				else if (next == Cell::Goal) {
					game_state = GameState::Win;
					std::cout << "WIN!\n";
				}
				else if (step_count >= 5) {
					game_state = GameState::Flash;
					std::cout << "FLASH!\n";
					flash_timer = 3.0f;
				}
			}
		}
		player->position =player_start_position+ glm::vec3(player_cell.x * CELL_SIZE, player_cell.y * CELL_SIZE, 0.0f);
		//keep camera position relative to player position:
		camera->transform->position = camera_start_position+ glm::vec3(player_cell.x * CELL_SIZE, player_cell.y * CELL_SIZE, 0.0f);
	}
	else if (game_state == GameState::Win || game_state == GameState::Lose) {
		up.pressed = false;
		down.pressed = false;
		left.pressed = false;
		right.pressed = false;
	}
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	//flashlight energy is bright in Flash state, dim in Dark state, and bright again in Win/Lose states:
	glm::vec3 light_energy;
	if (game_state == GameState::Flash) {
		light_energy = glm::vec3(1.0f, 1.0f, 0.95f);
	}
	else if (game_state == GameState::Dark) {
		float dark_energy = 0.0f;
		if (flash_count == 1) {
			dark_energy = 0.12f;
		}
		else if (flash_count == 2) {
			dark_energy = 0.06f;
		}
		else if (flash_count == 3) {
			dark_energy = 0.02f;
		}
		else {
			dark_energy = 0.0f;
		}
		light_energy = glm::vec3(dark_energy);
	}
	else {
		light_energy = glm::vec3(1.0f, 1.0f, 0.95f);
	}
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(light_energy));
	glUseProgram(0);

	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		std::string state_text;
		if (game_state == GameState::Win) {
			state_text = "You WIN!";
		}
		else if (game_state == GameState::Lose) {
			state_text = "You LOSE! Press R to restart.";
		}
		else {
			state_text = "WASD moves; Every 5 steps,   flashlight will flash for 3 seconds.";
		}
		constexpr float H = 0.09f;
		lines.draw_text(state_text,
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text(state_text,
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0xff, 0xff, 0xff, 0x00));
	}
}

PlayMode::Cell PlayMode::get_cell(glm::ivec2 cell) {

	int col = player_map_pos.x + cell.x;
	int row = player_map_pos.y - cell.y;

	if (row < 0 || row >= static_cast<int>(LEVEL.size())) {
		return Cell::Wall;
	}

	if (col < 0 || col >= static_cast<int>(LEVEL[row].size())) {
		return Cell::Wall;
	}

	char tile = LEVEL[row][col];

	if (tile == '#') {
		return Cell::Wall;
	}
	else if (tile == 'X') {
		return Cell::Trap;
	}
	else if (tile == 'G') {
		return Cell::Goal;
	}

	return Cell::Empty;
}

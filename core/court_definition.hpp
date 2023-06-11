#pragma once

namespace svb {

constexpr int max_players = 4;

constexpr int canvas_width = 1024;
constexpr int canvas_height = 576;
constexpr int court_center_x = 512;
constexpr int court_center_y = 288;
constexpr int court_padding_x = 100;
constexpr int court_padding_y = 50;
constexpr int court_line_width = 10;

constexpr int court_width = (canvas_width - (court_padding_x * 2)) / 2;
constexpr int court_height = (canvas_height - (court_padding_y * 2)) / 2;

constexpr float player_speed = 300.0;
constexpr float ball_speed = 400.0;
constexpr float player_base_radius = 24.0;
constexpr float ball_base_radius = 24.0;

} // namespace svb

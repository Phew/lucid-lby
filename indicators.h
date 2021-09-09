#pragma once

class hud_indicators {
public:
	void DrawFrame(const vec2_t& pos, const std::string& name, const std::vector<std::string>& first_column, const std::vector<std::string>& second_column);
	void Indicators();
	void StateIndicators();
	int	m_width, m_height;
};

extern hud_indicators g_indicators;
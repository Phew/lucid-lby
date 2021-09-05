#include "includes.h"

void Form::draw() {
	// opacity should reach 1 in 500 milliseconds.
	constexpr float frequency = 1.f / 0.5f;

	// the increment / decrement per frame.
	float step = frequency * g_csgo.m_globals->m_frametime;

	// if open		-> increment
	// if closed	-> decrement
	m_open ? m_opacity += step : m_opacity -= step;

	// clamp the opacity.
	math::clamp(m_opacity, 0.f, 1.f);

	m_alpha = 255;
	if (!m_open)
		return;

	// get gui color.
	Color color = g_gui.m_color;
	color.a() = m_alpha;

	// background.
	render::rect_filled(m_x, m_y, m_width, m_height, { 26, 26, 26, 255 });

	// border.
	render::rect(m_x, m_y, m_width, m_height, { 15, 15, 15, 255 });
	render::rect(m_x + 1, m_y + 1, m_width - 2, m_height - 2, { 15, 15, 15, 255 });
	//render::rect(m_x + 2, m_y + 2, m_width - 4, m_height - 4, { 40, 40, 40, 245 });
	//render::rect(m_x + 3, m_y + 3, m_width - 6, m_height - 6, { 40, 40, 40, 245 });
	//render::rect(m_x + 4, m_y + 4, m_width - 8, m_height - 8, { 40, 40, 40, 245 });
	//render::rect(m_x + 5, m_y + 5, m_width - 10, m_height - 10, { 60, 60, 60, 245 });

	// draw tabs if we have any.
	if (!m_tabs.empty()) {
		// tabs background and border.
		Rect tabs_area = GetTabsRect();

		render::rect_filled(tabs_area.x, tabs_area.y, tabs_area.w, tabs_area.h, { 26, 26, 26, 180 });
		//render::rect( tabs_area.x, tabs_area.y, tabs_area.w, tabs_area.h, { 0, 0, 0, m_alpha } );
		render::rect(tabs_area.x + 2, tabs_area.y + 2, tabs_area.w - 4, tabs_area.h - 25, { 26, 26, 26, 180 }); // x = pos / y = idfk / w 

		for (size_t i{}; i < m_tabs.size(); ++i) {
			const auto& t = m_tabs[i];
			int font_width = render::menu_shade.size(t->m_title).m_width;

			// text
			render::menu_shade.string(tabs_area.x + (i * (tabs_area.w / m_tabs.size())) + 16, tabs_area.y,
				Color{ 255, 255, 255, 180 }, t->m_title);

			// active tab indicator
	/*		render::rect_filled_fade(tabs_area.x + (i * (tabs_area.w / m_tabs.size())) + 10, tabs_area.y + 14,
				font_width + 11, 2,
				t == m_active_tab ? Color{ 0, 0, 0, 0 } : Color{ 0, 0, 0, m_alpha }, 0, 150);*/

			render::rect_filled(tabs_area.x + (i * (tabs_area.w / m_tabs.size())) + 10, tabs_area.y + 14,
				font_width + 11, 2,
				t == m_active_tab ? color : Color{ 255, 255, 255, 180 });
		}

		// this tab has elements.
		if (!m_active_tab->m_elements.empty()) {
			// elements background and border.
			Rect el = GetElementsRect();

			render::rect_filled(el.x, el.y, el.w, el.h, { 26, 26, 26, 180 });
			render::rect(el.x, el.y, el.w, el.h, { 25, 25, 25, 255 });
			render::rect(el.x + 1, el.y + 1, el.w - 2, el.h - 2, { 25, 25, 25, 255 });

			// iterate elements to display.
			for (const auto& e : m_active_tab->m_elements) {

				// draw the active element last.
				if (!e || (m_active_element && e == m_active_element))
					continue;

				if (!e->m_show)
					continue;

				// this element we dont draw.
				if (!(e->m_flags & ElementFlags::DRAW))
					continue;

				e->draw();
			}

			// we still have to draw one last fucker.
			if (m_active_element && m_active_element->m_show)
				m_active_element->draw();
		}
	}
}
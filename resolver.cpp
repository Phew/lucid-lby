#include "includes.h"
Resolver g_resolver{};;

LagRecord* Resolver::FindIdealRecord(AimPlayer* data) {
	LagRecord* first_valid, *current;

	if (data->m_records.empty())
		return nullptr;

	first_valid = nullptr;

	// iterate records.
	for (const auto& it : data->m_records) {
		if (it->dormant() || it->immune() || !it->valid())
			continue;

		// get current record.
		current = it.get();

		// first record that was valid, store it for later.
		if (!first_valid)
			first_valid = current;

		// try to find a record with a shot, lby update, walking or no anti-aim.
		if (it->m_shot || it->m_mode == Modes::RESOLVE_BODY || it->m_mode == Modes::RESOLVE_WALK || it->m_mode == Modes::RESOLVE_NONE)
			return current;
	}

	// none found above, return the first valid record if possible.
	return (first_valid) ? first_valid : nullptr;
}

LagRecord* Resolver::FindLastRecord(AimPlayer* data) {
	LagRecord* current;

	if (data->m_records.empty())
		return nullptr;

	// iterate records in reverse.
	for (auto it = data->m_records.crbegin(); it != data->m_records.crend(); ++it) {
		current = it->get();

		// if this record is valid.
		// we are done since we iterated in reverse.
		if (current->valid() && !current->immune() && !current->dormant())
			return current;
	}

	return nullptr;
}

void Resolver::OnBodyUpdate(Player* player, float value) {
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	// set data.
	data->m_old_body = data->m_body;
	data->m_body = value;
}

float Resolver::GetAwayAngle(LagRecord* record) {
	float  delta{ std::numeric_limits< float >::max() };
	vec3_t pos;
	ang_t  away;

	math::VectorAngles(g_cl.m_local->m_vecOrigin() - record->m_pred_origin, away);
	return away.y;

}

void Resolver::MatchShot(AimPlayer* data, LagRecord* record) {
	// do not attempt to do this in nospread mode.
	if (g_menu.main.config.mode.get() == 1)
		return;

	float shoot_time = -1.f;

	Weapon* weapon = data->m_player->GetActiveWeapon();
	if (weapon) {
		// with logging this time was always one tick behind.
		// so add one tick to the last shoot time.
		shoot_time = weapon->m_fLastShotTime() + g_csgo.m_globals->m_interval;
	}

	// this record has a shot on it.
	if (game::TIME_TO_TICKS(shoot_time) == game::TIME_TO_TICKS(record->m_sim_time)) {
		if (record->m_lag <= 2)
			record->m_shot = true;

		// more then 1 choke, cant hit pitch, apply prev pitch.
		else if (data->m_records.size() >= 2) {
			LagRecord* previous = data->m_records[1].get();

			if (previous && !previous->dormant())
				record->m_eye_angles.x = previous->m_eye_angles.x;
		}
	}
}

void Resolver::SetMode(LagRecord* record) {
	// the resolver has 3 modes to chose from.
	// these modes will vary more under the hood depending on what data we have about the player
	// and what kind of hack vs. hack we are playing (mm/nospread).

	float speed = record->m_velocity.length_2d();

	//move
	if ((record->m_flags & FL_ONGROUND) && speed > 5.f && !record->m_fake_walk)
		record->m_mode = Modes::RESOLVE_WALK;

	//override

	//stand
	else if ((record->m_flags & FL_ONGROUND) && (speed <= 10.f || record->m_fake_walk) && !g_input.GetKeyState(g_menu.main.aimbot.override.get()))
		record->m_mode = Modes::RESOLVE_STAND;

	//in air
	else if (!(record->m_flags & FL_ONGROUND))
		record->m_mode = Modes::RESOLVE_AIR;
}

void Resolver::ResolveAngles(Player* player, LagRecord* record) {
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	// mark this record if it contains a shot.
	MatchShot(data, record);

	// next up mark this record with a resolver mode that will be used.
	SetMode(record);

	if (g_menu.main.config.mode.get() == 1)
		record->m_eye_angles.x = 90.f;

	// reset bruteforced angels if enemy is dormant / dead.

	ResetBruteforce(data);

	// we arrived here we can do the acutal resolve.
	if (record->m_mode == Modes::RESOLVE_WALK)
		ResolveWalk(data, record);


	else if (record->m_mode == Modes::RESOLVE_STAND) {
		ResolveStand(record, data, player);

	}

	else if (record->m_mode == Modes::RESOLVE_AIR)
		ResolveAir(data, record, player);

	// normalize the eye angles, doesn't really matter but its clean.
	math::NormalizeAngle(record->m_eye_angles.y);
}

void Resolver::AntiFreestand(LagRecord* record) {
	// constants
	constexpr float STEP{ 4.f };
	constexpr float RANGE{ 32.f };

	// best target.
	vec3_t enemypos = record->m_player->GetShootPosition();
	float away = GetAwayAngle(record);

	// construct vector of angles to test.
	std::vector< AdaptiveAngle > angles{ };
	angles.emplace_back(away - 180.f);
	angles.emplace_back(away + 90.f);
	angles.emplace_back(away - 90.f);

	// start the trace at the your shoot pos.
	vec3_t start = g_cl.m_local->GetShootPosition();

	// see if we got any valid result.
	// if this is false the path was not obstructed with anything.
	bool valid{ false };

	// iterate vector of angles.
	for (auto it = angles.begin(); it != angles.end(); ++it) {

		// compute the 'rough' estimation of where our head will be.
		vec3_t end{ enemypos.x + std::cos(math::deg_to_rad(it->m_yaw)) * RANGE,
			enemypos.y + std::sin(math::deg_to_rad(it->m_yaw)) * RANGE,
			enemypos.z };

		// draw a line for debugging purposes.
		// g_csgo.m_debug_overlay->AddLineOverlay( start, end, 255, 0, 0, true, 0.1f );

		// compute the direction.
		vec3_t dir = end - start;
		float len = dir.normalize();

		// should never happen.
		if (len <= 0.f)
			continue;

		// step thru the total distance, 4 units per step.
		for (float i{ 0.f }; i < len; i += STEP) {
			// get the current step position.
			vec3_t point = start + (dir * i);

			// get the contents at this point.
			int contents = g_csgo.m_engine_trace->GetPointContents(point, MASK_SHOT_HULL);

			// contains nothing that can stop a bullet.
			if (!(contents & MASK_SHOT_HULL))
				continue;

			float mult = 1.f;

			// over 50% of the total length, prioritize this shit.
			if (i > (len * 0.5f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.75f))
				mult = 1.25f;

			// over 90% of the total length, prioritize this shit.
			if (i > (len * 0.9f))
				mult = 2.f;

			// append 'penetrated distance'.
			it->m_dist += (STEP * mult);

			// mark that we found anything.
			valid = true;
		}
	}

	if (!valid) {
		return;
	}

	// put the most distance at the front of the container.
	std::sort(angles.begin(), angles.end(),
		[](const AdaptiveAngle& a, const AdaptiveAngle& b) {
		return a.m_dist > b.m_dist;
	});

	// the best angle should be at the front now.
	AdaptiveAngle* best = &angles.front();

	record->m_eye_angles.y = best->m_yaw;
}


void Resolver::ResetBruteforce(AimPlayer* data) {
	if (!data->m_player->alive() || data->m_player->dormant()) {
		data->m_body_index = 0;
		data->m_last_move = 0;
		data->m_unknown_move = 0;
		data->m_stand_index = 0;
		data->m_air_index = 0;
		data->m_move_index = 0;
	}
}
void Resolver::ResolveYawBruteforce(LagRecord* record, AimPlayer* data)
{
	auto local_player = g_cl.m_local;
	if (!local_player)
		return;

	record->m_mode = Modes::RESOLVE_STAND;

	const float at_target_yaw = GetAwayAngle(record);

	switch (data->m_stand_index % 4)
	{
	case 0:
		g_resolver.iPlayers[record->m_player->index()] = true;
		record->m_eye_angles.y += 180.f;
		break;
	case 1:
		g_resolver.iPlayers[record->m_player->index()] = true;
		record->m_eye_angles.y += 90.f;
		break;
	case 2:
		g_resolver.iPlayers[record->m_player->index()] = true;
		record->m_eye_angles.y -= 180.f;
		break;
	case 3:
		g_resolver.iPlayers[record->m_player->index()] = true;
		AntiFreestand(record);		break;
	}

	switch (data->m_unknown_move % 4)
	{
	case 0:
		g_resolver.iPlayers[record->m_player->index()] = true;
		record->m_eye_angles.y += 180.f;
		break;
	case 1:
		g_resolver.iPlayers[record->m_player->index()] = true;
		record->m_eye_angles.y += 90.f;
		break;
	case 2:
		g_resolver.iPlayers[record->m_player->index()] = true;
		record->m_eye_angles.y -= 180.f;
		break;
	case 3:
		g_resolver.iPlayers[record->m_player->index()] = true;
		AntiFreestand(record);
		break;
	}
	float velyaw = math::rad_to_deg(std::atan2(record->m_velocity.y, record->m_velocity.x));
	switch (data->m_air_index % 4) {
	case 0:
		g_resolver.iPlayers[record->m_player->index()] = true;
		record->m_eye_angles.y += 180.f;
		break;
	case 1:
		g_resolver.iPlayers[record->m_player->index()] = true;
		record->m_eye_angles.y = velyaw + 180.f;
		break;
	case 2:
		g_resolver.iPlayers[record->m_player->index()] = true;
		record->m_eye_angles.y = velyaw - 90.f;
		break;
	case 3:
		g_resolver.iPlayers[record->m_player->index()] = true;
		record->m_eye_angles.y = velyaw + 90.f;
		break;
	}
}

void Resolver::ResolveWalk(AimPlayer* data, LagRecord* record) {

	//set resolver mode.
	record->m_mode = Modes::RESOLVE_WALK;

	g_resolver.resolver_mode[record->m_player->index()] = 4;

	//set the fake indicator off
	g_resolver.iPlayers[record->m_player->index()] = false;

	// apply lby to eyeangles.
	record->m_eye_angles.y = record->m_body;

	// delay body update.
	data->m_body_update = record->m_anim_time + 0.22f;

	if (data->m_move_index > 2) { // enables after 3 misses.
		g_resolver.iPlayers[record->m_player->index()] = true;
		AntiFreestand(record);
	}

	std::memcpy(&data->m_walk_record, record, sizeof(LagRecord));
}

void Resolver::ResolveStand(LagRecord* record, AimPlayer* data, Player* player)
{
	// pointer for easy access.
	LagRecord* move = &data->m_walk_record;

	// get predicted away angle for the player.
	float away = math::CalcAngle(g_cl.m_local->m_vecOrigin(), player->m_vecOrigin()).y;

	C_AnimationLayer* curr = &record->m_layers[3];
	int act = data->m_player->GetSequenceActivity(curr->m_sequence);

	float diff = math::NormalizedAngle(record->m_body - move->m_body);
	float delta = record->m_anim_time - move->m_anim_time;

	ang_t vAngle = ang_t(0, 0, 0);
	math::CalcAngle3(record->m_player->m_vecOrigin(), g_cl.m_local->m_vecOrigin(), vAngle);

	float flToMe = vAngle.y;

	float at_target_yaw = math::CalcAngle(record->m_player->m_vecOrigin(), g_cl.m_local->m_vecOrigin()).y;

	if (move->m_sim_time > 0.f) {
		vec3_t delta = move->m_origin - record->m_origin;
		float time = move->m_pred_time;
		if (delta.length() <= 128.f) {
			data->m_moved = true;
		}
	}
	if (!data->m_moved) {

		record->m_mode = Modes::RESOLVE_UNKNOWM;

		C_AnimationLayer* current_layer = &record->m_layers[6];
		static C_AnimationLayer* previous_layer = current_layer;
		int current_layer_activity = data->m_player->GetSequenceActivity(current_layer->m_sequence);
		int previous_layer_activity = data->m_player->GetSequenceActivity(previous_layer->m_sequence);

		if (current_layer_activity == 979) {
			if (previous_layer_activity == 979) {
				if (previous_layer->m_cycle != current_layer->m_cycle || current_layer->m_weight == 1.f) {
					if (data->m_records.size() >= 2)
					{
						LagRecord* previous_record = data->m_records[1].get();

						g_resolver.iPlayers[record->m_player->index()] = true;

					}
				}
				else if (current_layer->m_weight == 0.f && (previous_layer->m_cycle > 0.92f && current_layer->m_cycle > 0.92f)) { // breaking lby with delta < 120
					g_resolver.iPlayers[record->m_player->index()] = true;
					AntiFreestand(record);
				}
			}
		}
		else if (data->m_body != data->m_old_body)
		{
			g_resolver.resolver_mode[record->m_player->index()] = 4;
			record->m_eye_angles.y = record->m_body;
			data->m_body_update = record->m_anim_time + 1.1f;
			iPlayers[record->m_player->index()] = false;
			record->m_mode = Modes::RESOLVE_BODY;
		}
		else if (data->m_unknown_move >= 1) {
			g_resolver.iPlayers[record->m_player->index()] = true;
			ResolveYawBruteforce(record, data);
		}
		else {
			g_resolver.iPlayers[record->m_player->index()] = true;
			AntiFreestand(record);
		}
	}
	else if (data->m_moved) {
		float diff = math::NormalizedAngle(record->m_body - move->m_body);
		float delta = record->m_anim_time - move->m_anim_time;

		record->m_mode = Modes::RESOLVE_STAND;

		const float at_target_yaw = GetAwayAngle(record);
		record->m_eye_angles.y = record->m_body - 145.f;
		C_AnimationLayer* current_layer = &record->m_layers[6];
		static C_AnimationLayer* previous_layer = current_layer;
		int current_layer_activity = data->m_player->GetSequenceActivity(current_layer->m_sequence);
		int previous_layer_activity = data->m_player->GetSequenceActivity(previous_layer->m_sequence);

		if (current_layer_activity == 979) { // smoking that LBY PACK
			if (previous_layer_activity == 979) {
				if (previous_layer->m_cycle != current_layer->m_cycle || current_layer->m_weight == 1.f) {
					if (data->m_records.size() >= 2)
					{
						LagRecord* previous_record = data->m_records[1].get();
						g_resolver.resolver_mode[record->m_player->index()] = 1;
						g_resolver.iPlayers[record->m_player->index()] = true;

					}
				}
				else if (current_layer->m_weight == 0.f && (previous_layer->m_cycle > 0.92f && current_layer->m_cycle > 0.92f)) { // breaking lby with delta < 120
					g_resolver.iPlayers[record->m_player->index()] = true;
					g_resolver.resolver_mode[record->m_player->index()] = 2;
					AntiFreestand(record);
				}
			}
		}
		else if (data->m_stand_index >= 1) {
			g_resolver.iPlayers[record->m_player->index()] = true;
			g_resolver.resolver_mode[record->m_player->index()] = 3;
			ResolveYawBruteforce(record, data);
		}
		else if (data->m_stand_index2 >= 1) {
			g_resolver.iPlayers[record->m_player->index()] = true;
			g_resolver.resolver_mode[record->m_player->index()] = 3;
			ResolveYawBruteforce(record, data);
		}
		else if (data->m_body != data->m_old_body)
		{
			g_resolver.resolver_mode[record->m_player->index()] = 4;
			record->m_eye_angles.y = record->m_body;
			data->m_body_update = record->m_anim_time + 1.1f;
			iPlayers[record->m_player->index()] = false;
			record->m_mode = Modes::RESOLVE_BODY;
		}
		else {
			g_resolver.iPlayers[record->m_player->index()] = true;
			g_resolver.resolver_mode[record->m_player->index()] = 5;
			AntiFreestand(record);
		}
	}
}

void Resolver::ResolveAir(AimPlayer* data, LagRecord* record, Player* player) {
	// for no-spread call a seperate resolver.
	if (g_menu.main.config.mode.get() == 1) {
		g_resolver.iPlayers[record->m_player->index()] = true;
		AirNS(data, record);
		return;
	}
	if (record->m_velocity.length_2d() < 60.f && !(record->m_flags & FL_ONGROUND)) {
		if (data->m_air_index < 0) {
			record->m_mode = Modes::RESOLVE_AIR;
			g_resolver.resolver_mode[record->m_player->index()] = 6;
			g_resolver.iPlayers[record->m_player->index()] = true;
			AntiFreestand(record);
			return;
		}
		else {
			g_resolver.resolver_mode[record->m_player->index()] = 3;
			ResolveYawBruteforce(record, data);
		}
	}
}

void Resolver::ResolvePoses(Player* player, LagRecord* record) {
	AimPlayer* data = &g_aimbot.m_players[player->index() - 1];

	// only do this bs when in air.
	if (record->m_mode == Modes::RESOLVE_AIR) {
		// ang = pose min + pose val x ( pose range )

		// lean_yaw
		player->m_flPoseParameter()[2] = g_csgo.RandomInt(0, 4) * 0.25f;

		// body_yaw
		player->m_flPoseParameter()[11] = g_csgo.RandomInt(1, 3) * 0.25f;
	}
}



/* ------------------------NO SPREAD----------------------------------------*/

void Resolver::StandNS(AimPlayer* data, LagRecord* record) {
	// get away angles.
	float away = GetAwayAngle(record);

	switch (data->m_shots % 8) {
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 90.f;
		break;
	case 2:
		record->m_eye_angles.y = away - 90.f;
		break;

	case 3:
		record->m_eye_angles.y = away + 45.f;
		break;
	case 4:
		record->m_eye_angles.y = away - 45.f;
		break;

	case 5:
		record->m_eye_angles.y = away + 135.f;
		break;
	case 6:
		record->m_eye_angles.y = away - 135.f;
		break;

	case 7:
		record->m_eye_angles.y = away + 0.f;
		break;

	default:
		break;
	}

	// force LBY to not fuck any pose and do a true bruteforce.
	record->m_body = record->m_eye_angles.y;
}



void Resolver::AirNS(AimPlayer* data, LagRecord* record) {
	// get away angles.
	float away = GetAwayAngle(record);

	switch (data->m_shots % 9) {
	case 0:
		record->m_eye_angles.y = away + 180.f;
		break;

	case 1:
		record->m_eye_angles.y = away + 150.f;
		break;
	case 2:
		record->m_eye_angles.y = away - 150.f;
		break;

	case 3:
		record->m_eye_angles.y = away + 165.f;
		break;
	case 4:
		record->m_eye_angles.y = away - 165.f;
		break;

	case 5:
		record->m_eye_angles.y = away + 135.f;
		break;
	case 6:
		record->m_eye_angles.y = away - 135.f;
		break;

	case 7:
		record->m_eye_angles.y = away + 90.f;
		break;
	case 8:
		record->m_eye_angles.y = away - 90.f;
		break;

	default:
		break;
	}
}
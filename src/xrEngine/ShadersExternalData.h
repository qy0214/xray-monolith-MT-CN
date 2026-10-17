#pragma once

// Õðàíèëèùå âíåøíèõ øåéäåðíûõ ïàðàìåòðîâ, êîòîðûå ÷èòàþòñÿ â Blender_Recorder_StandartBinding.cpp
class ShadersExternalData //--#SM+#--
{
public:
	Fmatrix m_script_params; // Ìàòðèöà, çíà÷åíèÿ êîòîðîé äîñòóïíû èç Lua
	Fvector4 hud_params;     // [zoom_rotate_factor, secondVP_zoom_factor, hud_fov, NULL] - Ïàðàìåòðû õóäà îðóæèÿ
	Fvector4 hud_fov_params; // [scope_zoom_factor, min_scope_zoom_factor, NULL, NULL]
	// FDDA / device-screen contract: the modpack exoskeleton screens read the actor
	// condition, the screen interference and the on-screen clock from these.
	Fvector4 m_actor_params; // [_, actor condition 0..1, _, _] -> red..green tint
	Fvector4 m_affects;      // [screen interference 0..1, _, _, _]
	Fvector4 m_digiclock;    // [hours, minutes, seconds, day phase]
	Fvector4 m_timearrow;    // [hour hand, minute hand, _, _] as 0..1 phases
	Fvector4 m_timearrow2;   // [second hand, _, _, _] as a 0..1 phase
	Fvector4 m_blender_mode; // x\y = [0 - default, 1 - night vision, 2 - thermo vision, ... ñì. common.h] - Ðåæèìû ðåíäåðèíãà
							 // x - îñíîâíîé âüþïîðò, y - âòîðîé âüþïîðò, z = ?, w = [0 - èä¸ò ðåíäåð îáû÷íîãî îáúåêòà, 1 - èä¸ò ðåíäåð äåòàëüíûõ îáúåêòîâ (òðàâà, ìóñîð)]

	ShadersExternalData()
	{
		m_script_params = Fmatrix();
		hud_params.set(0.f, 0.f, 0.f, 0.f);
		hud_fov_params.set(0.f, 0.f, 0.f, 0.f);
		m_actor_params.set(0.f, 1.f, 0.f, 0.f);
		m_affects.set(0.f, 0.f, 0.f, 0.f);
		m_digiclock.set(0.f, 0.f, 0.f, 0.f);
		m_timearrow.set(0.f, 0.f, 0.f, 0.f);
		m_timearrow2.set(0.f, 0.f, 0.f, 0.f);
		m_blender_mode.set(0.f, 0.f, 0.f, 0.f);
	}
}; 
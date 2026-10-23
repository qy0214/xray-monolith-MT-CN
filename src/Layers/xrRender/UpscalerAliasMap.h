#pragma once
// ---------------------------------------------------------------------------
// MOD-name <-> native upscaler value mapping (single source of truth).
//
// The modpack MCM addresses the upscaler through the r4_* names whose token
// order differs from the native r_* order:
//   r4_upscaler: off=0, fsr3=1, dlss=2      (upscaler_compat_token)
//   r_upscaler : off=0, dlss=1, fsr3=2      (upscaler_token)
// Both token arrays in xrRender_console.cpp are generated from the method table
// below, and the per-frame alias sync uses these functions, so adding a method
// or a quality step is a one-table edit instead of a set of hand written
// reordering functions that select the wrong upscaler when one side is missed
// (review P2-5).
//
// Zero dependencies by design (built-in types only): the unit tests in
// src/tests include this header directly.
// ---------------------------------------------------------------------------

namespace compat
{
	struct UpscalerMethodEntry
	{
		const char* name;		// shared by both vocabularies
		unsigned int native;	// r_upscaler value
		unsigned int alias;		// r4_upscaler value
	};

	// Ordered by native value - that is upscaler_token's order.
	constexpr UpscalerMethodEntry UpscalerMethods[] = {
		{"off", 0u, 0u},
		{"dlss", 1u, 2u},
		{"fsr3", 2u, 1u},
	};

	// Out-of-range inputs keep the historical ternary-chain fallbacks: the alias
	// side falls back to native dlss, the native side to alias fsr3.
	constexpr unsigned int aliasMethodToNative(unsigned int v)
	{
		for (const UpscalerMethodEntry& e : UpscalerMethods)
			if (e.alias == v) return e.native;
		return 1u;
	}

	constexpr unsigned int nativeMethodToAlias(unsigned int v)
	{
		for (const UpscalerMethodEntry& e : UpscalerMethods)
			if (e.native == v) return e.alias;
		return 1u;
	}

	// The quality vocabularies differ only at the seams: r4_upscaler_quality has
	// no "off" (its native_aa step is DLSS's dlaa) and its "custom" step has no
	// native scale mode. Index by the alias value; see UpscalerQualityCustom.
	struct UpscalerQualityEntry
	{
		unsigned int alias;	// r4_upscaler_quality
		unsigned int dlss;	// r_dlss_quality
		unsigned int fsr3;	// r_fsr3_quality
	};

	constexpr UpscalerQualityEntry UpscalerQualities[] = {
		{0u, 1u, 0u},	// native_aa: dlaa / native_aa
		{1u, 2u, 1u},	// quality
		{2u, 3u, 2u},	// balanced
		{3u, 4u, 3u},	// performance
		{4u, 5u, 4u},	// ultra_performance
	};

	constexpr unsigned int UpscalerQualityCustom = 5u;

	constexpr unsigned int aliasQualityToDlss(unsigned int q)
	{
		for (const UpscalerQualityEntry& e : UpscalerQualities)
			if (e.alias == q) return e.dlss;
		return 4u;	// custom (and anything else): performance
	}

	constexpr unsigned int dlssQualityToAlias(unsigned int q)
	{
		for (const UpscalerQualityEntry& e : UpscalerQualities)
			if (e.dlss == q) return e.alias;
		return 3u;	// DLSS "off" has no MOD entry: performance
	}

	constexpr unsigned int aliasQualityToFsr3(unsigned int q)
	{
		for (const UpscalerQualityEntry& e : UpscalerQualities)
			if (e.alias == q) return e.fsr3;
		return 3u;
	}

	constexpr unsigned int fsr3QualityToAlias(unsigned int q)
	{
		for (const UpscalerQualityEntry& e : UpscalerQualities)
			if (e.fsr3 == q) return e.alias;
		return 3u;
	}

	constexpr bool upscalerMethodsRoundTrip()
	{
		for (const UpscalerMethodEntry& e : UpscalerMethods)
		{
			if (nativeMethodToAlias(aliasMethodToNative(e.alias)) != e.alias) return false;
			if (aliasMethodToNative(nativeMethodToAlias(e.native)) != e.native) return false;
		}
		return true;
	}

	static_assert(upscalerMethodsRoundTrip(), "upscaler method table must survive alias<->native round trips");

	// Which way the alias/native sync pushes, as a pure decision so the unit
	// tests can lock the rule that mattered (review P1-1): a user.ltx carrying
	// only a native selection - a pipdlss-era config - must push native ->
	// alias, never have the alias default (off) wipe it.
	enum class UpscalerSyncDirection
	{
		None,
		AliasToNative,
		NativeToAlias,
	};

	constexpr UpscalerSyncDirection upscaler_sync_direction(
		bool first_sync, bool alias_configured, bool native_configured,
		bool alias_changed, bool native_changed)
	{
		if (first_sync)
		{
			// The trackers are empty, so both sides look "changed" and the change
			// counters cannot pick a direction: the latches do. A fresh install
			// (neither side set) keeps the historical off push.
			if (alias_configured) return UpscalerSyncDirection::AliasToNative;
			if (native_configured) return UpscalerSyncDirection::NativeToAlias;
			return UpscalerSyncDirection::AliasToNative;
		}
		if (alias_changed && !native_changed) return UpscalerSyncDirection::AliasToNative;
		if (native_changed && !alias_changed) return UpscalerSyncDirection::NativeToAlias;
		return UpscalerSyncDirection::None;
	}
}

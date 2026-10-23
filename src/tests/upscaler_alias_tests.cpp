// Mapping tests for the MOD<->native upscaler vocabularies
// (Layers/xrRender/UpscalerAliasMap.h).
//
// The two console tokens deliberately enumerate the same methods in different
// orders (r4_upscaler: off/fsr3/dlss, r_upscaler: off/dlss/fsr3), and the sync
// in xrRender_console.cpp silently selects the wrong upscaler if the two sides
// drift. These lock the table: round trips through both directions, the swapped
// indexes, and the historical fallbacks for out-of-range and seam values.
#include "UpscalerAliasMap.h"
#include "test_harness.h"

void run_upscaler_alias_tests()
{
	using namespace compat;

	// every method survives both directions
	for (const UpscalerMethodEntry& e : UpscalerMethods)
	{
		CHECK(nativeMethodToAlias(aliasMethodToNative(e.alias)) == e.alias);
		CHECK(aliasMethodToNative(nativeMethodToAlias(e.native)) == e.native);
	}

	// the swapped order that motivated the table: dlss and fsr3 sit at different
	// indexes in the two vocabularies
	CHECK(aliasMethodToNative(2) == 1);	// r4 "dlss"  -> r_upscaler dlss
	CHECK(aliasMethodToNative(1) == 2);	// r4 "fsr3"  -> r_upscaler fsr3
	CHECK(nativeMethodToAlias(1) == 2);	// r_upscaler dlss -> r4 "dlss"
	CHECK(nativeMethodToAlias(2) == 1);	// r_upscaler fsr3 -> r4 "fsr3"
	CHECK(aliasMethodToNative(0) == 0 && nativeMethodToAlias(0) == 0);

	// out-of-range keeps the historical ternary-chain fallbacks
	CHECK(aliasMethodToNative(9) == 1);
	CHECK(nativeMethodToAlias(9) == 1);

	// quality: every alias step round-trips through its native column
	for (const UpscalerQualityEntry& e : UpscalerQualities)
	{
		CHECK(dlssQualityToAlias(aliasQualityToDlss(e.alias)) == e.alias);
		CHECK(fsr3QualityToAlias(aliasQualityToFsr3(e.alias)) == e.alias);
	}

	// the seams: DLSS "off" (0) has no MOD entry, r4 "custom" has no native mode
	CHECK(aliasQualityToDlss(0) == 1);	// r4 native_aa -> DLSS dlaa
	CHECK(dlssQualityToAlias(0) == 3);	// DLSS off -> performance
	CHECK(aliasQualityToDlss(UpscalerQualityCustom) == 4);
	CHECK(aliasQualityToFsr3(UpscalerQualityCustom) == 3);
	// fsr3 quality is the identity for the shared steps
	CHECK(aliasQualityToFsr3(3) == 3 && fsr3QualityToAlias(3) == 3);
	CHECK(fsr3QualityToAlias(9) == 3 && dlssQualityToAlias(9) == 3);

	std::printf("[tests] upscaler_alias: MOD<->native method/quality mapping round trips\n");
}

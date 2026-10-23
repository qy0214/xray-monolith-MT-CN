#include "stdafx.h"
#pragma hdrstop

#include "fs_internal.h"

#include <functional>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <sstream>
#include "mezz_stringbuffer.h"

XRCORE_API CInifile const* pSettings = NULL;
XRCORE_API CInifile const* pSettingsAuth = NULL;

BOOL print_dltx_warnings = FALSE;

CInifile* CInifile::Create(const char* szFileName, BOOL ReadOnly)
{
	return xr_new<CInifile>(szFileName, ReadOnly);
}

void CInifile::Destroy(CInifile* ini)
{
	xr_delete(ini);
}

bool sect_pred(const CInifile::Sect& x, LPCSTR val) noexcept
{
	return xr_strcmp(x.Name, val) < 0;
};

//------------------------------------------------------------------------------
//Тело функций Inifile
//------------------------------------------------------------------------------
XRCORE_API BOOL _parse(LPSTR dest, LPCSTR src)
{
	BOOL bInsideSTR = false;
	if (src)
	{
		while (*src)
		{
			if (isspace((u8)*src))
			{
				if (bInsideSTR)
				{
					*dest++ = *src++;
					continue;
				}
				while (*src && isspace(*src))
				{
					++src;
				}
				continue;
			}
			else if (*src == '"')
			{
				bInsideSTR = !bInsideSTR;
			}
			*dest++ = *src++;
		}
	}
	*dest = 0;
	return bInsideSTR;
}

XRCORE_API void _decorate(LPSTR dest, LPCSTR src)
{
	if (src)
	{
		BOOL bInsideSTR = false;
		while (*src)
		{
			if (*src == ',')
			{
				if (bInsideSTR) { *dest++ = *src++; }
				else
				{
					*dest++ = *src++;
					*dest++ = ' ';
				}
				continue;
			}
			else if (*src == '"')
			{
				bInsideSTR = !bInsideSTR;
			}
			*dest++ = *src++;
		}
	}
	*dest = 0;
}

//------------------------------------------------------------------------------

BOOL CInifile::Sect::line_exist(LPCSTR L, LPCSTR* val)
{
	auto A = std::lower_bound(Data.begin(), Data.end(), L, item_comparator());
	if (A != Data.end() && xr_strcmp(*A->first, L) == 0)
	{
		if (val) *val = *A->second;
		return TRUE;
	}
	return FALSE;
}

// Initialize the static cache member
BOOL dltx_use_cache = TRUE;
xr_unordered_flat_map<xr_string, CInifile::Root> CInifile::CachedData;
xrCriticalSection CInifile::CacheCS;
void CInifile::InvalidateCache(LPCSTR path) {
	if (path && path[0])
	{
		xr_string FileName(path);
		toLowerCase(FileName);
		xrCriticalSectionGuard g(CacheCS);
		CachedData.erase(FileName);
	}
	else
	{
		xrCriticalSectionGuard g(CacheCS);
		for (auto& p : CachedData)
		{
			p.second.clear();
		}
		CachedData.clear();
		CachedData.rehash(0);
	}
};

void CInifile::InsertIntoDATA(xr_unordered_flat_map<shared_str, Items>& FinalData)
{
	DATA.reserve(FinalData.size());
	for (auto& SectPair : FinalData)
	{
        DATA.push_back({ SectPair.first, SectPair.second });
	}
	std::sort(DATA.begin(), DATA.end(), [](const Sect& a, const Sect& b)
	{
		return xr_strcmp(a.Name, b.Name) < 0;
	});
}

CInifile::CInifile(IReader* F, LPCSTR path
#ifndef _EDITOR
                   , allow_include_func_t allow_include_func
#endif
)
{
	PROF_EVENT("CInifile::CInifile IReader");
	m_file_name[0] = 0;
	m_flags.zero();
	m_flags.set(eSaveAtEnd, FALSE);
	m_flags.set(eReadOnly, TRUE);
	m_flags.set(eOverrideNames, FALSE);
	Load(F, path
#ifndef _EDITOR
	     , allow_include_func
#endif
	);
}

CInifile::CInifile(LPCSTR szFileName,
                   BOOL ReadOnly,
                   BOOL bLoad,
                   BOOL SaveAtEnd,
                   u32 sect_count
#ifndef _EDITOR
                   , allow_include_func_t allow_include_func
#endif
)

{
	PROF_EVENT("CInifile::CInifile FileName");
	if (szFileName && strstr(szFileName, "system"))
		Msg("-----loading %s", szFileName);

	m_file_name[0] = 0;
	m_flags.zero();
	if (szFileName)
		xr_strcpy(m_file_name, sizeof(m_file_name), szFileName);

	m_flags.set(eSaveAtEnd, SaveAtEnd);
	m_flags.set(eReadOnly, ReadOnly);

	if (bLoad)
	{
		// Find in cache and use it, skip initiating IReader
		if (dltx_use_cache && IsValidFileNameForCache())
		{
			xr_string FileName(m_file_name);
			toLowerCase(FileName);

			xrCriticalSectionGuard g(CacheCS);
			auto CachedDataIt = CachedData.find(FileName);
			if (CachedDataIt != CachedData.end())
			{
				if (print_dltx_warnings)
					Msg("[DLTX] [%s] Found data in cache", m_file_name);
                DATA = CachedDataIt->second;
				return;
			}
		}

		string_path path, folder;
		_splitpath(m_file_name, path, folder, 0, 0);
		xr_strcat(path, sizeof(path), folder);
		IReader* R = FS.r_open(szFileName);
		if (R)
		{
			if (sect_count)
				DATA.reserve(sect_count);
			Load(R, path
#ifndef _EDITOR
			     , allow_include_func
#endif
			);
			FS.r_close(R);
		}
	}
}

CInifile::~CInifile()
{
	if (!m_flags.test(eReadOnly) && m_flags.test(eSaveAtEnd))
	{
		if (!save_as())
			Log("!Can't save inifile:", m_file_name);
	}
}

// ---------------------------------------------------------------------------
// DLTX: the mod-merge machinery (insert_item, loadFile, LTXLoad,
// StashCurrentSection, SortAndFilterSection, MergeSections, EvaluateSection
// and the DLTX_* queries) moved to Xr_ini_dltx.cpp. This file keeps the
// generic CInifile container: ctors, Load, save_as, the r_/w_ readers and
// writers. print_dltx_warnings stays here (both files read it).
// ---------------------------------------------------------------------------

void CInifile::Load(IReader* F, LPCSTR path
#ifndef _EDITOR
                    , allow_include_func_t allow_include_func
#endif
)
{
	R_ASSERT(F);

	static shared_str DLTX_DELETE = "DLTX_DELETE";
	string_path currentFileName;

	// Assemble paths and filename
	MezzStringBuffer split_drive;
	MezzStringBuffer split_dir;
	MezzStringBuffer split_name;
	MezzStringBuffer split_ext;

	_splitpath_s(m_file_name, split_drive, split_drive.GetSize(), split_dir, split_dir.GetSize(), split_name, split_name.GetSize(), split_ext, split_ext.GetSize());

	xr_string FileName = xr_string(split_name) + xr_string(split_ext);
	strcpy(currentFileName, FileName.c_str());

	// CRITICAL OPTIMIZATION: Single-pass load instead of double read
	// Parse both base and override data in one pass
	// Start with depth 0
	LTXLoad(
		F,
		path,
		true,
		currentFileName,
		0
#ifndef _EDITOR
		, allow_include_func
#endif
	);

	// Sort items by depth and name
	for (auto& [k, v] : BaseData)
		SortAndFilterSection(v);
	for (auto& [k, v] : OverrideData)
		SortAndFilterSection(v);

	// Merge base and override data together
	EvaluationsContext Evaluations;
	Evaluations.ResolvedCache.reserve(BaseData.size() + OverrideData.size());
	RStringVec BaseDataSectionNames;
	BaseDataSectionNames.reserve(BaseData.size());
	for (const auto& SectPair : BaseData)
	{
		BaseDataSectionNames.push_back(SectPair.first);
	}
	for (const auto& SectName : BaseDataSectionNames)
	{
		EvaluateSection(SectName, Evaluations, currentFileName);
	}

	auto& ResolvedData = Evaluations.ResolvedCache;
	// Handle marked-for-delete sections
	for (auto &s : SectionsToDelete)
	{
		Msg("[DLTX] [%s] Found section %s to delete", m_file_name, s.c_str());
		auto it = ResolvedData.find(s);
		if (it != ResolvedData.end())
		{
			Msg("[DLTX] [%s] Deleting section %s", m_file_name, s.c_str());
			ResolvedData.erase(it);
			auto s_it = OverrideData.find(s);
			if (s_it != OverrideData.end())
			{
				Msg("[DLTX] [%s] Deleting overrides for section %s", m_file_name, s.c_str());
				OverrideData.erase(s_it);
			}
		}
	}

	// Insert all finalized sections into final container
	InsertIntoDATA(ResolvedData);

    if (dltx_use_cache && IsValidFileNameForCache())
    {
        xr_string FileName(m_file_name);
        toLowerCase(FileName);
        xrCriticalSectionGuard g(CacheCS);
        CachedData.emplace(std::move(FileName), DATA);
    }

	// Handle override warnings
	if (OverrideData.size())
	{
		if (print_dltx_warnings)
		{
			for (const auto& [k, v] : OverrideData)
			{
				auto override_filenames = OverrideToFilename.find(k);
				if (override_filenames != OverrideToFilename.end())
				{
					for (const auto& override_filename : override_filenames->second)
					{
						Msg("~[DLTX] WARNING: Attemped to override section '%s', which doesn't exist. Ensure that a base section with the same name is loaded first. Check this file and its DLTX mods: %s, mod file %s", k.c_str(), m_file_name, override_filename.c_str());
					}
				}
			}
		}
	}

	// Cleanup
	OverrideToFilename.clear();
	SectionToFilename.clear();
	SectionsToDelete.clear();
	BaseParentDataMap.clear();
	BaseData.clear();
	OverrideParentDataMap.clear();
	OverrideData.clear();
	OverrideModifyListData.clear();

	OverrideToFilename.rehash(0);
	SectionToFilename.rehash(0);
	SectionsToDelete.rehash(0);
	BaseParentDataMap.rehash(0);
	BaseData.rehash(0);
	OverrideParentDataMap.rehash(0);
	OverrideData.rehash(0);
	OverrideModifyListData.rehash(0);
}

// DLTX debug queries (DLTX_print, DLTX_getFilenameOfLine, DLTX_isOverride)
// moved to Xr_ini_dltx.cpp.

void CInifile::save_as(IWriter& writer, bool bcheck) const
{
	string4096 temp, val;
	for (RootCIt r_it = DATA.begin(); r_it != DATA.end(); ++r_it)
	{
		xr_sprintf(temp, sizeof(temp), "[%s]", (*r_it).Name.c_str());
		writer.w_string(temp);
		if (bcheck)
		{
			xr_sprintf(temp, sizeof(temp), "; %d %d", (*r_it).Name._get()->intrusive_ref_count(), xr_strlen((*r_it).Name._get()->value));
			writer.w_string(temp);
		}

		for (const Item& I : (*r_it).Data)
		{
			if (*I.first)
			{
				if (*I.second)
				{
					_decorate(val, *I.second);
					// only name and value
					xr_sprintf(temp, sizeof(temp), "%8s%-32s = %-32s", " ", I.first.c_str(), val);
				}
				else
				{
					// only name
					xr_sprintf(temp, sizeof(temp), "%8s%-32s = ", " ", I.first.c_str());
				}
			}
			else
			{
				// no name, so no value
				temp[0] = 0;
			}
			_TrimRight(temp);
			if (temp[0]) writer.w_string(temp);
		}
		writer.w_string(" ");
	}
}

bool CInifile::save_as(LPCSTR new_fname)
{
	// save if needed
	if (new_fname && new_fname[0])
		xr_strcpy(m_file_name, sizeof(m_file_name), new_fname);

	R_ASSERT(m_file_name&&m_file_name[0]);
	IWriter* F = FS.w_open_ex(m_file_name);
	if (!F)
		return (false);

	xr_string FileName(m_file_name);
	toLowerCase(FileName);
	InvalidateCache(FileName.c_str());
	save_as(*F);
	FS.w_close(F);
	return (true);
}

BOOL CInifile::section_exist(LPCSTR S) const
{
	RootCIt I = std::lower_bound(DATA.begin(), DATA.end(), S, sect_pred);
	return (I != DATA.end() && xr_strcmp(*(*I).Name, S) == 0);
}

BOOL CInifile::line_exist(LPCSTR S, LPCSTR L) const
{
	if (!section_exist(S)) return FALSE;

	Sect& I = r_section(S);
	auto A = std::lower_bound(I.Data.begin(), I.Data.end(), L, item_comparator());
	return A != I.Data.end() && xr_strcmp(*A->first, L) == 0;
}

u32 CInifile::line_count(LPCSTR Sname) const
{
	Sect& S = r_section(Sname);
	auto I = S.Data.begin();
	u32 C = 0;
	for (; I != S.Data.end(); I++) if (*I->first) C++;
	return C;
}

u32 CInifile::section_count() const
{
	return DATA.size();
}


//--------------------------------------------------------------------------------------
CInifile::Sect& CInifile::r_section(const shared_str& S) const { return r_section(*S); }
BOOL CInifile::line_exist(const shared_str& S, const shared_str& L) const { return line_exist(*S, *L); }
u32 CInifile::line_count(const shared_str& S) const { return line_count(*S); }
BOOL CInifile::section_exist(const shared_str& S) const { return section_exist(*S); }

//--------------------------------------------------------------------------------------
// Read functions
//--------------------------------------------------------------------------------------
CInifile::Sect& CInifile::r_section(LPCSTR S) const
{
	R_ASSERT2(S && strlen(S),
	         "Empty section (null\\'') passed into CInifile::r_section(). See info above ^, check your configs and 'call stack'.")
	; //--#SM+#--

	char section[256];
	xr_strcpy(section, sizeof(section), S);
	strlwr(section);
	RootCIt I = std::lower_bound(DATA.begin(), DATA.end(), section, sect_pred);
	if (!(I != DATA.end() && xr_strcmp(*(*I).Name, section) == 0))
	{
		Debug.fatal(DEBUG_INFO, "Can't open section '%s'. Please attach [*.ini_log] file to your bug report", S);
	}
    const CInifile::Sect& res = *I;
	return const_cast<CInifile::Sect&>(res);
}

LPCSTR CInifile::r_string(LPCSTR S, LPCSTR L) const
{
	if (!S || !L || !strlen(S) || !strlen(L)) //--#SM+#-- [fix for one of "xrDebug - Invalid handler" error log]
	{
		Msg("!![ERROR] CInifile::r_string: S = [%s], L = [%s]", S, L);
	}

	Sect const& I = r_section(S);
	auto A = std::lower_bound(I.Data.begin(), I.Data.end(), L, item_comparator());
	if (A != I.Data.end() && xr_strcmp(*A->first, L) == 0)
	{
		shared_str V = A->second;
		LPCSTR res = *V;
		return res;
	}
	else
		Debug.fatal(DEBUG_INFO, "Can't find variable %s in [%s]", L, S);
	return 0;
}

shared_str CInifile::r_string_wb(LPCSTR S, LPCSTR L) const
{
	LPCSTR _base = r_string(S, L);

	if (0 == _base) return shared_str(0);

	string4096 _original;
	xr_strcpy(_original, sizeof(_original), _base);
	u32 _len = xr_strlen(_original);
	if (0 == _len) return shared_str("");
	if ('"' == _original[_len - 1]) _original[_len - 1] = 0; // skip end
	if ('"' == _original[0]) return shared_str(&_original[0] + 1); // skip begin
	return shared_str(_original);
}

u8 CInifile::r_u8(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	return u8(atoi(C));
}

u16 CInifile::r_u16(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	return u16(atoi(C));
}

u32 CInifile::r_u32(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	return u32(atoi(C));
}

u64 CInifile::r_u64(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
#ifndef _EDITOR
	return _strtoui64(C, NULL, 10);
#else
    return (u64)_atoi64(C);
#endif
}

s64 CInifile::r_s64(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	return _atoi64(C);
}

s8 CInifile::r_s8(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	return s8(atoi(C));
}

s16 CInifile::r_s16(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	return s16(atoi(C));
}

s32 CInifile::r_s32(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	return s32(atoi(C));
}

float CInifile::r_float(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	return float(atof(C));
}

Fcolor CInifile::r_fcolor(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	Fcolor V = {0, 0, 0, 0};
	sscanf(C, "%f,%f,%f,%f", &V.r, &V.g, &V.b, &V.a);
	return V;
}

u32 CInifile::r_color(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	u32 r = 0, g = 0, b = 0, a = 255;
	sscanf(C, "%d,%d,%d,%d", &r, &g, &b, &a);
	return color_rgba(r, g, b, a);
}

Ivector2 CInifile::r_ivector2(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	Ivector2 V = {0, 0};
	sscanf(C, "%d,%d", &V.x, &V.y);
	return V;
}

Ivector3 CInifile::r_ivector3(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	Ivector V = {0, 0, 0};
	sscanf(C, "%d,%d,%d", &V.x, &V.y, &V.z);
	return V;
}

Ivector4 CInifile::r_ivector4(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	Ivector4 V = {0, 0, 0, 0};
	sscanf(C, "%d,%d,%d,%d", &V.x, &V.y, &V.z, &V.w);
	return V;
}

Fvector2 CInifile::r_fvector2(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	Fvector2 V = {0.f, 0.f};
	sscanf(C, "%f,%f", &V.x, &V.y);
	return V;
}

Fvector3 CInifile::r_fvector3(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	Fvector3 V = {0.f, 0.f, 0.f};
	sscanf(C, "%f,%f,%f", &V.x, &V.y, &V.z);
	return V;
}

Fvector4 CInifile::r_fvector4(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	Fvector4 V = {0.f, 0.f, 0.f, 0.f};
	sscanf(C, "%f,%f,%f,%f", &V.x, &V.y, &V.z, &V.w);
	return V;
}

BOOL CInifile::r_bool(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	VERIFY2(
		xr_strlen(C) <= 5,
		make_string(
			"\"%s\" is not a valid bool value, section[%s], line[%s]",
			C,
			S,
			L
		)
	);
	char B[8];
	strncpy_s(B, sizeof(B), C, 7);
	B[7] = 0;
	strlwr(B);
	return IsBOOL(B);
}

CLASS_ID CInifile::r_clsid(LPCSTR S, LPCSTR L) const
{
	LPCSTR C = r_string(S, L);
	return TEXT2CLSID(C);
}

int CInifile::r_token(LPCSTR S, LPCSTR L, const xr_token* token_list) const
{
	LPCSTR C = r_string(S, L);
	for (int i = 0; token_list[i].name; i++)
		if (!stricmp(C, token_list[i].name))
			return token_list[i].id;
	return 0;
}

BOOL CInifile::r_line(LPCSTR S, int L, const char** N, const char** V) const
{
	Sect& SS = r_section(S);
	if (L >= (int)SS.Data.size() || L < 0) return FALSE;
	for (SectCIt I = SS.Data.begin(); I != SS.Data.end(); I++)
		if (!(L--))
		{
			*N = *I->first;
			*V = *I->second;
			return TRUE;
		}
	return FALSE;
}

BOOL CInifile::r_line(const shared_str& S, int L, const char** N, const char** V) const
{
	return r_line(*S, L, N, V);
}

//--------------------------------------------------------------------------------------------------------
// Write functions
//--------------------------------------------------------------------------------------
void CInifile::w_string(LPCSTR S, LPCSTR L, LPCSTR V, LPCSTR comment)
{
	R_ASSERT(!m_flags.test(eReadOnly));

	// section
	string256 sect;
	_parse(sect, S);
	_strlwr(sect);

	if (!section_exist(sect))
	{
		// create _new_ section
		RootIt I = std::lower_bound(DATA.begin(), DATA.end(), sect, sect_pred);
		DATA.insert(I, Sect{ sect });
	}

	// parse line/value
	string4096 line;
	_parse(line, L);
	string4096 value;
	_parse(value, V);

	// duplicate & insert
	Item I;
	Sect& data = r_section(sect);
	I.first = (line[0] ? line : 0);
	I.second = (value[0] ? value : 0);

	//#ifdef DEBUG
	// I.comment = (comment?comment:0);
	//#endif
	auto it = std::lower_bound(data.Data.begin(), data.Data.end(), I.first, item_comparator());

	if (it != data.Data.end() && it->first.equal(I.first))
	{
		// Check for "first" matching
		if (0 == xr_strcmp(*it->first, *I.first))
		{
			BOOL b = m_flags.test(eOverrideNames);
			R_ASSERT2(b, make_string("name[%s] already exist in section[%s]", line, sect).c_str());
			it->second = I.second;
			it->filename = I.filename;
		}
		else
		{
			data.Data.insert(it, I);
		}
	}
	else
	{
		data.Data.insert(it, I);
	}
}

void CInifile::w_u8(LPCSTR S, LPCSTR L, u8 V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%d", V);
	w_string(S, L, temp, comment);
}

void CInifile::w_u16(LPCSTR S, LPCSTR L, u16 V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%d", V);
	w_string(S, L, temp, comment);
}

void CInifile::w_u32(LPCSTR S, LPCSTR L, u32 V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%d", V);
	w_string(S, L, temp, comment);
}

void CInifile::w_u64(LPCSTR S, LPCSTR L, u64 V, LPCSTR comment)
{
	string128 temp;
#ifndef _EDITOR
	_ui64toa_s(V, temp, sizeof(temp), 10);
#else
    _ui64toa(V, temp, 10);
#endif
	w_string(S, L, temp, comment);
}

void CInifile::w_s64(LPCSTR S, LPCSTR L, s64 V, LPCSTR comment)
{
	string128 temp;
#ifndef _EDITOR
	_i64toa_s(V, temp, sizeof(temp), 10);
#else
    _i64toa(V, temp, 10);
#endif
	w_string(S, L, temp, comment);
}

void CInifile::w_s8(LPCSTR S, LPCSTR L, s8 V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%d", V);
	w_string(S, L, temp, comment);
}

void CInifile::w_s16(LPCSTR S, LPCSTR L, s16 V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%d", V);
	w_string(S, L, temp, comment);
}

void CInifile::w_s32(LPCSTR S, LPCSTR L, s32 V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%d", V);
	w_string(S, L, temp, comment);
}

void CInifile::w_float(LPCSTR S, LPCSTR L, float V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%f", V);
	w_string(S, L, temp, comment);
}

void CInifile::w_fcolor(LPCSTR S, LPCSTR L, const Fcolor& V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%f,%f,%f,%f", V.r, V.g, V.b, V.a);
	w_string(S, L, temp, comment);
}

void CInifile::w_color(LPCSTR S, LPCSTR L, u32 V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%d,%d,%d,%d", color_get_R(V), color_get_G(V), color_get_B(V), color_get_A(V));
	w_string(S, L, temp, comment);
}

void CInifile::w_ivector2(LPCSTR S, LPCSTR L, const Ivector2& V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%d,%d", V.x, V.y);
	w_string(S, L, temp, comment);
}

void CInifile::w_ivector3(LPCSTR S, LPCSTR L, const Ivector3& V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%d,%d,%d", V.x, V.y, V.z);
	w_string(S, L, temp, comment);
}

void CInifile::w_ivector4(LPCSTR S, LPCSTR L, const Ivector4& V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%d,%d,%d,%d", V.x, V.y, V.z, V.w);
	w_string(S, L, temp, comment);
}

void CInifile::w_fvector2(LPCSTR S, LPCSTR L, const Fvector2& V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%f,%f", V.x, V.y);
	w_string(S, L, temp, comment);
}

void CInifile::w_fvector3(LPCSTR S, LPCSTR L, const Fvector3& V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%f,%f,%f", V.x, V.y, V.z);
	w_string(S, L, temp, comment);
}

void CInifile::w_fvector4(LPCSTR S, LPCSTR L, const Fvector4& V, LPCSTR comment)
{
	string128 temp;
	xr_sprintf(temp, sizeof(temp), "%f,%f,%f,%f", V.x, V.y, V.z, V.w);
	w_string(S, L, temp, comment);
}

void CInifile::w_bool(LPCSTR S, LPCSTR L, BOOL V, LPCSTR comment)
{
	w_string(S, L, V ? "on" : "off", comment);
}

void CInifile::remove_line(LPCSTR S, LPCSTR L)
{
	R_ASSERT(!m_flags.test(eReadOnly));

	if (line_exist(S, L))
	{
		Sect& data = r_section(S);
		auto A = std::lower_bound(data.Data.begin(), data.Data.end(), L, item_comparator());
		if (A != data.Data.end() && xr_strcmp(*A->first, L) == 0)
			data.Data.erase(A);
	}
}

bool CInifile::remove_section(LPCSTR S)
{
	R_ASSERT(!m_flags.test(eReadOnly));

	if (!S || !S[0])
		return false;

	// sections are stored parsed and lower cased, the same way a write creates them
	string256 sect;
	_parse(sect, S);
	_strlwr(sect);

	RootIt I = std::lower_bound(DATA.begin(), DATA.end(), sect, sect_pred);
	if (I == DATA.end() || xr_strcmp(*(*I).Name, sect) != 0)
		return false;

	DATA.erase(I);
	return true;
}

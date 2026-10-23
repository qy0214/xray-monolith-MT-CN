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

// ---------------------------------------------------------------------------
// DLTX: the mod-merge half of CInifile (demonized/IX-Ray system).
//
// Everything that resolves "base file + mod_*.ltx overrides" lives here: the
// file/mod loader (loadFile, LTXLoad), the section stash/sort/merge machinery
// (StashCurrentSection, SortAndFilterSection, MergeSections, EvaluateSection,
// insert_item and the parse helpers) and the DLTX debug queries. The generic
// CInifile container API (ctors, Load, save_as, the r_/w_ readers and writers)
// stays in Xr_ini.cpp. Split for reviewability only - no behaviour change.
//
// print_dltx_warnings is defined in Xr_ini.cpp (read there by the ctor cache
// path as well); dltx_use_cache and the CachedData cache are only touched by
// the code that stayed.
// ---------------------------------------------------------------------------

extern BOOL print_dltx_warnings;

// Defined in Xr_ini.cpp (XRCORE_API, but declared only at the use sites).
XRCORE_API BOOL _parse(LPSTR dest, LPCSTR src);

void CInifile::insert_item(Sect* tgt, Item& I)
{
	// demonized
	// DLTX: add or remove item from the section parameter if it has a structure of "name = item1, item2, item3, ..."
	// >name = item will add item to the list
	// <name = item will remove item from the list
	if (*I.first)
	{
		ModifyListType op = static_cast<ModifyListType>(I.first.c_str()[0]);
		if (op == ModifyListType::Insert || op == ModifyListType::Remove)
		{
			// push_back then back because vector must be created first if it doesn't exist via []
			OverrideModifyListData[tgt->Name].push_back(I);
			OverrideModifyListData[tgt->Name].back().insertionIndex = OverrideModifyListData[tgt->Name].size();
			return;
		}
	}

	// Just push back items, will be filtered later
	I.insertionIndex = tgt->Data.size();
	tgt->Data.push_back(I);
}

IC BOOL is_empty_line_now(IReader* F)
{
	char* a0 = (char*)F->pointer() - 4;
	char* a1 = (char*)(F->pointer()) - 3;
	char* a2 = (char*)F->pointer() - 2;
	char* a3 = (char*)(F->pointer()) - 1;

	return (*a0 == 13) && (*a1 == 10) && (*a2 == 13) && (*a3 == 10);
};

// Regex pattern cache (added before Load function)
static const std::regex& GetCachedRegex(const xr_string& pattern)
{
	static xr_unordered_flat_map<xr_string, std::regex> g_RegexCache;
	auto it = g_RegexCache.find(pattern);
	if (it == g_RegexCache.end())
	{
		auto result = g_RegexCache.emplace(pattern, std::regex(pattern.c_str()));
		return result.first->second;
	}
	return it->second;
}

// Helper function for efficient single-pass string trimming during read
static inline void TrimStringInPlace(xr_string& str)
{
	// Skip leading whitespace
	size_t start = 0;
	while (start < str.length() && isspace((u8)str[start]))
		++start;

	// Skip trailing whitespace
	size_t end = str.length();
	while (end > start && isspace((u8)str[end - 1]))
		--end;

	if (start > 0 || end < str.length())
	{
		str = str.substr(start, end - start);
	}
}

static void MergeParentSet(RStringVec* ParentsBase, RStringVec* ParentsOverride, bool bIncludeRemovers)
{
	// Optimized parent set merging using std::remove_if for O(n) complexity
	for (const auto& CurrentParentStr : *ParentsOverride)
	{
		const char* CurrentParent = CurrentParentStr.c_str();
		char first_char = *CurrentParent;
		bool bIsParentRemoval = (first_char == '!');

		// Build the opposite marker string more efficiently
		xr_string StaleParentString;
		StaleParentString.reserve(xr_strlen(CurrentParent) + 1);
		StaleParentString += (bIsParentRemoval ? "" : "!");
		StaleParentString += (CurrentParent + (bIsParentRemoval ? 1 : 0));

		// Use single-pass remove_if instead of reverse iteration with erase
		auto new_end = std::remove_if(ParentsBase->begin(), ParentsBase->end(),
			[&StaleParentString](const shared_str& item) {
				return xr_strcmp(item, StaleParentString.c_str()) == 0;
			});
		ParentsBase->erase(new_end, ParentsBase->end());

		// Insert new parent if not a remover or if including removers
		if (bIncludeRemovers || !bIsParentRemoval)
		{
			ParentsBase->push_back(CurrentParentStr);
		}
	}
};

void CInifile::loadFile(
		const string_path _fn,
		const string_path inc_path,
		const string_path name,
		string_path currentFileName,
		int depth
	#ifndef _EDITOR
		, allow_include_func_t allow_include_func
	#endif
	)
{
#ifndef _EDITOR
	if (!allow_include_func || allow_include_func(_fn))
#endif
	{
		IReader* I = FS.r_open(_fn);
		R_ASSERT3(I, "Can't find include file:", name);

		strcpy(currentFileName, name);

		LTXLoad(
			I,
			inc_path,
			false,
			currentFileName,
			depth
#ifndef _EDITOR
			, allow_include_func
#endif
		);

		FS.r_close(I);
	}
};

void CInifile::StashCurrentSection(
		Sect*& CurrentBase,
		Sect*& CurrentOverride,
		string_path currentFileName
	)
{
	// Store base section if exists
	if (CurrentBase)
	{
		auto SectIt = BaseData.find(CurrentBase->Name);
		if (SectIt != BaseData.end() && SectIt->first.equal(CurrentBase->Name))
		{
			Debug.fatal(DEBUG_INFO, "[DLTX] Duplicate section '%s' wasn't marked as an override.\n\nOverride section by prefixing it with '!' (![%s]) or give it a unique name.\n\nCheck this file and its DLTX mods:\n\"%s\",\nfile with section \"%s\",\nfile with duplicate \"%s\"", *CurrentBase->Name, *CurrentBase->Name, m_file_name, SectionToFilename[CurrentBase->Name].c_str(), currentFileName);
		}
		else
		{
			BaseData.emplace(CurrentBase->Name, *CurrentBase);
			SectionToFilename[CurrentBase->Name] = currentFileName;
		}
		xr_delete(CurrentBase);
	}

	// Store override section if exists
	if (CurrentOverride)
	{
		auto SectIt = OverrideData.find(CurrentOverride->Name);
		if (SectIt != OverrideData.end() && SectIt->first.equal(CurrentOverride->Name))
		{
			// Overwrite existing override data
			for (Item& CurrentItem : CurrentOverride->Data)
			{
				insert_item(&SectIt->second, CurrentItem);
			}

			OverrideToFilename[SectIt->first].insert(currentFileName);
		}
		else
		{
			OverrideData.emplace(CurrentOverride->Name, *CurrentOverride);
			OverrideToFilename[CurrentOverride->Name].insert(currentFileName);
		}
		xr_delete(CurrentOverride);
	}
};

void CInifile::SortAndFilterSection(Sect& Data)
{
	if (Data.Data.size() < 2) return;

	static shared_str DLTX_DELETE = "DLTX_DELETE";

	// 1. Sort by Key, then by Depth (Ascending), then by insertionOrder (Descending).
	std::sort(Data.Data.begin(), Data.Data.end(), [](const Item& a, const Item& b)
	{
		// Compare keys alpphabetically
		int res = xr_strcmp(a.first, b.first);
		if (res != 0) return res < 0;

		// Compare depths, lower depth wins
		if (a.depth != b.depth) return a.depth < b.depth;

		// Compare insertionIndex, higher wins
		return a.insertionIndex > b.insertionIndex;
	});

	// 2. Linear pass to keep the first (lowest depth) of each key group, but the item inserted last at that depth wins
	auto write_it = Data.Data.begin();
	for (auto read_it = Data.Data.begin(); read_it != Data.Data.end(); )
	{
		if (write_it != read_it)
		{
			*write_it = std::move(*read_it);
		}

		// Save the key pointer to skip duplicates
		shared_str current_key = write_it->first;
		++write_it;

		// Skip all other kv pairs
		++read_it;
		while (read_it != Data.Data.end() && read_it->first == current_key)
		{
			++read_it;
		}
	}
	Data.Data.erase(write_it, Data.Data.end());
}

// Single-pass LTXLoad that distinguishes override vs base data during parsing
void CInifile::LTXLoad (
		IReader* F,
		LPCSTR path,
		BOOL bIsRootFile,
		string_path currentFileName,
		int depth
#ifndef _EDITOR
		, allow_include_func_t allow_include_func
#endif
	)
{
	static shared_str DLTX_DELETE = "DLTX_DELETE";
	Sect* CurrentBase = 0;
	Sect* CurrentOverride = 0;
	MezzStringBuffer str;
	MezzStringBuffer str2;

	BOOL bInsideSTR = FALSE;
	BOOL bIsCurrentSectionOverride = FALSE;
	BOOL bHasLoadedModFiles = FALSE;

	static auto InsertParentStringsInMap = [](shared_str SectionName, xr_unordered_flat_map<shared_str, RStringVec>& ParentMap)
	{
		auto It = ParentMap.find(SectionName);

		if (It == ParentMap.end())
		{
			auto result = ParentMap.emplace(SectionName, RStringVec());
			return &result.first->second;
		}

		return &It->second;
	};

	static auto GetParentsSetFromString = [](const char* ParentString, MezzStringBuffer& str2)
	{
		auto ParentSet = RStringVec();

		u32 ItemCount = _GetItemCount(ParentString);

		for (u32 i = 0; i < ItemCount; i++)
		{
			_GetItem(ParentString, i, str2, str2.GetSize());
			ParentSet.insert(ParentSet.end(), str2.GetBuffer());
		}

		return ParentSet;
	};

	// Optimized regex match with caching
	static auto GetRegexMatch = [](const xr_string& InputString, const xr_string& PatternString)
	{
		const std::regex& Pattern = GetCachedRegex(PatternString);
		std::smatch MatchResult;
		xr_string input = InputString.c_str();

		std::regex_search(input, MatchResult, Pattern);

		if (MatchResult.begin() == MatchResult.end())
		{
			return xr_string();
		}

		xr_string result = MatchResult.begin()->str().c_str();
		return result;
	};

	// Optimized regex full match with caching
	static auto IsFullRegexMatch = [](const xr_string& InputString, const xr_string& PatternString)
	{
		const std::regex& Pattern = GetCachedRegex(PatternString);
		return std::regex_match(InputString.c_str(), Pattern);
	};

	xr_unordered_flat_set<shared_str> sectionsMarkedForCreate;

	while (!F->eof() || (bIsRootFile && !bHasLoadedModFiles))
	{
		if (!F->eof())
		{
			F->r_string(str, str.GetSize());
			_Trim(str);
		}
		else if (!bHasLoadedModFiles && bIsRootFile)
		{
			StashCurrentSection(
				CurrentBase,
				CurrentOverride,
				currentFileName
			);
			bHasLoadedModFiles = TRUE;

			if (!m_file_name[0])
			{
				continue;
			}

			// Assemble paths and filename
			MezzStringBuffer split_drive;
			MezzStringBuffer split_dir;
			MezzStringBuffer split_name;

			_splitpath_s(m_file_name, split_drive, split_drive.GetSize(), split_dir, split_dir.GetSize(), split_name, split_name.GetSize(), NULL, 0);

			xr_string FilePath = xr_string(split_drive) + xr_string(split_dir);
			xr_string FileName = split_name;

			// Collect all files that could potentially be confused as a root file by our mod files
			FS_FileSet AmbiguousFiles;
			FS.file_list(AmbiguousFiles, FilePath.c_str(), FS_ListFiles, (FileName + "_*.ltx").c_str());

			// Collect all matching mod files
			FS_FileSet ModFiles;
			FS.file_list(ModFiles, FilePath.c_str(), FS_ListFiles, ("mod_" + FileName + "_*.ltx").c_str());

			// Found mod files, set depth lower than base
			int d = -200;
			int dt = -200;
			for (auto It = ModFiles.begin(); It != ModFiles.end(); ++It)
			{
				xr_string ModFileName = It->name.c_str();

				// Determine if we should load this mod file, or if it's meant for a different root file
				static auto bIsModfileMeantForMeFunc = [](FS_FileSet AmbiguousFiles, xr_string ModFileName)
				{
					for (auto It2 = AmbiguousFiles.begin(); It2 != AmbiguousFiles.end(); ++It2)
					{
						xr_string name = It2->name.c_str();
						xr_string AmbiguousFileName = xr_string(GetRegexMatch(name, "^.+(?=.ltx$)").c_str());
						xr_string AmbiguousFileMatchPattern = xr_string("mod_") + AmbiguousFileName + xr_string("_.+.ltx");

						if (IsFullRegexMatch(ModFileName, AmbiguousFileMatchPattern.c_str()))
						{
							return false;
						}
					}

					return true;
				};
				bool bIsModfileMeantForMe = bIsModfileMeantForMeFunc(AmbiguousFiles, ModFileName);

				if (!bIsModfileMeantForMe)
				{
					continue;
				}

				xr_string ModFileNameStr = ModFileName.c_str();
				loadFile(
					(FilePath + ModFileNameStr).c_str(),
					FilePath.c_str(),
					ModFileName.c_str(),
					currentFileName,
					d
#ifndef _EDITOR
					, allow_include_func
#endif
				);
				d += dt;
			}

			continue;
		}
		xr_string currentLine = str;

		// Parse comment - single pass instead of multiple strchr calls
		LPSTR comm = strchr(str, ';');
		LPSTR comm_1 = strchr(str, '/');

		if (comm_1 && (*(comm_1 + 1) == '/') && ((!comm) || (comm && (comm_1 < comm))))
		{
			comm = comm_1;
		}

#ifdef DEBUG
		LPSTR comment = 0;
#endif
		if (comm)
		{
			// Check if comment is within quotes
			char quot = '"';
			bool in_quot = false;

			LPCSTR q1 = strchr(str, quot);
			if (q1 && q1 < comm)
			{
				LPCSTR q2 = strchr(++q1, quot);
				if (q2 && q2 > comm)
					in_quot = true;
			}

			if (!in_quot)
			{
				*comm = 0;
#ifdef DEBUG
				comment = comm + 1;
#endif
			}
		}

		_Trim(str);

		static auto isOverrideSection = [](char* str)
		{
			return strstr(str, "![") == &str[0];
		};

		static auto isSafeOverrideSection = [](char* str)
		{
			return strstr(str, "@[") == &str[0];
		};

		static auto isModSection = [](char* str)
		{
			return isOverrideSection(str) || isSafeOverrideSection(str);
		};

		if (str[0] && (str[0] == '#') && strstr(str, "#include"))
		{
			string_path inc_name;
			R_ASSERT(path && path[0]);
			if (_GetItem(str, 1, inc_name, '"'))
			{
				string_path fn, inc_path, folder;
				strconcat(sizeof(fn), fn, path, inc_name);
				_splitpath(fn, inc_path, folder, 0, 0);
				xr_strcat(inc_path, sizeof(inc_path), folder);

				if (strstr(inc_name, "*.ltx"))
				{
					FS_FileSet fset;
					FS.file_list(fset, inc_path, FS_ListFiles, inc_name);

					for (FS_FileSet::iterator it = fset.begin(); it != fset.end(); it++)
					{
						LPCSTR _name = it->name.c_str();
						string_path _fn;
						strconcat(sizeof(_fn), _fn, inc_path, _name);

						// Include file, increase depth by 1 from either mod file or base file
						loadFile(
							_fn,
							inc_path,
							_name,
							currentFileName,
							depth + 1
#ifndef _EDITOR
							, allow_include_func
#endif
						);
					}
				}
				// Include file, increase depth by 1 from either mod file or base file
				else
					loadFile(
						fn,
						inc_path,
						inc_name,
						currentFileName,
						depth + 1
#ifndef _EDITOR
						, allow_include_func
#endif
					);
			}

			continue;
		}
		else if (str[0] && strstr(str, "!![") == &str[0])
		{
			// Section delete
			StashCurrentSection(
				CurrentBase,
				CurrentOverride,
				currentFileName
			);

			u32 SectionNameStartPos = 3;
			xr_string SecName = xr_string(str).substr(SectionNameStartPos, strchr(str, ']') - str - SectionNameStartPos).c_str();
			for (auto i = SecName.begin(); i != SecName.end(); ++i)
			{
				*i = tolower(*i);
			}
			Msg("[DLTX] [%s] Encountered %s, mark section to delete", m_file_name, str.GetBuffer());
			SectionsToDelete.insert(SecName.c_str());

			continue;
		}
		else if ((str[0] && (str[0] == '[')) || isModSection(str))
		{
			// New section - stash previous
			StashCurrentSection(
				CurrentBase,
				CurrentOverride,
				currentFileName
			);

			u32 SectionNameStartPos = (isModSection(str) ? 2 : 1);
			xr_string SecName = xr_string(str).substr(SectionNameStartPos, strchr(str, ']') - str - SectionNameStartPos).c_str();
			for (auto i = SecName.begin(); i != SecName.end(); ++i)
			{
				*i = tolower(*i);
			}

			bIsCurrentSectionOverride = false;
			if (isOverrideSection(str))
			{
				bIsCurrentSectionOverride = true;
			}
			else if (isSafeOverrideSection(str))
			{
				bIsCurrentSectionOverride = true;
				auto SectIt = BaseData.find(SecName.c_str());
				if (SectIt == BaseData.end())
				{
					sectionsMarkedForCreate.insert(SecName.c_str());
				}
			}

			// Create appropriate section (base or override)
			if (bIsCurrentSectionOverride)
			{
				CurrentOverride = xr_new<Sect>();
				CurrentOverride->Name = SecName.c_str();
			}
			else
			{
				CurrentBase = xr_new<Sect>();
				CurrentBase->Name = SecName.c_str();
			}

			R_ASSERT3(strchr(str, ']'), "Bad ini section found: ", str);

			// Handle section inheritance
			LPCSTR inherited_names = strstr(str, "]:");
			if (0 != inherited_names)
			{
				VERIFY2(m_flags.test(eReadOnly), "Allow for readonly mode only.");
				inherited_names += 2;

				auto CurrentParents = GetParentsSetFromString(
					inherited_names,
					str2
				);

				if (bIsCurrentSectionOverride)
				{
					auto* SectionParents = InsertParentStringsInMap(SecName.c_str(), OverrideParentDataMap);
					MergeParentSet(SectionParents, &CurrentParents, true);
				}
				else
				{
					auto* SectionParents = InsertParentStringsInMap(SecName.c_str(), BaseParentDataMap);
					MergeParentSet(SectionParents, &CurrentParents, true);
				}
			}

			continue;
		}
		else if (str[0] && str[0] != ';')
		{
			// name = value
			bool bIsDelete = str[0] == '!';

			MezzStringBuffer value_raw;
			char* name = (char*)(str + (bIsDelete ? 1 : 0));
			char* t = strchr(name, '=');
			if (t)
			{
				*t = 0;
				_Trim(name);
				++t;
				xr_strcpy(value_raw, value_raw.GetSize(), t);
				bInsideSTR = _parse(str2, value_raw);
				if (bInsideSTR)
				{
					while (bInsideSTR)
					{
						xr_strcat(value_raw, value_raw.GetSize(), "\r\n");
						MezzStringBuffer str_add_raw;
						F->r_string(str_add_raw, str_add_raw.GetSize());
						R_ASSERT2(
							xr_strlen(value_raw) + xr_strlen(str_add_raw) < value_raw.GetSize(),
							make_string(
								"Incorrect inifile format: section[%s], variable[%s]. Odd number of quotes (\") found, but should be even."
								,
								(CurrentBase ? CurrentBase->Name.c_str() : (CurrentOverride ? CurrentOverride->Name.c_str() : "unknown")),
								name
							)
						);
						xr_strcat(value_raw, value_raw.GetSize(), str_add_raw);
						bInsideSTR = _parse(str2, value_raw);
						if (bInsideSTR)
						{
							if (is_empty_line_now(F))
								xr_strcat(value_raw, value_raw.GetSize(), "\r\n");
						}
					}
				}
			}
			else
			{
				_Trim(name);
				str2[0] = 0;
			}

			Item I;
			I.first = (name[0] ? name : NULL);
			if (!I.first)
			{
                if (print_dltx_warnings)
				    Msg("~[DLTX] WARNING: Malformed line %s in file %s, can't get key name, skipping, section data might be altered unexpectedly", currentLine.c_str(), currentFileName);
				continue;
			}
			I.second = bIsDelete ? DLTX_DELETE.c_str() : (str2[0] ? str2.GetBuffer() : NULL);

			auto fname = toLowerCaseCopy(trimCopy(getFilename(std::string(currentFileName))));
			I.filename = fname.c_str();
			I.depth = depth;

			if (*I.first || *I.second)
			{
				// Insert into appropriate current section
				if (CurrentBase)
					insert_item(CurrentBase, I);
				if (CurrentOverride)
					insert_item(CurrentOverride, I);
			}

			continue;
		}
	}

	StashCurrentSection(
		CurrentBase,
		CurrentOverride,
		currentFileName
	);

	// Create empty sections that were marked with @[ and weren't defined normally
	for (auto& SecName : sectionsMarkedForCreate)
	{
		auto SectIt = BaseData.find(SecName);
		if (SectIt == BaseData.end())
		{
			CurrentBase = xr_new<Sect>();
			CurrentBase->Name = SecName.c_str();
			BaseData.emplace(CurrentBase->Name, *CurrentBase);
			OverrideToFilename[CurrentBase->Name].insert(currentFileName);
			SectionToFilename[CurrentBase->Name] = currentFileName;
			xr_delete(CurrentBase);
		}
	}
};

CInifile::Items CInifile::MergeSections(
		const CInifile::Items& BaseItems,
		const CInifile::Items& OverrideItems,
		xr_unordered_flat_set<shared_str>& DeletedItems,
		bool IsMergingBaseAndMod
	)
{
	static shared_str DLTX_DELETE = "DLTX_DELETE";

	Items Result;
	Result.reserve(BaseItems.size() + OverrideItems.size());
	auto b_it = BaseItems.begin();
	auto o_it = OverrideItems.begin();

	while (b_it != BaseItems.end() || o_it != OverrideItems.end())
	{
		// 1. Handle end of streams
		if (b_it == BaseItems.end())
		{
			if (o_it->second == DLTX_DELETE)
			{
				if (IsMergingBaseAndMod)
				{
					DeletedItems.insert(o_it->first);
				}
			}
			else
			{
				Result.push_back(*o_it);
			}

			o_it++;
			continue;
		}

		if (o_it == OverrideItems.end())
		{
			Result.push_back(*b_it++);
			continue;
		}

		// 2. Compare Keys
		int cmp = xr_strcmp(b_it->first, o_it->first);

		if (cmp < 0)
		{
			// Base has a key that Override doesn't touch. Keep it.
			Result.push_back(*b_it++);
		}
		else if (cmp > 0)
		{
			// Override has a new key. Add it.
			// Check for DLTX_DELETE token and in Base+Mod merge, Add to DeletedItems for later CSV processing
			if (o_it->second == DLTX_DELETE)
			{
				if (IsMergingBaseAndMod)
				{
					DeletedItems.insert(o_it->first);
				}
			}
			else
			{
				Result.push_back(*o_it);
			}
			o_it++;
		}
		else
		{
			// Collision, key exists in both.
			// Override wins
			// Check for DLTX_DELETE token and
			// 1. In Base+Mod merge, Add to DeletedItems for later CSV processing
			// 2. In Parent+Base check, push Parent KV pair
			if (o_it->second == DLTX_DELETE)
			{
				if (IsMergingBaseAndMod)
				{
					DeletedItems.insert(o_it->first);
				}
				else
				{
					Result.push_back(*b_it);
				}
			}
			else
			{
				Result.push_back(*o_it);
			}

			o_it++;
			b_it++;
		}
	}
	return Result;
}

CInifile::Items CInifile::EvaluateSection(
	shared_str SectionName,
	EvaluationsContext& Evaluations,
	string_path currentFileName
)
{
	auto cache_it = Evaluations.ResolvedCache.find(SectionName);
	if (cache_it != Evaluations.ResolvedCache.end())
	{
		return cache_it->second;
	}

	if (Evaluations.IsInStack(SectionName))
	{
		Debug.fatal(DEBUG_INFO, "[DLTX] Section '%s' has cyclical dependencies. Cycle loop %s. Check this file and its DLTX mods: %s, mod file %s",
			SectionName.c_str(),
			Evaluations.GetRecursionStackAsString().c_str(),
			m_file_name,
			currentFileName
		);
	}

	Evaluations.RecursionStack.push_back(SectionName);

	static shared_str DLTX_DELETE = "DLTX_DELETE";

	auto BaseParentsIt = BaseParentDataMap.find(SectionName);
	auto OverrideParentsIt = OverrideParentDataMap.find(SectionName);

	auto* BaseParents = (BaseParentsIt != BaseParentDataMap.end()) ? &BaseParentsIt->second : nullptr;
	auto* OverrideParents = (OverrideParentsIt != OverrideParentDataMap.end()) ? &OverrideParentsIt->second : nullptr;

	// Create base parents map if override parents exist
	if (OverrideParents && !BaseParents)
	{
		auto result = BaseParentDataMap.emplace(SectionName, RStringVec());
		BaseParentsIt = BaseParentDataMap.find(SectionName);
		BaseParents = (BaseParentsIt != BaseParentDataMap.end()) ? &BaseParentsIt->second : nullptr;
		MergeParentSet(BaseParents, OverrideParents, false);
	}
	else if (BaseParents && OverrideParents)
	{
		MergeParentSet(BaseParents, OverrideParents, false);
	}

	Items ResolvedParents;
	xr_unordered_flat_set<shared_str> DeletedItems;
	if (BaseParents)
	{
		for (const auto& parent : *BaseParents)
		{
			auto BaseDataIt = BaseData.find(parent);
			if (BaseDataIt == BaseData.end())
			{
				auto OverrideDataIt = OverrideData.find(parent);
				if (OverrideDataIt != OverrideData.end())
				{
                    if (print_dltx_warnings)
                    {
                        Msg("~[DLTX] WARNING: Section '%s' has parent '%s' that is defined as Override. Creating parent for backwards compatibility. Check this file and its DLTX mods: %s, mod file %s",
                            SectionName.c_str(),
                            parent.c_str(),
                            m_file_name,
                            currentFileName
                        );
                    }
					BaseData[parent].Name = parent;
				}
				else
				{
                    if (print_dltx_warnings)
                    {
                        Msg("~[DLTX] WARNING: Section '%s' inherits from non-existent section '%s'. Creating fallback empty parent section. Check this file and its DLTX mods: %s, mod file %s",
                            SectionName.c_str(),
                            parent.c_str(),
                            m_file_name,
                            currentFileName
                        );
                    }
					BaseData[parent].Name = parent;
				}
			}
			Items ParentData = EvaluateSection(parent, Evaluations, currentFileName);
			ResolvedParents = MergeSections(ResolvedParents, ParentData, DeletedItems, false);
		}
	}

	Items ResolvedBaseAndMods = BaseData[SectionName].Data;
	auto o_it = OverrideData.find(SectionName);
	if (o_it != OverrideData.end())
	{
		ResolvedBaseAndMods = MergeSections(ResolvedBaseAndMods, o_it->second.Data, DeletedItems, true);
		OverrideData.erase(o_it);
	}
	Items CurrentResult = MergeSections(ResolvedParents, ResolvedBaseAndMods, DeletedItems, false);

	// Optimized list splitting and joining with reduced allocations
	static auto split_list = [](shared_str items, char delimiter = ',')
	{
		xr_vector<std::string_view> vec;
		vec.reserve(16);  // Pre-allocate for typical list sizes

		const char* str = items.c_str();
		if (!str || *str == '\0') return vec;

		const char* start = str;
		const char* current = str;

		while (true)
		{
			if (*current == delimiter || *current == '\0')
			{
				// Calculate range
				const char* t_start = start;
				const char* t_end = current;

				// Trim leading whitespace
				while (t_start < t_end && isspace((unsigned char)*t_start)) t_start++;
				// Trim trailing whitespace
				while (t_end > t_start && isspace((unsigned char)*(t_end - 1))) t_end--;

				// Only add if not empty (handles ", ,")
				if (t_start < t_end)
				{
					vec.emplace_back(t_start, t_end - t_start);
				}

				if (*current == '\0') break;
				start = current + 1;
			}
			current++;
		}

		return vec;
	};

	// Store result back - optimized join with pre-calculated capacity
	static auto join_list = [](const xr_vector<std::string_view>& items_vec, char delimiter = ',')
	{
		if (items_vec.empty())
			return xr_string();

		// 1. Calculate exact size (avoiding the +delimiter on the last element)
		size_t total_size = 0;
		for (const auto& i : items_vec)
			total_size += i.length();

		total_size += (items_vec.size() - 1); // space for delimiters

		// 2. Build the string
		xr_string ret;
		ret.reserve(total_size);

		for (size_t idx = 0; idx < items_vec.size(); ++idx)
		{
			if (idx > 0)
				ret.push_back(delimiter);

			ret.append(items_vec[idx]);
		}
		return ret;
	};

	// Process list modifications
	if (OverrideModifyListData.find(SectionName) != OverrideModifyListData.end())
	{
		// 1. Pre-sort the modifications by key (ignoring the > / < prefix for the sort) and insertionIndex
		// This allows us to walk through CurrentSect.Data and OverrideModifyListData simultaneously.
		auto& overrideData = OverrideModifyListData[SectionName];
		if (!overrideData.empty())
		{
			std::sort(overrideData.begin(), overrideData.end(), [](const Item& a, const Item& b)
			{
				int res = xr_strcmp((*a.first) + 1, (*b.first) + 1);
				if (res != 0) return res < 0;

				// Preserve original file and line order
				return a.insertionIndex < b.insertionIndex;
			});

			Items result;
			result.reserve(CurrentResult.size() + overrideData.size());

			auto data_it = CurrentResult.begin();
			auto mod_it = overrideData.begin();

			while (data_it != CurrentResult.end() || mod_it != overrideData.end())
			{
				// If we have a modification, check if it's valid (has a value)
				if (mod_it != overrideData.end()) {
					if (mod_it->second == NULL) {
						mod_it++; // Skip modification with empty value
						continue;
					}
				}

				// Find the current "Active Key" to process
				shared_str active_key;
				if (mod_it != overrideData.end() && data_it != CurrentResult.end())
				{
					shared_str mod_key = ((*mod_it->first) + 1);
					int cmp = xr_strcmp(data_it->first, mod_key);
					active_key = (cmp <= 0) ? data_it->first : mod_key;
				}
				else if (mod_it != overrideData.end())
					active_key = ((*mod_it->first) + 1);
				else
					active_key = data_it->first;

				// 2. Identify if we have an existing item and any mods for this key
				Item* existing = (data_it != CurrentResult.end() && data_it->first == active_key) ? &(*data_it) : nullptr;

				// 3. Process all mods for this specific key in a sub-loop
				if (mod_it != overrideData.end() && xr_strcmp((*mod_it->first) + 1, active_key) == 0)
				{
					// Check if this is a new entry or a modification
					Item working_item;
					bool exists_in_output = false;

					if (existing)
					{
						working_item = std::move(*existing);
						exists_in_output = true;
						data_it++;
					}
					else
					{
						// Check deletedItems block for brand new keys
						if (DeletedItems.find(active_key) == DeletedItems.end())
						{
							working_item.first = active_key;
							working_item.second = ""; // Start empty
							exists_in_output = true;
						}
					}

					// Apply all mods for this key (e.g., <item, then >newitem1, then >newitem3)
					while (mod_it != overrideData.end() && xr_strcmp((*mod_it->first) + 1, active_key) == 0)
					{
						if (exists_in_output && mod_it->second != NULL)
						{
							ModifyListType op = static_cast<ModifyListType>((*mod_it->first)[0]);

							auto sect_it_items_vec = split_list(working_item.second);
							auto I_items_vec = split_list(mod_it->second);

							if (op == ModifyListType::Insert)
							{
								for (const auto& item : I_items_vec)
								{
									sect_it_items_vec.push_back(item);
								}
							}
							else if (op == ModifyListType::Remove)
							{
								sect_it_items_vec.erase(std::remove_if(sect_it_items_vec.begin(), sect_it_items_vec.end(), [&I_items_vec](std::string_view existing_item)
								{
									// Check if the existing item is in our "to remove" list
									return std::find(I_items_vec.begin(), I_items_vec.end(), existing_item) != I_items_vec.end();
								}), sect_it_items_vec.end());
							}

							working_item.second = join_list(sect_it_items_vec).c_str();
							working_item.filename = mod_it->filename;
						}
						mod_it++;
					}

					if (exists_in_output && working_item.second.size())
					{
						result.push_back(std::move(working_item));
					}
				}
				else
				{
					// No mods for this key, just move the data item if it exists
					if (existing)
					{
						result.push_back(std::move(*data_it));
						data_it++;
					}
				}
			}

			CurrentResult.swap(result);
		}
	}

	Evaluations.RecursionStack.pop_back();
	CurrentResult.shrink_to_fit();
	return Evaluations.ResolvedCache[SectionName] = std::move(CurrentResult);
};

// demonized: print DLTX override info
void CInifile::DLTX_print(LPCSTR sec, LPCSTR line)
{
	Msg("%s", m_file_name);
	if (!sec) {
		for (const auto& d : DATA) {
			Msg("[%s]", d.Name.c_str());
			for (const auto& s : d.Data) {
				printIniItemLine(s);
			}
		}
		return;
	}

	if (!section_exist(sec)) {
		Msg("![DLTX_print] no section exists by name %s", sec);
		return;
	}

	Sect& I = r_section(sec);

	if (!line) {
		Msg("[%s]", I.Name.c_str());
		for (const auto& s : I.Data) {
			printIniItemLine(s);
		}
		return;
	}

	if (!line_exist(sec, line)) {
		Msg("![DLTX_print] no line %s exists in section %s", line, sec);
		return;
	}

	auto A = std::lower_bound(I.Data.begin(), I.Data.end(), line, item_comparator());
	if (A != I.Data.end() && xr_strcmp(*A->first, line) == 0)
	{
		Msg("[%s]", I.Name.c_str());
		printIniItemLine(*A);
	}

}
LPCSTR CInifile::DLTX_getFilenameOfLine(LPCSTR sec, LPCSTR line)
{
	if (!sec) {
		Msg("![DLTX_getFilenameOfLine] no section provided");
		return nullptr;
	}

	if (!line) {
		Msg("![DLTX_getFilenameOfLine] no line provided for section %s", sec);
		return nullptr;
	}

	if (!section_exist(sec)) {
		Msg("![DLTX_getFilenameOfLine] no section exists by name %s", sec);
		return nullptr;
	}

	if (!line_exist(sec, line)) {
		Msg("![DLTX_getFilenameOfLine] no line %s exists in section %s", line, sec);
		return nullptr;
	}

	Sect& I = r_section(sec);
	auto A = std::lower_bound(I.Data.begin(), I.Data.end(), line, item_comparator());
	if (A != I.Data.end() && xr_strcmp(*A->first, line) == 0)
	{
		auto fname = A->filename.c_str();
		return fname;
	}
	return nullptr;
}
bool CInifile::DLTX_isOverride(LPCSTR sec, LPCSTR line)
{
	auto fname = DLTX_getFilenameOfLine(sec, line);
	if (!fname) {
		return false;
	}
	return xr_string(fname).find("mod_") == 0;
}

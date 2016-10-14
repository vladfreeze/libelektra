/**
 * @file
 *
 * @brief Source for simplespeclang plugin
 *
 * @copyright BSD License (see doc/COPYING or http://www.libelektra.org)
 *
 */

#include "simplespeclang.hpp"

using namespace ckdb;

#include <kdbease.h>
#include <kdberrors.h>

#include <kdbmeta.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>


namespace
{

std::string getConfigEnum (Plugin * handle)
{
	Key * k = ksLookupByName (elektraPluginGetConfig (handle), "/keyword/enum", 0);
	if (!k) return "enum";

	return keyString (k);
}

std::string getConfigAssign (Plugin * handle)
{
	Key * k = ksLookupByName (elektraPluginGetConfig (handle), "/keyword/assign", 0);
	if (!k) return "=";

	return keyString (k);
}

int serialise (std::ostream & os, ckdb::Key * parentKey, ckdb::KeySet * ks, Plugin * handle)
{
	ckdb::Key * cur;

	ksRewind (ks);
	while ((cur = ksNext (ks)) != nullptr)
	{
		const ckdb::Key * meta = ckdb::keyGetMeta (cur, "check/enum");
		if (!meta) continue;

		os << getConfigEnum (handle) << " ";
		os << elektraKeyGetRelativeName (cur, parentKey) << " ";
		os << getConfigAssign (handle);
		while ((meta = keyNextMeta (cur)))
		{
			os << " " << ckdb::keyString (meta);
		}
		os << "\n";
	}

	return 1;
}

int unserialise (std::istream & is, ckdb::Key * errorKey, ckdb::KeySet * ks, Plugin * handle)
{
	Key * cur = nullptr;

	Key * parent = keyNew (keyName (errorKey), KEY_END);

	std::string line;
	while (std::getline (is, line))
	{
		std::string read;
		std::stringstream ss (line);
		ss >> read;
		if (read == "mountpoint")
		{
			ss >> read;
			keySetMeta (parent, "mountpoint", read.c_str ());
			continue;
		}
		else if (read == "plugins")
		{
			std::string plugins;
			while (ss >> read)
			{
				// replace (read.begin(), read.end(), '_', ' ');
				plugins += read + " ";
			}
			keySetMeta (parent, "infos/plugins", plugins.c_str ());
			continue;
		}
		else if (read != getConfigEnum (handle))
		{
			ELEKTRA_LOG ("not an enum %s", read.c_str ());
			continue;
		}

		ss >> read;
		ELEKTRA_LOG ("key for enum is %s", read.c_str ());
		cur = keyNew (keyName (errorKey), KEY_END);
		keyAddName (cur, read.c_str ());

		ss >> read;
		if (read != getConfigAssign (handle))
		{
			ELEKTRA_SET_ERRORF (ELEKTRA_ERROR_PARSE, errorKey, "Expected assignment (%s), but got (%s)",
					    getConfigAssign (handle).c_str (), read.c_str ());
			continue;
		}

		std::string enums;
		while (ss >> read)
		{
			elektraMetaArrayAdd (cur, "check/enum", read.c_str ());
		}

		if (!enums.empty ())
		{
			keySetMeta (cur, "check/enum", "#");
		}

		keySetMeta (cur, "required", "yes"); // TODO bug: required key removed by spec?
		keySetMeta (cur, "mandatory", "yes");

		ckdb::ksAppendKey (ks, cur);
	}

	ckdb::ksAppendKey (ks, parent);

	return 1;
}
}

extern "C" {

int elektraSimplespeclangGet (Plugin * handle, KeySet * returned ELEKTRA_UNUSED, Key * parentKey ELEKTRA_UNUSED)
{
	if (!elektraStrCmp (keyName (parentKey), "system/elektra/modules/simplespeclang"))
	{
		KeySet * contract =
			ksNew (30, keyNew ("system/elektra/modules/simplespeclang", KEY_VALUE,
					   "simplespeclang plugin waits for your orders", KEY_END),
			       keyNew ("system/elektra/modules/simplespeclang/exports", KEY_END),
			       keyNew ("system/elektra/modules/simplespeclang/exports/get", KEY_FUNC, elektraSimplespeclangGet, KEY_END),
			       keyNew ("system/elektra/modules/simplespeclang/exports/set", KEY_FUNC, elektraSimplespeclangSet, KEY_END),
			       keyNew ("system/elektra/modules/simplespeclang/exports/checkconf", KEY_FUNC,
				       elektraSimplespeclangCheckConfig, KEY_END),
#include ELEKTRA_README (simplespeclang)
			       keyNew ("system/elektra/modules/simplespeclang/infos/version", KEY_VALUE, PLUGINVERSION, KEY_END), KS_END);
		ksAppend (returned, contract);
		ksDel (contract);

		return 1; // success
	}

	std::ifstream ofs (keyString (parentKey), std::ios::binary);
	if (!ofs.is_open ())
	{
		ELEKTRA_SET_ERROR_GET (parentKey);
		return -1;
	}

	return unserialise (ofs, parentKey, returned, handle);
}

int elektraSimplespeclangSet (Plugin * handle ELEKTRA_UNUSED, KeySet * returned ELEKTRA_UNUSED, Key * parentKey ELEKTRA_UNUSED)
{
	// ELEKTRA_LOG (ELEKTRA_LOG_MODULE_DUMP, "opening file %s", keyString (parentKey));
	std::ofstream ofs (keyString (parentKey), std::ios::binary);
	if (!ofs.is_open ())
	{
		ELEKTRA_SET_ERROR_SET (parentKey);
		return -1;
	}

	return serialise (ofs, parentKey, returned, handle);
}

int elektraSimplespeclangCheckConfig (Key * errorKey ELEKTRA_UNUSED, KeySet * conf ELEKTRA_UNUSED)
{
	return 0;
}

Plugin * ELEKTRA_PLUGIN_EXPORT (simplespeclang)
{
	// clang-format off
	return elektraPluginExport ("simplespeclang",
		ELEKTRA_PLUGIN_GET,	&elektraSimplespeclangGet,
		ELEKTRA_PLUGIN_SET,	&elektraSimplespeclangSet,
		ELEKTRA_PLUGIN_END);
}

}

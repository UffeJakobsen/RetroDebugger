#ifndef _CGT2Favorites_H_
#define _CGT2Favorites_H_

#include <vector>
#include <string>

extern "C" {
#include "ginstrops.h"
}

struct GT2FavoriteEntry
{
	std::string displayName;
	INSTRPACKAGE package;
};

class CGT2Favorites
{
public:
	CGT2Favorites();

	std::vector<GT2FavoriteEntry> entries;

	void Load();                          // read favorites.hjson
	void Save();                          // write favorites.hjson
	void AddFromInstrument(int einum);    // snapshot ginstr[einum] -> new entry
	void RemoveAt(int index);
	void ApplyTo(int einum, int index);   // load entry into the instrument

private:
	std::string FilePath() const;         // gPathToSettings/gt2/favorites.hjson
};

#endif

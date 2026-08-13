#pragma once

#include "lv_types.h"

void LvPersistLoad();
void LvPersistSave(int force);
CharSnap* LvPersistFind(const char* name, int create);
void LvMarkDirty();

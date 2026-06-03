#include "C64UModeManager.h"

#include <algorithm>

C64UModeManager::C64UModeManager()
	: mode(static_cast<int>(C64U_MODE_DISCONNECTED))
{
}

EC64UMode C64UModeManager::GetMode() const
{
	return static_cast<EC64UMode>(mode.load());
}

void C64UModeManager::SetMode(EC64UMode newMode)
{
	mode.store(static_cast<int>(newMode));
}

int C64UModeManager::ResolveAutoMode()
{
	std::lock_guard<std::mutex> lock(managerMutex);

	// Check which trace views are visible in traceViewOpenOrder
	// Classify each into: needs6510, needsVIC, needs1541
	// For now, default to mode 1 (6510 Only) when no trace views are open
	// or when we cannot determine the best mode.

	if (traceViewOpenOrder.empty())
	{
		return 1;  // 6510 Only
	}

	bool needs6510 = false;
	bool needsVIC = false;
	bool needs1541 = false;

	// For now, all trace views need 6510 as baseline
	// Future: classify views based on their type
	for (auto *view : traceViewOpenOrder)
	{
		if (view != nullptr)
		{
			needs6510 = true;
		}
	}

	if (needs6510 && needsVIC && needs1541)
	{
		// Conflict: newest view wins, oldest conflicting view goes into pausedViews
		// For now, pick 6510+VIC (mode 3) as the compromise
		return 3;
	}
	else if (needs6510 && needsVIC)
	{
		return 3;  // 6510+VIC
	}
	else if (needs6510 && needs1541)
	{
		return 5;  // 6510+1541
	}
	else if (needsVIC)
	{
		return 2;  // VIC Only
	}
	else if (needs1541)
	{
		return 4;  // 1541 Only
	}
	else if (needs6510)
	{
		return 1;  // 6510 Only
	}

	return 1;  // Default: 6510 Only
}

void C64UModeManager::TraceViewOpened(CGuiView *view)
{
	if (view == nullptr)
		return;

	std::lock_guard<std::mutex> lock(managerMutex);

	// Remove if already present (re-opened)
	auto it = std::find(traceViewOpenOrder.begin(), traceViewOpenOrder.end(), view);
	if (it != traceViewOpenOrder.end())
	{
		traceViewOpenOrder.erase(it);
	}

	traceViewOpenOrder.push_back(view);
	pausedViews.erase(view);
}

void C64UModeManager::TraceViewClosed(CGuiView *view)
{
	if (view == nullptr)
		return;

	std::lock_guard<std::mutex> lock(managerMutex);

	auto it = std::find(traceViewOpenOrder.begin(), traceViewOpenOrder.end(), view);
	if (it != traceViewOpenOrder.end())
	{
		traceViewOpenOrder.erase(it);
	}
	pausedViews.erase(view);
}

bool C64UModeManager::IsViewPausedByConflict(CGuiView *view) const
{
	std::lock_guard<std::mutex> lock(managerMutex);
	return pausedViews.find(view) != pausedViews.end();
}

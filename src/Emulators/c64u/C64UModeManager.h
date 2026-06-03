#ifndef _C64UMODEMANAGER_H_
#define _C64UMODEMANAGER_H_

#include "C64UMode.h"

#include <atomic>
#include <mutex>
#include <set>
#include <vector>

class CGuiView;

class C64UModeManager
{
public:
	C64UModeManager();

	EC64UMode GetMode() const;
	void SetMode(EC64UMode mode);

	// Trace mode auto-resolution
	int ResolveAutoMode();  // determines mode from open trace views
	void TraceViewOpened(CGuiView *view);
	void TraceViewClosed(CGuiView *view);
	bool IsViewPausedByConflict(CGuiView *view) const;

	// View tracking
	std::vector<CGuiView *> traceViewOpenOrder;
	std::set<CGuiView *> pausedViews;

private:
	std::atomic<int> mode;
	mutable std::mutex managerMutex;
};

#endif

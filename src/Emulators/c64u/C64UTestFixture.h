#ifndef _C64UTESTFIXTURE_H_
#define _C64UTESTFIXTURE_H_

class C64UTestFixture
{
public:
	static inline void SetEnabled(bool enabled)
	{
		isEnabled = enabled;
	}

	static inline bool IsEnabled()
	{
		return isEnabled;
	}

private:
	static inline bool isEnabled = false;
};

#endif

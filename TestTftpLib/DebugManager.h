#pragma once
#include <memory>
#include <functional>
#include <string>

class DebugManager
{

public:
	static DebugManager& Init();

	static DebugManager& Instance();

	bool IsDebug() const;
	void OnDebug(const std::function<void()> &fn) const;
	void Throw(const std::string &message) const;
	void ThrowIf(bool cond, const std::string& message) const;

private:
	DebugManager();

	class DebugManagerImpl;
	std::unique_ptr<DebugManagerImpl> impl;
};


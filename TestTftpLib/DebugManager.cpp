#include "DebugManager.h"

#include <iostream>
#include <Windows.h>
#include <functional>

class NoopDebugDelegate {
public:
	void Init() {};
	void Uninit() {};
	bool IsDebug() const { return false; }
	void OnDebug(const std::function<void()>& fn) const {}
	void Throw(const std::string& message) const {}
	void ThrowIf(bool cond, const std::string& message) const {}
};

class ActuallyDebuggingDebugDelegate {
public:
	void Init();
	void Uninit();
	bool IsDebug() const { return true; }
	void OnDebug(const std::function<void()>& fn) const { fn(); }
	void Throw(const std::string& message) const { throw std::runtime_error{ message }; };
	void ThrowIf(bool cond, const std::string& message) const;

private:
	HANDLE backupSTDIN;
	HANDLE backupSTDERR;
	HANDLE backupSTDOUT;
};

#ifdef DEBUG
using DebugDelegate = ActuallyDebuggingDebugDelegate;
#else
using DebugDelegate = NoopDebugDelegate;
#endif

class DebugManager::DebugManagerImpl{

private:
	DebugDelegate delegate;

public:
	DebugManagerImpl();
	~DebugManagerImpl();

	bool IsDebug() const;
	void OnDebug(const std::function<void()>& fn) const;
	void Throw(const std::string& message) const;
	void ThrowIf(bool cond, const std::string& message) const;
};


std::unique_ptr<DebugManager> DebugManagerInstance;

DebugManager::DebugManager() 
	: impl{ new DebugManagerImpl() } {
}

DebugManager& DebugManager::Init() {
	if (DebugManagerInstance != nullptr ) {
		if (DebugManagerInstance->IsDebug()) {
			throw std::runtime_error{ "Attempting to init DebugManager twice" };
		}
	}
	DebugManagerInstance.reset(new DebugManager());
	return *DebugManagerInstance;
}

DebugManager& DebugManager::Instance() {
	if (DebugManagerInstance == nullptr) {
		throw std::runtime_error{ "DebugManager must be initialized before instance access" };
	}

	return *DebugManagerInstance;
}

bool DebugManager::IsDebug() const {
	return impl->IsDebug();
}

void DebugManager::OnDebug(const std::function<void()> &fn) const
{
	return impl->OnDebug(fn);
}

void DebugManager::Throw(const std::string& message) const
{
	return impl->Throw(message);
}

void DebugManager::ThrowIf(bool cond, const std::string& message) const
{
	return impl->ThrowIf(cond ,message);
}

////////////// actual delegate

void ActuallyDebuggingDebugDelegate::Init() {
	
	backupSTDIN = GetStdHandle(STD_INPUT_HANDLE);
	backupSTDERR = GetStdHandle(STD_ERROR_HANDLE);
	backupSTDOUT = GetStdHandle(STD_OUTPUT_HANDLE);

	FILE* seriously = nullptr;

	AllocConsole();
	(void)freopen_s(&seriously, "conin$", "r", stdin);
	(void)freopen_s(&seriously, "conout$", "w", stdout);
	(void)freopen_s(&seriously, "conout$", "w", stderr);
}

void ActuallyDebuggingDebugDelegate::Uninit() {
	SetStdHandle(STD_INPUT_HANDLE, backupSTDIN);
	SetStdHandle(STD_ERROR_HANDLE, backupSTDERR);
	SetStdHandle(STD_OUTPUT_HANDLE, backupSTDOUT);
	
	FreeConsole();
}

void ActuallyDebuggingDebugDelegate::ThrowIf(bool cond, const std::string& message) const { 
	if (cond) {
		throw std::runtime_error{ message }; 
	}
};

////////////// pimpl

DebugManager::DebugManagerImpl::DebugManagerImpl() {
	delegate.Init();
}

DebugManager::DebugManagerImpl::~DebugManagerImpl() {
	delegate.Uninit();
}

bool DebugManager::DebugManagerImpl::IsDebug() const{
	return delegate.IsDebug();
}

void DebugManager::DebugManagerImpl::OnDebug(const std::function<void()>& fn) const {
	delegate.OnDebug(fn);
}

void DebugManager::DebugManagerImpl::Throw(const std::string& message) const
{
	delegate.Throw(message);
}

void DebugManager::DebugManagerImpl::ThrowIf(bool cond, const std::string& message) const
{
	delegate.ThrowIf(cond, message);
}

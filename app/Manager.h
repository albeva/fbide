/**
 * FBIde project
 */
#pragma once

#include "app_pch.cpp"


/**
 * Manager is gateway to the SDK.
 *
 * It provides connecting point for various
 * other services (managers)
 */
class Manager : private NonCopyable
{
public:
    
    // Get manager instance
    static Manager & Get();
    
    // clean up
    static void Release();
    
private:
    
    Manager();
    ~Manager();

};

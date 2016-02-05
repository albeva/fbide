/**
 * FBIde project
 */
#pragma once

/**
 * Disallow copying this class
 */
class NonCopyable
{
protected:
    NonCopyable() {};
    ~NonCopyable() {};
    
private:
    NonCopyable(const NonCopyable &) = delete;
    NonCopyable & operator = (const NonCopyable &) = delete;
};

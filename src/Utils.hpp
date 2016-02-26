/**
 * FBIde project
 */
#pragma once

namespace fbide {
    
    /**
     * is_one_of checks if type T is one of the given types
     */
    template<typename T>
    constexpr bool is_one_of()
    {
        return false;
    }
    
    template<typename T, typename U, typename... R>
    constexpr bool is_one_of()
    {
        return std::is_same<std::decay_t<T>, U>::value || is_one_of<T, R...>();
    }

    
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
    
}
